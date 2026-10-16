"""Run the production C decoders against manual examples and corrupt frames."""
import ctypes
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SensorCodec(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("clang")
        if not compiler:
            raise unittest.SkipTest("A host C compiler is required for sensor decoder tests")
        library = Path(cls.temp.name) / ("sensor_codec.dll" if os.name == "nt" else "sensor_codec.so")
        flags = ["-shared", "-Wall", "-Wextra", "-Werror"]
        command = [compiler]
        if Path(compiler).stem == "zig":
            command += ["cc"]
        if os.name != "nt":
            flags += ["-fPIC"]
        elif Path(compiler).stem in ("zig", "gcc", "cc"):
            flags += ["-Wl,--export-all-symbols"]
        else:
            flags += ["--target=x86_64-pc-windows-msvc", "-ffreestanding", "-nostdlib", "-fuse-ld=lld",
                      "-Wl,/noentry", "-Wl,/export:ze03_decode", "-Wl,/export:zs03_decode"]
        cls.compiler_command = command
        result = subprocess.run([*command, *flags, str(ROOT / "common/sensor_codec.c"), "-o", str(library)],
                                capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        cls.lib = ctypes.CDLL(str(library))
        byte_ptr = ctypes.POINTER(ctypes.c_uint8)
        cls.lib.ze03_decode.argtypes = [byte_ptr, ctypes.c_size_t, byte_ptr, ctypes.POINTER(ctypes.c_uint16)]
        cls.lib.zs03_decode.argtypes = [byte_ptr, ctypes.POINTER(ctypes.c_int16), ctypes.POINTER(ctypes.c_uint16)]
        if os.name == "nt":
            # Release DLL before TemporaryDirectory cleanup on Windows.
            import _ctypes
            cls.addClassCleanup(lambda: _ctypes.FreeLibrary(cls.lib._handle))

    def gas(self, frame):
        gas, value = ctypes.c_uint8(99), ctypes.c_uint16(99)
        data = (ctypes.c_uint8 * len(frame))(*frame)
        ok = self.lib.ze03_decode(data, len(frame), ctypes.byref(gas), ctypes.byref(value))
        return ok, gas.value, value.value

    @staticmethod
    def frame(gas, value, decimals=0):
        frame = [255, 134, value >> 8, value & 255, gas, decimals, 0, 0]
        return frame + [(-sum(frame[1:])) & 255]

    def test_ze03_h2s_and_nh3_scaling(self):
        self.assertEqual(self.gas(self.frame(3, 42)), (1, 3, 4200))
        self.assertEqual(self.gas(self.frame(2, 123, 1)), (1, 2, 1230))
        self.assertEqual(self.gas(self.frame(2, 123, 2)), (1, 2, 123))

    def test_ze03_rejects_corruption_and_wrong_gas(self):
        frame = self.frame(3, 42)
        frame[-1] ^= 1
        for bad in [frame, self.frame(5, 209, 1), self.frame(3, 42, 3),
                    self.frame(3, 65535), self.frame(3, 42)[:-1]]:
            self.assertEqual(self.gas(bad), (0, 99, 99))

    def zs03(self, frame):
        temperature, humidity = ctypes.c_int16(999), ctypes.c_uint16(999)
        data = (ctypes.c_uint8 * 5)(*frame)
        ok = self.lib.zs03_decode(data, ctypes.byref(temperature), ctypes.byref(humidity))
        return ok, temperature.value, humidity.value

    def test_zs03_manual_positive_and_negative_examples(self):
        self.assertEqual(self.zs03([55, 0, 18, 0, 73]), (1, 180, 550))
        self.assertEqual(self.zs03([55, 0, 18, 128, 201]), (1, -180, 550))

    def test_zs03_manual_corrupt_example_and_invalid_values(self):
        for bad in [[41, 0, 6, 0, 68], [55, 0, 61, 0, 116],
                    [55, 1, 18, 0, 74], [99, 0, 18, 0, 117]]:
            self.assertEqual(self.zs03(bad), (0, 999, 999))

    def test_real_node_legacy_and_digital_registers(self):
        for digital in (False, True):
            name = "digital" if digital else "legacy"
            executable = Path(self.temp.name) / (name + (".exe" if os.name == "nt" else ""))
            includes = ["esp32c3-node/main", "stm32/include"] if digital else ["stm32/include"]
            args = [*self.compiler_command, "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                    "-DNODE_SENSOR_TYPE=2", "-I" + str(ROOT / "common")]
            if digital:
                args += ["-DNODE_DIGITAL_SENSORS=1"]
            args += ["-I" + str(ROOT / path) for path in includes]
            args += [str(ROOT / path) for path in
                     ["tests/node_address_test.c", "stm32/src/node.c", "common/protocol.c"]]
            result = subprocess.run([*args, "-o", str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_software_uart_waveforms(self):
        executable = Path(self.temp.name) / ("soft_uart.exe" if os.name == "nt" else "soft_uart")
        args = [*self.compiler_command, "-std=gnu11", "-O2", "-Wall", "-Wextra", "-Werror",
                "-I" + str(ROOT / "common"), str(ROOT / "tests/soft_uart_test.c"),
                str(ROOT / "common/soft_uart.c"), "-o", str(executable)]
        result = subprocess.run(args, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        result = subprocess.run([str(executable)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()

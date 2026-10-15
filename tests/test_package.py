import importlib.util
import unittest
import zlib
import struct
from pathlib import Path
spec=importlib.util.spec_from_file_location('package',Path(__file__).resolve().parents[1]/'tools/package_firmware.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class Packaging(unittest.TestCase):
    def test_cross_format(self):
        data=struct.pack('<II',0x20005000,0x08004009)+b'firmware'
        command=module.package(data,'stm32',2,'https://example.com/fw.bin','01'*12)
        self.assertEqual(command['crc32'],zlib.crc32(data))
        self.assertNotIn('hmac',command)
    def test_reject_boot_image(self):
        data=struct.pack('<II',0x20005000,0x08000009)+b'firmware'
        with self.assertRaises(ValueError):module.package(data,'stm32',2,'https://example.com/fw.bin','01'*12)
    def test_size_and_transport(self):
        for data,url in [(b'123','https://example.com/f'),(bytes(46*1024+1),'https://example.com/f'),(b'12345678','http://example.com/f')]:
            with self.assertRaises(ValueError):module.package(data,'stm32',2,url,'01'*12)

if __name__=='__main__':unittest.main()

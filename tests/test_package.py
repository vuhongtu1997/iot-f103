import hashlib
import hmac
import importlib.util
import struct
import unittest
import zlib
from pathlib import Path
spec=importlib.util.spec_from_file_location('package',Path(__file__).resolve().parents[1]/'tools/package_firmware.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class Packaging(unittest.TestCase):
    def test_cross_format(self):
        data=struct.pack('<II',0x20005000,0x08004009)+b'firmware';key=bytes(range(32))
        command=module.package(data,key,'stm32',2,'https://example.com/fw.bin','01'*12)
        descriptor=struct.pack('<IIII',0x103c8,len(data),zlib.crc32(data),2)
        self.assertEqual(command['hmac'],hmac.new(key,descriptor+data,hashlib.sha256).hexdigest())
    def test_reject_boot_image(self):
        data=struct.pack('<II',0x20005000,0x08000009)+b'firmware'
        with self.assertRaises(ValueError):module.package(data,bytes(32),'stm32',2,'https://example.com/fw.bin','01'*12)
    def test_size_and_transport(self):
        for data,url in [(b'123','https://example.com/f'),(bytes(46*1024+1),'https://example.com/f'),(b'12345678','http://example.com/f')]:
            with self.assertRaises(ValueError):module.package(data,bytes(32),'stm32',2,url,'01'*12)

if __name__=='__main__':unittest.main()

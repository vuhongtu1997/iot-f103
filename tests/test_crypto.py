"""Cross-check firmware C crypto against Python standard-library crypto."""
import ctypes
import hashlib
import hmac
import subprocess
import tempfile
import unittest
from pathlib import Path

class Crypto(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory();tmp=Path(cls.tmp.name)
        common=Path(__file__).resolve().parents[1]/'common'
        wrapper=tmp/'wrapper.c'
        wrapper.write_text('#include "sha256.h"\nvoid calculate(const unsigned char *k, size_t kn,const unsigned char *p,size_t n,unsigned char *out){hmac_ctx c;hmac_init(&c,k,kn);hmac_update(&c,p,n);hmac_final(&c,out);}\n')
        lib=tmp/'crypto.so'
        subprocess.run(['gcc','-shared','-fPIC','-O2','-I'+str(common),str(wrapper),str(common/'sha256.c'),'-o',str(lib)],check=True)
        cls.lib=ctypes.CDLL(str(lib));cls.lib.calculate.argtypes=[ctypes.c_char_p,ctypes.c_size_t,ctypes.c_char_p,ctypes.c_size_t,ctypes.c_void_p]
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()
    def test_hmac_boundaries(self):
        for keylen in (32,100):
            key=bytes(i%256 for i in range(keylen))
            for n in (0,1,55,56,63,64,65,128,777,47104):
                data=bytes((i*7)%256 for i in range(n));out=ctypes.create_string_buffer(32)
                self.lib.calculate(key,len(key),data,len(data),out)
                self.assertEqual(out.raw,hmac.new(key,data,hashlib.sha256).digest(),(keylen,n))

if __name__=='__main__':unittest.main()

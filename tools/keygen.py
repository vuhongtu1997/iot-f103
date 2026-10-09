#!/usr/bin/env python3
"""Generate shared HMAC key outside source control; refuse accidental overwrite."""
import argparse
import secrets
from pathlib import Path

def main():
    p=argparse.ArgumentParser();p.add_argument('--demo',action='store_true');args=p.parse_args()
    root=Path(__file__).resolve().parents[1]
    header=root/'common/ota_key.h';binary=root/'tools/ota-key.bin'
    if header.exists() or binary.exists():
        p.error('Key already exists. Back it up; do not silently rotate deployed devices.')
    key=bytes(range(32)) if args.demo else secrets.token_bytes(32)
    header.write_text('#ifndef IOT_OTA_KEY_H\n#define IOT_OTA_KEY_H\nstatic const unsigned char OTA_KEY[32]={'+','.join(f'0x{x:02x}' for x in key)+'};\n#endif\n')
    binary.write_bytes(key);header.chmod(0o600);binary.chmod(0o600)
    print('Generated common/ota_key.h and tools/ota-key.bin. Keep both private.')

if __name__=='__main__':main()

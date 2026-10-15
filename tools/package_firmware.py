#!/usr/bin/env python3
"""Validate firmware metadata and print a cloud OTA command."""
import argparse
import json
import struct
import uuid
import zlib
from pathlib import Path
from urllib.parse import urlsplit

def package(data, target, version, url, uid=None, job=None):
    if not 0<=version<=0xffffffff:raise ValueError('version must be uint32')
    if target not in ('esp32','stm32'):raise ValueError('unknown target')
    limit=0x180000 if target=='esp32' else 46*1024
    if not 8<=len(data)<=limit:raise ValueError('firmware size exceeds slot or is too small')
    u=urlsplit(url)
    if u.scheme!='https' or not u.hostname or u.username or u.password or len(url)>511:
        raise ValueError('use an HTTPS URL without embedded credentials')
    if target=='stm32':
        if uid is None or len(uid)!=24 or len(bytes.fromhex(uid))!=12:raise ValueError('STM32 needs 24 hex characters of UID')
        sp,pc=struct.unpack('<II',data[:8])
        if not (0x20000000<sp<=0x20005000 and sp%8==0 and pc&1 and 0x08004000<=pc&~1<0x08004000+len(data)):
            raise ValueError('invalid STM32 vectors; link APPLICATION at 0x08004000, not bootloader')
    crc=zlib.crc32(data)&0xffffffff
    result={'op':'ota','job_id':job or str(uuid.uuid4()),'target':target,'url':url,'size':len(data),'crc32':crc,'version':version,
            }
    if target=='stm32':result['uid']=uid.lower()
    return result

def main():
    p=argparse.ArgumentParser();p.add_argument('firmware',type=Path)
    p.add_argument('--target',required=True,choices=['esp32','stm32']);p.add_argument('--version',required=True,type=int);p.add_argument('--url',required=True);p.add_argument('--uid');p.add_argument('--job-id');p.add_argument('--output',type=Path)
    a=p.parse_args()
    try:command=package(a.firmware.read_bytes(),a.target,a.version,a.url,a.uid,a.job_id)
    except (OSError,ValueError) as e:p.error(str(e))
    text=json.dumps(command,ensure_ascii=False,indent=2)+'\n'
    if a.output:a.output.write_text(text)
    else:print(text,end='')

if __name__=='__main__':main()

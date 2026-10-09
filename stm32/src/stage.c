#include "board.h"
#include "node.h"
#include "sha256.h"
#include "ota_key.h"
#include <string.h>
static uint8_t header[48];static uint32_t received;static int uploading;
uint32_t stage_received(void){return received;}
static int sane(const uint8_t *h){return get32(h)==MODEL_STM32&&get32(h+4)>=8&&get32(h+4)<=APP_MAX;}
int stage_begin(const uint8_t h[48]){
 uploading=0;received=0;if(!sane(h)||!nor_init()||!nor_erase(0))return 0;
 uint32_t n=get32(h+4);for(uint32_t a=STAGE_BASE;a<STAGE_BASE+n;a+=4096)if(!nor_erase(a))return 0;
 memcpy(header,h,48);if(!nor_write(0,h,48))return 0;uploading=1;return 1;
}
int stage_data(uint32_t off,const uint8_t *p,size_t n){
 if(!uploading||!n||n>CHUNK_SIZE||off>get32(header+4)||n>get32(header+4)-off)return 0;
 uint8_t check[CHUNK_SIZE];
 if(off<received){return off+n<=received&&nor_read(STAGE_BASE+off,check,n)&&constant_equal(check,p,n);}
 if(off!=received||!nor_write(STAGE_BASE+off,p,n)||!nor_read(STAGE_BASE+off,check,n)||!constant_equal(check,p,n))return 0;
 received+=(uint32_t)n;return 1;
}
static int vectors_valid(const uint8_t *v,uint32_t len){uint32_t sp=get32(v),pc=get32(v+4);return sp>0x20000000u&&sp<=0x20005000u&&!(sp&7u)&&(pc&1u)&&(pc&~1u)>=APP_BASE&&(pc&~1u)<APP_BASE+len;}
static int verify_external(const uint8_t *h){
 if(!sane(h))return 0;uint8_t b[CHUNK_SIZE],mac[32];uint32_t len=get32(h+4),c=0xffffffff;
 if(!nor_read(STAGE_BASE,b,8)||!vectors_valid(b,len))return 0;
 hmac_ctx ctx;hmac_init(&ctx,OTA_KEY,32);hmac_update(&ctx,h,16);
 for(uint32_t off=0;off<len;){uint32_t n=len-off;if(n>sizeof b)n=sizeof b;if(!nor_read(STAGE_BASE+off,b,n))return 0;
 c=crc32_update(c,b,n);hmac_update(&ctx,b,n);off+=n;}hmac_final(&ctx,mac);
 return (c^0xffffffff)==get32(h+8)&&constant_equal(mac,h+16,32);
}
int stage_commit(void){
 if(!uploading||received!=get32(header+4)||!verify_external(header))return 0;
 uint8_t marker[4];put32(marker,READY_MAGIC);if(!nor_write(48,marker,4))return 0;uploading=0;return 1;
}
int stage_install(void){
 uint8_t h[STAGE_HEADER_SIZE];if(!nor_init()||!nor_read(0,h,sizeof h))return 0;
 if(get32(h+48)!=READY_MAGIC||get32(h+52)==0)return 0;
 if(!verify_external(h))return -1;
 /* Never erase bootloader or address pages. Pending header survives resets. */
 for(uint32_t a=APP_BASE;a<APP_LIMIT;a+=1024)if(!flash_erase(a))return -1;
 uint32_t len=get32(h+4);uint8_t b[CHUNK_SIZE+2];
 /* Write vectors LAST: if external flash disappears after a reset during copy,
  * the blank internal vector prevents booting a partially written application. */
 uint8_t vectors[8];if(!nor_read(STAGE_BASE,vectors,8))return -1;
 for(uint32_t off=8;off<len;){uint32_t n=len-off;if(n>CHUNK_SIZE)n=CHUNK_SIZE;if(!nor_read(STAGE_BASE+off,b,n))return -1;
 if(n&1)b[n]=0xff;if(!flash_write(APP_BASE+off,b,(n+1)&~1u))return -1;off+=n;}
 uint32_t c=crc32_update(0xffffffff,vectors,8);
 c=crc32_update(c,(const void *)(uintptr_t)(APP_BASE+8),len-8)^0xffffffff;
 if(c!=get32(h+8))return -1;
 if(!flash_write(APP_BASE,vectors,8))return -1;
 uint8_t done[4]={0};if(!nor_write(52,done,4))return -1;return 1;
}

#define _GNU_SOURCE
#include "node.h"
#include "board.h"
#include "sha256.h"
#include "ota_key.h"
#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t external[0x20000];static int fail_after=-1,nor_present=1;
static unsigned erased;
static int permit(void){if(fail_after==0)return 0;if(fail_after>0)fail_after--;return 1;}
int flash_erase(uint32_t a){if(!permit())return 0;assert(a>=APP_BASE&&a<0x08010000u);memset((void *)(uintptr_t)a,0xff,1024);erased++;return 1;}
int flash_write(uint32_t a,const void *p,size_t n){uint8_t *d=(void *)(uintptr_t)a;const uint8_t *s=p;assert(a>=APP_BASE&&a+n<=0x08010000u&&!(n&1));for(size_t i=0;i<n;i+=2){if(!permit())return 0;d[i]&=s[i];d[i+1]&=s[i+1];}return 1;}
int nor_init(void){return nor_present;}
int nor_read(uint32_t a,void *p,size_t n){if(!nor_present||a+n>sizeof external)return 0;memcpy(p,external+a,n);return 1;}
int nor_erase(uint32_t a){if(!nor_present||a+4096>sizeof external)return 0;memset(external+a,0xff,4096);return 1;}
int nor_write(uint32_t a,const void *p,size_t n){if(!nor_present||a+n>sizeof external)return 0;const uint8_t *s=p;for(size_t i=0;i<n;i++)external[a+i]&=s[i];return 1;}
static void fromhex(const char *s,uint8_t *p,size_t n){for(size_t i=0;i<n;i++){unsigned v;assert(sscanf(s+2*i,"%2x",&v)==1);p[i]=(uint8_t)v;}}
static void crypto(void){
 uint8_t out[32],expected[32];sha256_ctx s;sha256_init(&s);sha256_update(&s,"abc",3);sha256_final(&s,out);
 fromhex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",expected,32);assert(!memcmp(out,expected,32));
 uint8_t key[20];memset(key,0x0b,20);hmac_ctx h;hmac_init(&h,key,20);hmac_update(&h,"Hi There",8);hmac_final(&h,out);
 fromhex("b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",expected,32);assert(!memcmp(out,expected,32));
 assert(crc16("123456789",9)==0x4b37);assert((crc32_update(0xffffffff,"123456789",9)^0xffffffff)==0xcbf43926);
 for(unsigned bits=0;bits<=96;bits++){uint8_t a[12]={0},b[12]={0};assert(prefix_matches(a,b,bits));if(bits){b[(bits-1)/8]|=(uint8_t)(0x80u>>((bits-1)%8));assert(!prefix_matches(a,b,bits));}}
 assert(!prefix_matches((uint8_t[12]){0},(uint8_t[12]){0},97));
}
static void config_tests(void){
 assert(config_address()==0);assert(!config_set_address(0));assert(!config_set_address(248));assert(config_set_address(1));
 uint8_t before[2048];memcpy(before,(void *)(uintptr_t)APP_LIMIT,2048);
 /* Cut power after every individual halfword of an address update. */
 for(int cut=0;cut<9;cut++){memcpy((void *)(uintptr_t)APP_LIMIT,before,2048);fail_after=cut;config_set_address(2);fail_after=-1;uint8_t a=config_address();assert(a==1||a==2);}
 memcpy((void *)(uintptr_t)APP_LIMIT,before,2048);assert(config_set_address(2));unsigned old=erased;assert(config_set_address(2));assert(erased==old);
}
static uint8_t image[777],hdr[48];
static void prepare(void){
 for(unsigned i=0;i<sizeof image;i++)image[i]=(uint8_t)(i*7);put32(image,0x20005000);put32(image+4,APP_BASE+0x101);
 put32(hdr,MODEL_STM32);put32(hdr+4,sizeof image);put32(hdr+8,crc32_update(0xffffffff,image,sizeof image)^0xffffffff);put32(hdr+12,2);
 hmac_ctx h;hmac_init(&h,OTA_KEY,32);hmac_update(&h,hdr,16);hmac_update(&h,image,sizeof image);hmac_final(&h,hdr+16);
 assert(stage_begin(hdr));assert(!stage_data(1,image,10));assert(!stage_data(0,image,0));assert(!stage_commit());
 for(uint32_t off=0;off<sizeof image;){size_t n=sizeof image-off;if(n>CHUNK_SIZE)n=CHUNK_SIZE;assert(stage_data(off,image+off,n));assert(stage_data(off,image+off,n));off+=(uint32_t)n;}
 assert(stage_received()==sizeof image);
}
static void stage_tests(void){
 prepare();external[STAGE_BASE+30]^=1;unsigned old=erased;assert(!stage_commit());assert(erased==old);
 prepare();assert(stage_commit());uint8_t saved[sizeof external];memcpy(saved,external,sizeof saved);
 uint8_t cfg[2048];memcpy(cfg,(void *)(uintptr_t)APP_LIMIT,2048);
 /* Fail during erase and programming, then reboot and repeat installation. */
 int cuts[]={0,1,20,46,60,200,433,434,435,436,437};
 for(unsigned i=0;i<sizeof cuts/sizeof *cuts;i++){
  memcpy(external,saved,sizeof saved);memset((void *)(uintptr_t)APP_BASE,0xff,APP_MAX);fail_after=cuts[i];stage_install();fail_after=-1;
  assert(stage_install()>=0);assert(!memcmp((void *)(uintptr_t)APP_BASE,image,sizeof image));assert(!memcmp((void *)(uintptr_t)APP_LIMIT,cfg,2048));
 }
 memcpy(external,saved,sizeof saved);memset((void *)(uintptr_t)APP_BASE,0xff,APP_MAX);fail_after=60;assert(stage_install()==-1);fail_after=-1;
 nor_present=0;assert(stage_install()==0);assert(get32((void *)(uintptr_t)APP_BASE)==0xffffffff);nor_present=1;assert(stage_install()==1);
 /* Valid CRC does not authorize a forged HMAC. */
 prepare();external[16]^=1;assert(stage_commit());old=erased;assert(stage_install()==-1);assert(erased==old);
 puts("PASS: crypto vectors, prefix bounds, address power cuts, chunks, tampering, interrupted installation, vector-last safety");
}
int main(void){void *p=mmap((void *)0x08000000,65536,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);assert(p!=(void *)-1);memset(p,0xff,65536);memset(external,0xff,sizeof external);crypto();config_tests();stage_tests();return 0;}

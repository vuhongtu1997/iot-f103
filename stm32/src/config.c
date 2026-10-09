#include "board.h"
#include "node.h"
#include <string.h>
#define PAGE0 0x0800f800u
#define PAGE1 0x0800fc00u
#define MAGIC 0x43464731u
static int valid(const uint8_t *p){return get32(p)==MAGIC&&get32(p+8)>=1&&get32(p+8)<=MAX_NODES&&(crc32_update(0xffffffff,p+4,8)^0xffffffff)==get32(p+12);}
static const uint8_t *active(void){
 const uint8_t *a=(const uint8_t *)(uintptr_t)PAGE0,*b=(const uint8_t *)(uintptr_t)PAGE1;
 if(!valid(a))return valid(b)?b:0;if(!valid(b))return a;
 return (int32_t)(get32(b+4)-get32(a+4))>0?b:a;
}
uint8_t config_address(void){const uint8_t *p=active();return p?(uint8_t)get32(p+8):0;}
int config_set_address(uint8_t addr){
 if(!addr||addr>MAX_NODES)return 0;const uint8_t *p=active();if(p&&get32(p+8)==addr)return 1;
 uint32_t target=(p&&(uintptr_t)p==PAGE0)?PAGE1:PAGE0;uint8_t record[16];
 put32(record,MAGIC);put32(record+4,p?get32(p+4)+1:1);put32(record+8,addr);put32(record+12,crc32_update(0xffffffff,record+4,8)^0xffffffff);
 /* Old page is retained. New record becomes visible only after magic is written. */
 return flash_erase(target)&&flash_write(target+4,record+4,12)&&flash_write(target,record,4);
}

#include "node.h"
#include "board.h"
#include <setjmp.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static jmp_buf done;static uint8_t request[32],address;static int consumed,writes,replies;
uint8_t config_address(void){return address;}
int config_set_address(uint8_t value){writes++;address=value;return 1;}
void board_uid(uint8_t uid[12]){memset(uid,1,12);}
int bus_receive(uint8_t *out,size_t cap){(void)cap;if(consumed++)longjmp(done,1);memcpy(out,request,8);return 8;}
void bus_send(const uint8_t *p,size_t n){assert(crc16(p,n)==0);replies++;}
uint16_t sensor_raw(void){return 123;}
int stage_begin(const uint8_t p[48]){(void)p;return 0;}
int stage_data(uint32_t o,const uint8_t *p,size_t n){(void)o;(void)p;(void)n;return 0;}
uint32_t stage_received(void){return 0;}
int stage_commit(void){return 0;}
void delay_ms(uint32_t n){(void)n;}
void board_reset(void){assert(0);}
static void run(int mode){put16(request+6,crc16(request,6));consumed=0;if(!setjmp(done))node_run(mode);}
int main(void){
 uint8_t broadcast[6]={0,6,1,0,0,5};memcpy(request,broadcast,6);run(0);assert(address==5&&writes==1&&replies==0);
 request[5]=6;run(0);assert(address==5&&writes==1&&replies==0); /* configured node locked */
 address=0;request[5]=247;run(1);assert(address==247&&writes==2&&replies==0); /* recovery bootloader */
 address=0;request[5]=0;run(0);assert(address==0&&writes==2&&replies==0);
 request[5]=248;run(0);assert(address==0&&writes==2&&replies==0);
 request[5]=9;request[3]=1;run(0);assert(address==0&&writes==2&&replies==0); /* wrong register */
 request[3]=0;request[0]=1;run(0);assert(address==0&&writes==2&&replies==0); /* no unicast commissioning */
 request[0]=0;request[1]=3;run(0);assert(address==0&&replies==0); /* broadcast reads never answer */
 puts("PASS: real STM32 parser, unconfigured-only address write, bootloader support, invalid IDs, broadcast silence");return 0;
}

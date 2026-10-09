#include "node.h"
#include "board.h"
#include <string.h>
static void reply(uint8_t *b,size_t n){put16(b+n,crc16(b,n));bus_send(b,n+2);}
void node_run(int boot_mode){
 uint8_t uid[12],rx[FRAME_MAX],tx[FRAME_MAX];board_uid(uid);
 for(;;){int size=bus_receive(rx,sizeof rx);if(size<4||crc16(rx,(size_t)size)!=0)continue;
 size_t n=(size_t)size-2;uint8_t addr=config_address();int reboot=0;
 tx[0]=rx[0];tx[1]=rx[1];
 /* Broadcast FC06 is write-only. Already configured nodes remain locked.
  * Power ONLY ONE unconfigured node at a time; configured nodes may stay on. */
 if(rx[0]==0){
  if(!addr&&rx[1]==6&&n==6&&(((unsigned)rx[2]<<8)|rx[3])==ADDRESS_REGISTER&&rx[4]==0&&rx[5]>=1&&rx[5]<=MAX_NODES)
   config_set_address(rx[5]);
  continue; /* Never send ACK or exception to any broadcast. */
 }
 if(!addr||rx[0]!=addr)continue;
 if(rx[1]==3&&n==6){
  unsigned start=(unsigned)rx[2]<<8|rx[3],count=(unsigned)rx[4]<<8|rx[5];
  if(!count||start+count>12){tx[1]|=0x80;tx[2]=2;reply(tx,3);continue;}
  uint16_t raw=boot_mode?0xffff:sensor_raw(),regs[12];
  regs[0]=raw;regs[1]=raw==0xffff?0xffff:(uint16_t)((uint32_t)raw*3300/4095);
  regs[2]=(uint16_t)(FW_VERSION>>16);regs[3]=(uint16_t)FW_VERSION;regs[4]=(uint16_t)boot_mode;regs[5]=addr;
  for(unsigned i=0;i<6;i++)regs[6+i]=(uint16_t)uid[2*i]<<8|uid[2*i+1];
  tx[2]=(uint8_t)(2*count);for(unsigned i=0;i<count;i++){tx[3+2*i]=(uint8_t)(regs[start+i]>>8);tx[4+2*i]=(uint8_t)regs[start+i];}reply(tx,3+2*count);
 }else if(rx[1]==FC_OTA&&n>=3){
  int ok=0;uint8_t op=rx[2];
  if(op==1&&n==51)ok=stage_begin(rx+3);
  if(op==2&&n>=8&&rx[7]>0&&rx[7]<=CHUNK_SIZE&&n==(size_t)8+rx[7])ok=stage_data(get32(rx+3),rx+8,rx[7]);
  if(op==3&&n==3){ok=stage_commit();reboot=ok;}
  if(op==4&&n==3)ok=1;
  tx[2]=op;tx[3]=(uint8_t)ok;put32(tx+4,stage_received());reply(tx,8);
  if(reboot){delay_ms(30);board_reset();}
 }else{tx[1]|=0x80;tx[2]=1;reply(tx,3);}
 }
}

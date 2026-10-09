#include "gateway.h"
#include "driver/uart.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static node_info sim[3];static uint8_t response[256];static size_t response_len;static int64_t us;static int corrupt_reply,drop_confirmation,persist_ok=1;static unsigned broadcasts;static int powered[3]={1,1,0};
int64_t esp_timer_get_time(void){return us;}
void vTaskDelay(unsigned n){us+=(int64_t)n*1000;}
int xQueueReceive(QueueHandle_t q,void *p,unsigned t){(void)q;(void)p;(void)t;return 0;}
void xQueueReset(QueueHandle_t q){(void)q;}
int uart_driver_install(int a,int b,int c,int d,QueueHandle_t *q,int f){(void)a;(void)b;(void)c;(void)d;(void)f;*q=(void *)1;return 0;}
int uart_param_config(int a,const uart_config_t *b){(void)a;(void)b;return 0;}
int uart_set_pin(int a,int b,int c,int d,int e){(void)a;(void)b;(void)c;(void)d;(void)e;return 0;}
int uart_set_mode(int a,int b){(void)a;(void)b;return 0;}
int uart_flush_input(int a){(void)a;response_len=0;return 0;}
int uart_wait_tx_done(int a,unsigned b){(void)a;(void)b;return 0;}
static void finish(size_t n){put16(response+n,crc16(response,n));response_len=n+2;}
int persist_registry(void){return persist_ok;}
int uart_write_bytes(int port,const void *data,size_t n){
 (void)port;const uint8_t *p=data;assert(crc16(p,n)==0);response[0]=p[0];response[1]=p[1];
 if(p[0]==0&&p[1]==6){
  broadcasts++;assert(n==8&&p[2]==1&&p[3]==0&&p[4]==0);
  for(unsigned i=0;i<3;i++)if(powered[i]&&!sim[i].address)sim[i].address=p[5];
  return (int)n; /* Modbus broadcast never replies. */
 }
 if(p[1]==3){
  unsigned matches=0,which=0;for(unsigned i=0;i<3;i++)if(powered[i]&&sim[i].address==p[0]){matches++;which=i;}
  if(!matches||drop_confirmation)return (int)n;
  if(matches!=1||corrupt_reply){memset(response,0x5a,8);response_len=8;return (int)n;}
  uint16_t regs[12]={123,99,0,1,0,sim[which].address};for(unsigned i=0;i<6;i++)regs[6+i]=(uint16_t)sim[which].uid[2*i]<<8|sim[which].uid[2*i+1];
  response[2]=24;for(unsigned i=0;i<12;i++){response[3+2*i]=(uint8_t)(regs[i]>>8);response[4+2*i]=(uint8_t)regs[i];}finish(27);
 }return (int)n;
}
int uart_read_bytes(int port,void *out,unsigned cap,unsigned wait){(void)port;us+=(int64_t)wait*1000;if(!response_len)return 0;assert(response_len<=cap);size_t n=response_len;memcpy(out,response,n);response_len=0;return (int)n;}
int main(void){
 for(unsigned i=0;i<3;i++)sim[i].uid[11]=(uint8_t)(i+1);sim[0].address=7;sim[1].address=0;sim[2].address=12;
 bus_init();assert(discover_nodes());assert(node_count==1);assert(broadcasts==0);node_info info;
 assert(assign_address(7,&info)==-1);assert(broadcasts==0);
 assert(assign_address(8,&info)==1);assert(broadcasts==1);assert(sim[0].address==7&&sim[1].address==8);assert(info.address==8&&info.uid[11]==2);
 powered[0]=0;assert(discover_nodes());assert(node_count==2);assert(assign_address(7,&info)==-1); /* offline reservation retained */
 persist_ok=0;assert(assign_address(11,&info)==-5);assert(broadcasts==1);persist_ok=1;
 /* Lost unicast confirmation after broadcast leaves a durable reservation. */
 sim[1].address=0;drop_confirmation=1;assert(assign_address(9,&info)==-3);assert(sim[1].address==9);assert(broadcasts==2);assert(assign_address(9,&info)==-1);
 /* Restored scan sees conflicting previous UID at another address: reject. */
 drop_confirmation=0;assert(!discover_nodes());
 /* Fresh registry after a crash: pending address is adopted without broadcast. */
 node_count=1;memset(nodes,0,sizeof *nodes);nodes[0].address=9;nodes[0].mode=2;assert(discover_nodes());assert(node_count==1&&nodes[0].uid[11]==2&&nodes[0].mode==0);assert(broadcasts==2);
 node_info saved[MAX_NODES];memcpy(saved,nodes,sizeof saved);corrupt_reply=1;assert(!discover_nodes());assert(!memcmp(saved,nodes,sizeof saved));
 puts("PASS: broadcast assignment, no broadcast ACK, occupied/offline IDs, pre-write persistence failure, uncertain reservation and read-only recovery");return 0;
}

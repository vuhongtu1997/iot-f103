/* Bare-register driver for genuine STM32F103C8T6, HSI 8MHz, USART1 38400 8N1.
 * UART RX is polled; flash operations only run after a complete request. */
#include "board.h"
#include "protocol.h"
#include <string.h>
#define R(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define RCC 0x40021000u
#define GPIOA 0x40010800u
#define GPIOC 0x40011000u
#define UART 0x40013800u
#define SPI 0x40013000u
#define ADC 0x40012400u
#define FLASH 0x40022000u
static volatile uint32_t ticks;
void SysTick_Handler(void){ticks++;}
uint32_t millis(void){return ticks;}
void delay_ms(uint32_t n){uint32_t start=millis();while(millis()-start<n){}}
static void pin(unsigned p,unsigned cfg){uint32_t a=GPIOA+(p<8?0:4),s=(p%8)*4;R(a)=(R(a)&~(15u<<s))|(cfg<<s);}
static void de(int on){R(GPIOA+0x10)=on?(1u<<8):(1u<<24);}
static void cs(int on){R(GPIOA+0x10)=on?(1u<<20):(1u<<4);}
void board_init(void){
 R(0xe000ed08)=
#ifdef BOOTLOADER
 0x08000000u;
#else
 APP_BASE;
#endif
 R(RCC+0x18)|=(1u<<2)|(1u<<4)|(1u<<9)|(1u<<12)|(1u<<14);
 /* HSI remains at reset 8MHz. ADC clock HSI / 4 = 2MHz. */
 R(RCC+4)=(R(RCC+4)&~(3u<<14))|(1u<<14);
 pin(8,2);de(0);pin(9,0xb);pin(10,4);
 R(UART+8)=208;R(UART+12)=(1u<<13)|(1u<<3)|(1u<<2);
 pin(4,2);cs(0);pin(5,0xb);pin(6,4);pin(7,0xb);
 R(SPI)=(1u<<2)|(1u<<8)|(1u<<9)|(1u<<6); /* master, /2, software NSS */
 pin(0,0);R(ADC+0x10)=7; /* SMPR2 channel 0 sample 239.5 cycles */
 R(ADC+8)=1;R(0xe000e014)=7999;R(0xe000e018)=0;R(0xe000e010)=7;
 __asm volatile("cpsie i");delay_ms(2);
 R(ADC+8)|=1u<<3;while(R(ADC+8)&(1u<<3)){}
 R(ADC+8)|=1u<<2;while(R(ADC+8)&(1u<<2)){}
 R(ADC+8)|=(7u<<17)|(1u<<20);
 R(GPIOC+4)=(R(GPIOC+4)&~(15u<<20))|(2u<<20);R(GPIOC+0x10)=1u<<13;
}
void board_uid(uint8_t out[12]){for(unsigned i=0;i<12;i++)out[i]=*(volatile const uint8_t *)(uintptr_t)(0x1ffff7e8u+i);}
int bus_receive(uint8_t *out,size_t cap){
 static uint8_t buf[FRAME_MAX];static size_t used;static uint32_t last;static int bad;
 while(R(UART)&0x3f){
  uint32_t sr=R(UART);uint8_t b=(uint8_t)R(UART+4);
  if(sr&15u)bad=1;
  if(sr&(1u<<5)){if(used<sizeof buf)buf[used++]=b;else bad=1;last=millis();}
 }
 if(used&&millis()-last>=3){int n=0;if(!bad&&used<=cap){memcpy(out,buf,used);n=(int)used;}used=0;bad=0;return n;}
 return 0;
}
void bus_send(const uint8_t *p,size_t n){
 delay_ms(2);de(1);for(size_t i=0;i<n;i++){while(!(R(UART)&(1u<<7))){}R(UART+4)=p[i];}
 while(!(R(UART)&(1u<<6))){}de(0);
 /* Drain possible local echo (RE is tied to DE, so normally none). */
 while(R(UART)&0x3f){volatile uint32_t discard=R(UART+4);(void)discard;}
}
uint16_t sensor_raw(void){R(ADC+8)|=1u<<22;uint32_t t=millis();while(!(R(ADC)&2u)){if(millis()-t>10)return 0xffff;}return (uint16_t)R(ADC+0x4c);}
void board_reset(void){R(0xe000ed0c)=0x05fa0004u;for(;;){}}
void board_jump(uint32_t base){
 uint32_t sp=R(base),pc=R(base+4);__asm volatile("cpsid i");R(0xe000e010)=0;
 for(unsigned i=0;i<8;i++){R(0xe000e180+4*i)=0xffffffff;R(0xe000e280+4*i)=0xffffffff;}
 R(0xe000ed04)=(1u<<25)|(1u<<27);R(0xe000ed08)=base;
 __asm volatile("msr msp, %0\n bx %1"::"r"(sp),"r"(pc):"memory");__builtin_unreachable();
}
static int flash_wait(void){while(R(FLASH+0xc)&1){}return (R(FLASH+0xc)&0x14u)==0;}
static void unlock(void){if(R(FLASH+0x10)&0x80){R(FLASH+4)=0x45670123;R(FLASH+4)=0xcdef89ab;}R(FLASH+0xc)=0x34;}
int flash_erase(uint32_t a){
 if(a<APP_BASE||a>=0x08010000u||(a&1023u))return 0;
 __asm volatile("cpsid i");unlock();R(FLASH+0x10)=2;R(FLASH+0x14)=a;R(FLASH+0x10)|=0x40;
 int ok=flash_wait();R(FLASH+0x10)=0x80;__asm volatile("cpsie i");return ok;
}
int flash_write(uint32_t a,const void *data,size_t n){
 if(a<APP_BASE||a+n>0x08010000u||(a&1)||(n&1))return 0;
 const uint8_t *p=data;int ok=1;__asm volatile("cpsid i");unlock();
 for(size_t i=0;i<n;i+=2){uint16_t v=get16(p+i);R(FLASH+0x10)=1;*(volatile uint16_t *)(uintptr_t)(a+i)=v;
 if(!flash_wait()||*(volatile const uint16_t *)(uintptr_t)(a+i)!=v){ok=0;break;}}
 R(FLASH+0x10)=0x80;__asm volatile("cpsie i");return ok;
}
static int spi_fault;
static uint8_t spi_byte(uint8_t b){
 unsigned guard=100000;while(!(R(SPI+8)&2u)&&--guard){}if(!guard){spi_fault=1;return 0xff;}
 *(volatile uint8_t *)(uintptr_t)(SPI+0xc)=b;
 guard=100000;while(!(R(SPI+8)&1u)&&--guard){}if(!guard){spi_fault=1;return 0xff;}
 return *(volatile uint8_t *)(uintptr_t)(SPI+0xc);
}
static void spi_end(void){unsigned guard=100000;while((R(SPI+8)&0x80u)&&--guard){}if(!guard)spi_fault=1;cs(0);}
static void command(uint8_t op,uint32_t a){cs(1);spi_byte(op);spi_byte((uint8_t)(a>>16));spi_byte((uint8_t)(a>>8));spi_byte((uint8_t)a);}
static int nor_wait(void){uint32_t t=millis();do{cs(1);spi_byte(5);uint8_t s=spi_byte(0xff);spi_end();if(spi_fault)return 0;if(!(s&1))return 1;}while(millis()-t<5000);return 0;}
static int wen(void){if(!nor_wait())return 0;cs(1);spi_byte(6);spi_end();return !spi_fault;}
int nor_init(void){spi_fault=0;cs(1);spi_byte(0x9f);uint8_t m=spi_byte(0xff),type=spi_byte(0xff),size=spi_byte(0xff);spi_end();return !spi_fault&&m==0xef&&type==0x40&&size==0x16;}
int nor_read(uint32_t a,void *out,size_t n){if(a+n>0x400000u||!nor_wait())return 0;uint8_t *p=out;command(3,a);while(n--)*p++=spi_byte(0xff);spi_end();return !spi_fault;}
int nor_write(uint32_t a,const void *data,size_t n){
 if(a+n>0x400000u)return 0;const uint8_t *p=data;
 while(n){size_t take=256-(a&255u);if(take>n)take=n;if(!wen())return 0;command(2,a);
 for(size_t i=0;i<take;i++)spi_byte(p[i]);spi_end();if(!nor_wait())return 0;a+=take;p+=take;n-=take;}return !spi_fault;
}
int nor_erase(uint32_t a){if(a>=0x400000u||(a&4095u)||!wen())return 0;command(0x20,a);spi_end();return nor_wait();}

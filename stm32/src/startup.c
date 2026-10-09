#include <stdint.h>
extern uint32_t _estack,_sidata,_sdata,_edata,_sbss,_ebss;
extern int main(void);
extern void SysTick_Handler(void);
void Reset_Handler(void){uint32_t *s=&_sidata;for(uint32_t *d=&_sdata;d<&_edata;)*d++=*s++;for(uint32_t *d=&_sbss;d<&_ebss;)*d++=0;main();for(;;){}}
static void Default_Handler(void){for(;;){}}
typedef void (*handler)(void);
__attribute__((section(".isr_vector"),used)) const handler vectors[76]={
 [0]=(handler)&_estack,[1]=Reset_Handler,[2 ... 14]=Default_Handler,[15]=SysTick_Handler,[16 ... 75]=Default_Handler
};

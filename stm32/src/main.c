#include "board.h"
#include "node.h"
int main(void){board_init();
#ifdef BOOTLOADER
 int installed=stage_install();
 const volatile uint32_t *v=(const volatile uint32_t *)(uintptr_t)APP_BASE;
 uint32_t sp=v[0],pc=v[1];
 if(installed>=0&&sp>0x20000000u&&sp<=0x20005000u&&!(sp&7u)&&(pc&1u)&&(pc&~1u)>=APP_BASE&&(pc&~1u)<APP_LIMIT)board_jump(APP_BASE);
 node_run(1);
#else
 node_run(0);
#endif
 return 0;}

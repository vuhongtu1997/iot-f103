#include "protocol.h"
uint16_t crc16(const void *data,size_t n) {
 const uint8_t *p=data;uint16_t c=0xffff;
 while(n--){c^=*p++;for(unsigned i=0;i<8;i++)c=(c>>1)^((c&1)?0xa001:0);}return c;
}
uint32_t crc32_update(uint32_t c,const void *data,size_t n) {
 const uint8_t *p=data;while(n--){c^=*p++;for(unsigned i=0;i<8;i++)c=(c>>1)^((c&1)?0xedb88320u:0);}return c;
}
int prefix_matches(const uint8_t uid[12],const uint8_t prefix[12],unsigned bits) {
 if(bits>96)return 0;
 for(unsigned i=0;i<bits;i++)if(((uid[i/8]^prefix[i/8])&(0x80u>>(i%8)))!=0)return 0;
 return 1;
}
int constant_equal(const void *a,const void *b,size_t n) {
 const uint8_t *x=a,*y=b;uint8_t d=0;while(n--)d|=*x++^*y++;return d==0;
}

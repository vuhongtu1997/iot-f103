#ifndef IOT_BOARD_H
#define IOT_BOARD_H
#include <stdint.h>
#include <stddef.h>
void board_init(void);
uint32_t millis(void);
void delay_ms(uint32_t);
void board_uid(uint8_t out[12]);
int bus_receive(uint8_t *out,size_t cap);
void bus_send(const uint8_t *,size_t);
uint16_t sensor_raw(void);
void board_reset(void);
void board_jump(uint32_t base);
int flash_erase(uint32_t);
int flash_write(uint32_t,const void *,size_t);
int nor_init(void);
int nor_read(uint32_t,void *,size_t);
int nor_write(uint32_t,const void *,size_t);
int nor_erase(uint32_t);
#endif

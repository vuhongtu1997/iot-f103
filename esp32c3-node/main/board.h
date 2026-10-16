#ifndef ESP32C3_NODE_BOARD_H
#define ESP32C3_NODE_BOARD_H
#include <stdint.h>
#include <stddef.h>
#include "sensors.h"
void board_init(void);
uint32_t millis(void);
void delay_ms(uint32_t ms);
void board_uid(uint8_t out[12]);
int bus_receive(uint8_t *out, size_t cap);
void bus_send(const uint8_t *data, size_t len);
int board_factory_button_pressed(void);
void board_reset(void);
#endif

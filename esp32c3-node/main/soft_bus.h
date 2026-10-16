#ifndef ESP32_NODE_SOFT_BUS_H
#define ESP32_NODE_SOFT_BUS_H
#include <stdint.h>
#include <stddef.h>
void soft_bus_init(void);
int soft_bus_receive(uint8_t *out, size_t capacity);
void soft_bus_send(const uint8_t *data, size_t length);
uint32_t soft_bus_errors(void);
#endif

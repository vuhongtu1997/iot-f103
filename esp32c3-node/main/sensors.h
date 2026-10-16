#ifndef ESP32_NODE_SENSORS_H
#define ESP32_NODE_SENSORS_H
#include <stdint.h>
void sensors_init(void);
void sensor_registers(uint16_t *registers, int boot_mode);
#endif

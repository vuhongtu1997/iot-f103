#ifndef IOT_SENSOR_CODEC_H
#define IOT_SENSOR_CODEC_H
#include <stdint.h>
#include <stddef.h>
/* Output is unchanged when validation fails. ZE03 units: 0.01 ppm. */
int ze03_decode(const uint8_t *frame, size_t size, uint8_t *gas, uint16_t *ppm100);
int zs03_decode(const uint8_t frame[5], int16_t *temperature10, uint16_t *humidity10);
#endif

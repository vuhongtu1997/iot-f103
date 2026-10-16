#ifndef IOT_PROTOCOL_H
#define IOT_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#define BUS_BAUD 38400
#define MAX_NODES 247
#define ADDRESS_REGISTER 0x0100u
#define FC_OTA 0x42
#define MODEL_STM32 0x000103c8u
#define MODEL_ESP32 0x00000032u
#define MODEL_ESP32C3_NODE 0x0033434eu
#define MODBUS_REGISTER_COUNT 13u
#define MODBUS_SENSOR_TYPE_REGISTER 12u
#define MODBUS_SENSOR_UNKNOWN 0u
#define MODBUS_SENSOR_GAS 1u
#define MODBUS_SENSOR_TEMPERATURE_HUMIDITY 2u
#define MODBUS_SENSOR_CH3 3u
#define MODBUS_SENSOR_PRESSURE 4u
/* ESP32 digital sensors extend the legacy register map, preserving 0..12.
 * Values are valid only when their bit in register 14 is set. */
#define DIGITAL_REGISTER_COUNT 21u
#define DIGITAL_SCHEMA_REGISTER 13u
#define DIGITAL_SCHEMA_V1 0x0101u
#define DIGITAL_VALID_REGISTER 14u
#define DIGITAL_H2S_REGISTER 15u /* ppm * 100 */
#define DIGITAL_NH3_REGISTER 16u /* ppm * 100 */
#define DIGITAL_PRESSURE_REGISTER 17u /* uint32 Pa, high word first */
#define DIGITAL_TEMPERATURE_REGISTER 19u /* signed int16 degrees C * 10 */
#define DIGITAL_HUMIDITY_REGISTER 20u /* %RH * 10 */
#define SENSOR_VALID_H2S 0x01u
#define SENSOR_VALID_NH3 0x02u
#define SENSOR_VALID_PRESSURE 0x04u
#define SENSOR_VALID_TEMPERATURE 0x08u
#define SENSOR_VALID_HUMIDITY 0x10u
#define SENSOR_WARMING_UP 0x4000u
#define SENSOR_PROTOCOL_UNAVAILABLE 0x8000u
#ifndef NODE_SENSOR_TYPE
#define NODE_SENSOR_TYPE MODBUS_SENSOR_UNKNOWN
#endif
#define APP_BASE 0x08004000u
#define APP_LIMIT 0x0800f800u
#define APP_MAX (APP_LIMIT-APP_BASE)
#define STAGE_BASE 0x1000u
#define READY_MAGIC 0xa55a5aa5u
#define CHUNK_SIZE 128
#define FRAME_MAX 256
#ifndef FW_VERSION
#define FW_VERSION 1u
#endif
/* Commissioning: Modbus broadcast address 0, FC06, register 0x0100, new ID.
 * Only unconfigured nodes accept it. Broadcasts never produce a response.
 * OTA operations: 1 begin, 2 data, 3 verify/activate, 4 progress.
 * Integers in custom frames and the OTA descriptor are little endian.
 * Standard FC03 registers retain Modbus big endian encoding. */
static inline uint16_t get16(const uint8_t *p) {return (uint16_t)(p[0]|p[1]<<8);}
static inline uint32_t get32(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline void put16(uint8_t *p,uint16_t v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static inline void put32(uint8_t *p,uint32_t v) {for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
#ifdef __cplusplus
extern "C" {
#endif
uint16_t crc16(const void *,size_t);
uint32_t crc32_update(uint32_t,const void *,size_t);
int prefix_matches(const uint8_t uid[12],const uint8_t prefix[12],unsigned bits);
int constant_equal(const void *,const void *,size_t);
#ifdef __cplusplus
}
#endif
/* Header in external flash: descriptor[16], READY[4], consumed[4]. */
#define STAGE_HEADER_SIZE 24
#endif

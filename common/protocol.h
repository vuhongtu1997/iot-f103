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
 * Integers in custom frames and signed descriptor are little endian.
 * Standard FC03 registers retain Modbus big endian encoding. */
static inline uint16_t get16(const uint8_t *p) {return (uint16_t)(p[0]|p[1]<<8);}
static inline uint32_t get32(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline void put16(uint8_t *p,uint16_t v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static inline void put32(uint8_t *p,uint32_t v) {for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
uint16_t crc16(const void *,size_t);
uint32_t crc32_update(uint32_t,const void *,size_t);
int prefix_matches(const uint8_t uid[12],const uint8_t prefix[12],unsigned bits);
int constant_equal(const void *,const void *,size_t);
/* Header in external flash: descriptor[16], HMAC[32], READY[4], consumed[4]. */
#define STAGE_HEADER_SIZE 56
#endif

#include "node.h"
#include "board.h"
#include <setjmp.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
static jmp_buf done;
static uint8_t request[32], address, last_response[FRAME_MAX];
static size_t last_response_size;
static int consumed, writes, replies;
uint8_t config_address(void) { return address; }
int config_set_address(uint8_t value)
{
    writes++;
    address = value;
    return 1;
}
int config_factory_reset(void) { address = 0; return 1; }
int board_factory_button_pressed(void) { return 0; }
uint32_t millis(void) { return 0; }
void board_uid(uint8_t uid[12]) { memset(uid, 1, 12); }
int bus_receive(uint8_t *out, size_t cap)
{
    (void)cap;
    if (consumed++)
        longjmp(done, 1);
    memcpy(out, request, 8);
    return 8;
}
void bus_send(const uint8_t *p, size_t n)
{
    assert(crc16(p, n) == 0);
    assert(n <= sizeof last_response);
    memcpy(last_response, p, n);
    last_response_size = n;
    replies++;
}
uint16_t sensor_raw(void) { return 123; }
#ifdef NODE_DIGITAL_SENSORS
void sensor_registers(uint16_t *regs, int boot_mode)
{
    regs[0] = regs[1] = 0xffff;
    regs[DIGITAL_SCHEMA_REGISTER] = DIGITAL_SCHEMA_V1;
    regs[DIGITAL_VALID_REGISTER] = boot_mode ? 0 : SENSOR_VALID_TEMPERATURE;
    regs[DIGITAL_TEMPERATURE_REGISTER] = (uint16_t)(int16_t)-180;
}
#endif
int stage_begin(const uint8_t p[16])
{
    (void)p;
    return 0;
}
int stage_data(uint32_t o, const uint8_t *p, size_t n)
{
    (void)o;
    (void)p;
    (void)n;
    return 0;
}
uint32_t stage_received(void) { return 0; }
int stage_commit(void) { return 0; }
void delay_ms(uint32_t n) { (void)n; }
void board_reset(void) { assert(0); }
static void run(int mode)
{
    put16(request + 6, crc16(request, 6));
    consumed = 0;
    if (!setjmp(done))
        node_run(mode);
}
int main(void)
{
    uint8_t broadcast[6] = {0, 6, 1, 0, 0, 5};
    memcpy(request, broadcast, 6);
    run(0);
    assert(address == 5 && writes == 1 && replies == 0);
    request[5] = 6;
    run(0);
    assert(address == 5 && writes == 1 && replies == 0); /* configured node locked */
    address = 0;
    request[5] = 247;
    run(1);
    assert(address == 247 && writes == 2 && replies == 0); /* recovery bootloader */
    address = 0;
    request[5] = 0;
    run(0);
    assert(address == 0 && writes == 2 && replies == 0);
    request[5] = 248;
    run(0);
    assert(address == 0 && writes == 2 && replies == 0);
    request[5] = 9;
    request[3] = 1;
    run(0);
    assert(address == 0 && writes == 2 && replies == 0); /* wrong register */
    request[3] = 0;
    request[0] = 1;
    run(0);
    assert(address == 0 && writes == 2 && replies == 0); /* no unicast commissioning */
    request[0] = 0;
    request[1] = 3;
    run(0);
    assert(address == 0 && replies == 0); /* broadcast reads never answer */
    address = 5;
    request[0] = 5;
    request[1] = 3;
    request[2] = request[4] = 0; /* FC03 offsets/counts are big endian. */
    request[3] = MODBUS_SENSOR_TYPE_REGISTER;
    request[5] = 1;
    run(0);
    assert(last_response_size == 7 && last_response[2] == 2);
    assert((((unsigned)last_response[3] << 8) | last_response[4]) == NODE_SENSOR_TYPE);
#ifdef NODE_DIGITAL_SENSORS
    request[3] = DIGITAL_SCHEMA_REGISTER;
    request[5] = DIGITAL_REGISTER_COUNT - DIGITAL_SCHEMA_REGISTER;
    run(0);
    assert(last_response_size == 21 && last_response[2] == 16);
    assert(last_response[3] == 1 && last_response[4] == 1); /* schema */
    assert(last_response[5] == 0 && last_response[6] == SENSOR_VALID_TEMPERATURE);
    assert(last_response[15] == 0xff && last_response[16] == 0x4c); /* -18 C */
    run(1);
    assert(last_response[5] == 0 && last_response[6] == 0); /* boot invalid */
    request[3] = DIGITAL_REGISTER_COUNT;
    request[5] = 1;
    run(0);
    assert(last_response_size == 5 && last_response[1] == 0x83 && last_response[2] == 2);
#else
    request[3] = DIGITAL_SCHEMA_REGISTER;
    request[5] = 1;
    run(0);
    assert(last_response_size == 5 && last_response[1] == 0x83 && last_response[2] == 2);
#endif
    puts("PASS: real STM32 parser, unconfigured-only address write, bootloader support, sensor type register, invalid IDs, broadcast silence");
    return 0;
}

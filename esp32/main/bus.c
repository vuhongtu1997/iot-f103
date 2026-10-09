#include "gateway.h"
#include "secrets.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <string.h>
node_info nodes[MAX_NODES];
unsigned node_count;
static QueueHandle_t uart_events;
void bus_init(void)
{
    uart_config_t c = {.baud_rate = BUS_BAUD, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT};
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 1024, 0, 32, &uart_events, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &c));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, RS485_TX_PIN, RS485_RX_PIN, RS485_DE_PIN, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_set_mode(UART_NUM_2, UART_MODE_RS485_HALF_DUPLEX));
}
int bus_exchange(uint8_t addr, uint8_t fc, const uint8_t *p, size_t n, uint8_t *out, size_t cap, int timeout_ms, int *activity)
{
    if (n + 4 > FRAME_MAX)
        return 0;
    uint8_t tx[FRAME_MAX], rx[FRAME_MAX];
    tx[0] = addr;
    tx[1] = fc;
    memcpy(tx + 2, p, n);
    put16(tx + 2 + n, crc16(tx, 2 + n));
    vTaskDelay(pdMS_TO_TICKS(4));
    uart_flush_input(UART_NUM_2);
    xQueueReset(uart_events);
    uart_write_bytes(UART_NUM_2, tx, n + 4);
    uart_wait_tx_done(UART_NUM_2, pdMS_TO_TICKS(100));
    int64_t start = esp_timer_get_time(), last = start;
    size_t used = 0;
    int seen = 0, bad = 0;
    while (esp_timer_get_time() - start < (int64_t)timeout_ms * 1000)
    {
        uint8_t b[64];
        int got = uart_read_bytes(UART_NUM_2, b, sizeof b, pdMS_TO_TICKS(1));
        if (got > 0)
        {
            seen = 1;
            last = esp_timer_get_time();
            if (used + (size_t)got <= sizeof rx)
            {
                memcpy(rx + used, b, (size_t)got);
                used += (size_t)got;
            }
            else
                bad = 1;
        }
        uart_event_t e;
        while (xQueueReceive(uart_events, &e, 0))
            if (e.type == UART_FRAME_ERR || e.type == UART_PARITY_ERR || e.type == UART_FIFO_OVF || e.type == UART_BUFFER_FULL)
            {
                seen = 1;
                bad = 1;
            }
        if (used && esp_timer_get_time() - last > 4000)
            break;
    }
    if (activity)
        *activity = seen;
    if (bad || used < 4 || rx[0] != addr || rx[1] != fc || crc16(rx, used) != 0 || used > cap)
        return 0;
    memcpy(out, rx, used);
    return (int)used;
}
static int probe_address(uint8_t address, node_info *info, int *active)
{
    const uint8_t req[4] = {0, 0, 0, 12};
    uint8_t rx[FRAME_MAX];
    int seen = 0;
    int n = bus_exchange(address, 3, req, 4, rx, sizeof rx, 60, &seen);
    if (active)
        *active = seen;
    if (!n)
        return seen ? -1 : 0;
    if (n != 29 || rx[2] != 24 || rx[13] != 0 || rx[14] != address)
        return -1;
    memset(info, 0, sizeof *info);
    info->address = address;
    info->mode = rx[12];
    info->version = (uint32_t)rx[7] << 24 | (uint32_t)rx[8] << 16 | (uint32_t)rx[9] << 8 | rx[10];
    memcpy(info->uid, rx + 15, 12);
    return 1;
}
static int uid_blank(const uint8_t *uid)
{
    uint8_t any = 0;
    for (unsigned i = 0; i < 12; i++)
        any |= uid[i];
    return !any;
}
int discover_nodes(void)
{
    /* Read-only scan. Never gives an address to a new device. Keep offline and
     * pending reservations loaded from NVS so their IDs cannot be reused. */
    node_info found[MAX_NODES];
    unsigned count = node_count;
    memcpy(found, nodes, count * sizeof *nodes);
    for (unsigned address = 1; address <= MAX_NODES; address++)
    {
        node_info info;
        int result = probe_address((uint8_t)address, &info, 0);
        if (result < 0)
            return 0;
        if (!result)
            continue;
        unsigned at = count;
        for (unsigned i = 0; i < count; i++)
        {
            if (found[i].address == address)
            {
                at = i;
                if (!uid_blank(found[i].uid) && !constant_equal(found[i].uid, info.uid, 12))
                    return 0;
            }
            else if (!uid_blank(found[i].uid) && constant_equal(found[i].uid, info.uid, 12))
                return 0;
        }
        if (at == count)
        {
            if (count == MAX_NODES)
                return 0;
            count++;
        }
        found[at] = info;
    }
    memcpy(nodes, found, count * sizeof *nodes);
    node_count = count;
    return 1;
}
int assign_address(uint8_t address, node_info *assigned)
{
    /* Return 1 confirmed; -1 reserved/occupied; -2 uncertain bus; -3 no usable
     * confirmation; -4 invalid/full; -5 persistence error. Never retry broadcast. */
    if (!address || address > MAX_NODES || node_count == MAX_NODES)
        return -4;
    for (unsigned i = 0; i < node_count; i++)
        if (nodes[i].address == address)
            return -1;
    node_info info;
    for (unsigned attempt = 0; attempt < 3; attempt++)
    {
        int result = probe_address(address, &info, 0);
        if (result > 0)
            return -1;
        if (result < 0)
            return -2;
    }
    /* Reserve durably BEFORE broadcast. An uncertain outcome remains reserved;
     * a later read-only scan can adopt the node without broadcasting again. */
    unsigned slot = node_count++;
    memset(nodes + slot, 0, sizeof *nodes);
    nodes[slot].address = address;
    nodes[slot].mode = 2;
    if (!persist_registry())
    {
        node_count--;
        return -5;
    }
    uint8_t tx[8] = {0, 6, (uint8_t)(ADDRESS_REGISTER >> 8), (uint8_t)ADDRESS_REGISTER, 0, address, 0, 0};
    put16(tx + 6, crc16(tx, 6));
    vTaskDelay(pdMS_TO_TICKS(4));
    uart_flush_input(UART_NUM_2);
    xQueueReset(uart_events);
    if (uart_write_bytes(UART_NUM_2, tx, sizeof tx) != (int)sizeof tx || uart_wait_tx_done(UART_NUM_2, pdMS_TO_TICKS(100)) != 0)
        return -3;
    /* FC06 broadcast has NO response. Give STM32 time to commit flash. */
    vTaskDelay(pdMS_TO_TICKS(200));
    for (unsigned attempt = 0; attempt < 3; attempt++)
    {
        int result = probe_address(address, &info, 0);
        if (result == 1)
        {
            for (unsigned i = 0; i < slot; i++)
                if (constant_equal(nodes[i].uid, info.uid, 12))
                    return -3;
            nodes[slot] = info;
            if (!persist_registry())
                return -5;
            if (assigned)
                *assigned = info;
            return 1;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return -3;
}
int read_node(const node_info *node, uint16_t regs[12])
{
    const uint8_t req[4] = {0, 0, 0, 12};
    uint8_t rx[FRAME_MAX];
    int n = bus_exchange(node->address, 3, req, 4, rx, sizeof rx, 150, 0);
    if (n != 29 || rx[2] != 24)
        return 0;
    for (unsigned i = 0; i < 12; i++)
        regs[i] = (uint16_t)rx[3 + 2 * i] << 8 | rx[4 + 2 * i];
    for (unsigned i = 0; i < 6; i++)
        if (regs[6 + i] != ((uint16_t)node->uid[2 * i] << 8 | node->uid[2 * i + 1]))
            return 0;
    return 1;
}
int node_ota_op(const node_info *node, uint8_t op, const uint8_t *data, size_t n, uint32_t *received)
{
    if (n + 1 > FRAME_MAX - 4)
        return 0;
    uint8_t p[FRAME_MAX], rx[FRAME_MAX];
    p[0] = op;
    if (n)
        memcpy(p + 1, data, n);
    for (unsigned attempt = 0; attempt < 3; attempt++)
    {
        int len = bus_exchange(node->address, FC_OTA, p, n + 1, rx, sizeof rx, op == 1 ? 20000 : op == 3 ? 15000
                                                                                                         : 1000,
                               0);
        if (len == 10 && rx[2] == op)
        {
            if (received)
                *received = get32(rx + 4);
            return rx[3] == 1;
        }
    }
    return 0;
}

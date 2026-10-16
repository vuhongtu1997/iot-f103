#include "soft_uart.h"
#include <string.h>

unsigned soft_uart_bit_boundary(unsigned bit)
{
    return (bit * 1000000u + SOFT_UART_BAUD / 2) / SOFT_UART_BAUD;
}
unsigned soft_uart_tx_level(uint8_t byte, unsigned bit)
{
    return bit == 0 ? 0 : bit >= 9 ? 1 : (byte >> (bit - 1)) & 1;
}
void soft_uart_rx_init(soft_uart_rx_t *r, uint32_t now, int level)
{
    memset(r, 0, sizeof *r);
    r->end = now;
    r->level = !!level;
}
void soft_uart_rx_lost(soft_uart_rx_t *r, uint32_t now, int level)
{
    uint32_t errors = r->errors + 1;
    soft_uart_rx_init(r, now, level);
    r->errors = errors;
    r->active = r->bad = 1; /* discard through the next RTU silent interval */
}
void soft_uart_rx_advance(soft_uart_rx_t *r, uint32_t now)
{
    while (r->receiving && r->sample < 10) {
        unsigned offset = ((2u * r->sample + 1) * 1000000u + SOFT_UART_BAUD) /
                          (2u * SOFT_UART_BAUD);
        if ((int32_t)(now - r->start) < (int32_t)offset) break;
        unsigned sample = r->sample++;
        if (!sample && r->level) {
            r->bad = 1;
            r->receiving = 0;
            r->end = now;
        } else if (sample >= 1 && sample <= 8) {
            r->value |= r->level << (sample - 1);
        } else if (sample == 9) {
            if (!r->level || r->used == SOFT_UART_FRAME_MAX) r->bad = 1;
            else r->frame[r->used++] = r->value;
            r->receiving = 0;
            r->end = r->start + soft_uart_bit_boundary(10);
        }
    }
    if (r->active && !r->receiving && (int32_t)(now - r->end) >= (int32_t)SOFT_UART_GAP_US) {
        if (r->bad || r->ready_size) r->errors++;
        else if (r->used) {
            memcpy(r->ready, r->frame, r->used);
            r->ready_size = r->used;
        }
        r->active = r->used = r->bad = 0;
    }
}
void soft_uart_rx_edge(soft_uart_rx_t *r, uint32_t now, int level)
{
    soft_uart_rx_advance(r, now);
    level = !!level;
    if (r->level && !level && !r->receiving) {
        if (r->active && (int32_t)(now - r->end) > 750) r->bad = 1;
        r->active = r->receiving = 1;
        r->start = now;
        r->sample = r->value = 0;
    }
    r->level = level;
}
int soft_uart_rx_take(soft_uart_rx_t *r, uint8_t *out, size_t capacity)
{
    unsigned size = r->ready_size;
    r->ready_size = 0;
    if (size > capacity) { r->errors++; return 0; }
    if (size) memcpy(out, r->ready, size);
    return (int)size;
}

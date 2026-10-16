#ifndef IOT_SOFT_UART_H
#define IOT_SOFT_UART_H
#include <stdint.h>
#include <stddef.h>
#define SOFT_UART_FRAME_MAX 256u
#define SOFT_UART_BAUD 38400u
#define SOFT_UART_GAP_US 1750u

/* Edge timestamps use microseconds modulo 2^32. Call advance only after all
 * edges up to that timestamp have been consumed. No ESP-IDF dependencies. */
typedef struct {
    uint32_t start, end, errors;
    uint16_t used, ready_size;
    uint8_t level, receiving, sample, value, active, bad;
    uint8_t frame[SOFT_UART_FRAME_MAX], ready[SOFT_UART_FRAME_MAX];
} soft_uart_rx_t;
void soft_uart_rx_init(soft_uart_rx_t *, uint32_t now, int level);
void soft_uart_rx_edge(soft_uart_rx_t *, uint32_t now, int level);
void soft_uart_rx_advance(soft_uart_rx_t *, uint32_t now);
void soft_uart_rx_lost(soft_uart_rx_t *, uint32_t now, int level);
int soft_uart_rx_take(soft_uart_rx_t *, uint8_t *, size_t capacity);
/* Ten line levels per 8N1 byte, LSB first, for the RMT transmitter. */
unsigned soft_uart_tx_level(uint8_t byte, unsigned bit);
unsigned soft_uart_bit_boundary(unsigned bit);
#endif

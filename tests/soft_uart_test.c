#include "soft_uart.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Independent serial source: ideal hardware UART bit clock, optionally offset
 * by 1.5%, with alternating ISR timestamp jitter. No production TX helpers. */
static uint32_t waveform(soft_uart_rx_t *rx, const uint8_t *data, size_t size,
                         uint32_t origin, double period, int jitter, int stop)
{
    int level = 1;
    unsigned transitions = 0;
    for (size_t byte = 0; byte < size; ++byte)
        for (unsigned bit = 0; bit < 10; ++bit) {
            int next = bit == 0 ? 0 : bit == 9 ? stop : (data[byte] >> (bit - 1)) & 1;
            if (next != level) {
                uint32_t at = origin + (uint32_t)((byte * 10 + bit) * period + 0.5);
                if (jitter) at += transitions++ % 2 ? jitter : -jitter;
                soft_uart_rx_edge(rx, at, next);
                level = next;
            }
        }
    return origin + (uint32_t)(size * 10 * period + 0.5);
}
static void check_frame(const uint8_t *data, size_t size, uint32_t start,
                        double period, int jitter)
{
    soft_uart_rx_t rx;
    uint8_t out[SOFT_UART_FRAME_MAX];
    soft_uart_rx_init(&rx, start - 2000, 1);
    uint32_t end = waveform(&rx, data, size, start, period, jitter, 1);
    soft_uart_rx_advance(&rx, end + SOFT_UART_GAP_US + 100);
    assert(soft_uart_rx_take(&rx, out, sizeof out) == (int)size);
    assert(memcmp(data, out, size) == 0 && rx.errors == 0);
}
int main(void)
{
    uint8_t bytes[SOFT_UART_FRAME_MAX + 1], out[SOFT_UART_FRAME_MAX];
    const uint8_t modbus[] = {5, 3, 0, 0, 0, 12, 0x44, 0x4b};
    check_frame(modbus, sizeof modbus, 2000, 1000000.0 / 38400, 0);
    for (unsigned i = 0; i < sizeof bytes; ++i) bytes[i] = (uint8_t)i;
    check_frame(bytes, 256, 2000, 1000000.0 / 38400, 0);
    check_frame(bytes, 256, 0xfffff000u, 1000000.0 / 38400, 0);
    check_frame(bytes, 256, 2000, 1000000.0 / 38400 * 1.015, 2);
    check_frame(bytes, 256, 2000, 1000000.0 / 38400 * 0.985, 2);
    memset(bytes, 0x55, sizeof bytes); /* maximum edge rate */
    check_frame(bytes, 256, 2000, 1000000.0 / 38400, 2);
    memset(bytes, 0xff, sizeof bytes); /* long high runs */
    check_frame(bytes, 256, 2000, 1000000.0 / 38400, 2);
    memset(bytes, 0, sizeof bytes); /* long low runs */
    check_frame(bytes, 256, 2000, 1000000.0 / 38400, 2);

    soft_uart_rx_t rx;
    soft_uart_rx_init(&rx, 0, 1);
    uint32_t end = waveform(&rx, bytes, 1, 2000, 1000000.0 / 38400, 0, 1);
    soft_uart_rx_advance(&rx, end - 5); /* stop sampled, byte end still in future */
    assert(rx.used == 1 && rx.active && !rx.ready_size && !rx.errors);
    soft_uart_rx_advance(&rx, end + SOFT_UART_GAP_US - 1);
    assert(!soft_uart_rx_take(&rx, out, sizeof out));
    soft_uart_rx_advance(&rx, end + SOFT_UART_GAP_US);
    assert(soft_uart_rx_take(&rx, out, sizeof out) == 1 && !rx.errors);

    soft_uart_rx_init(&rx, 0, 1);
    end = waveform(&rx, bytes, 257, 2000, 1000000.0 / 38400, 0, 1);
    soft_uart_rx_advance(&rx, end + 2000);
    assert(!soft_uart_rx_take(&rx, out, sizeof out) && rx.errors == 1);

    soft_uart_rx_init(&rx, 0, 1);
    end = waveform(&rx, bytes, 1, 2000, 1000000.0 / 38400, 0, 0); /* bad stop */
    soft_uart_rx_advance(&rx, end + 2000);
    assert(!soft_uart_rx_take(&rx, out, sizeof out) && rx.errors == 1);
    soft_uart_rx_edge(&rx, end + 2100, 1);
    end = waveform(&rx, modbus, sizeof modbus, end + 4000, 1000000.0 / 38400, 0, 1);
    soft_uart_rx_advance(&rx, end + 2000);
    assert(soft_uart_rx_take(&rx, out, sizeof out) == sizeof modbus);
    assert(!memcmp(out, modbus, sizeof modbus));

    soft_uart_rx_init(&rx, 0, 1);
    end = waveform(&rx, bytes, 1, 2000, 1000000.0 / 38400, 0, 1);
    end = waveform(&rx, bytes, 1, end + 900, 1000000.0 / 38400, 0, 1);
    soft_uart_rx_advance(&rx, end + 2000);
    assert(!soft_uart_rx_take(&rx, out, sizeof out) && rx.errors == 1); /* intra-frame gap */

    soft_uart_rx_init(&rx, 0, 1);
    end = waveform(&rx, modbus, sizeof modbus, 2000, 1000000.0 / 38400, 0, 1);
    /* Next falling edge must finish the old frame AND start the next byte. */
    uint32_t next = end + 2000;
    soft_uart_rx_edge(&rx, next, 0);
    assert(soft_uart_rx_take(&rx, out, sizeof out) == sizeof modbus);
    assert(!memcmp(out, modbus, sizeof modbus));
    soft_uart_rx_lost(&rx, next + 100, 1); /* ring overflow: discard damaged frame */
    soft_uart_rx_advance(&rx, next + 3000);
    assert(!soft_uart_rx_take(&rx, out, sizeof out));
    end = waveform(&rx, modbus, sizeof modbus, next + 4000, 1000000.0 / 38400, 0, 1);
    soft_uart_rx_advance(&rx, end + 2000);
    assert(soft_uart_rx_take(&rx, out, sizeof out) == sizeof modbus);

    for (unsigned byte = 0; byte < 256; ++byte) {
        assert(soft_uart_tx_level(byte, 0) == 0 && soft_uart_tx_level(byte, 9) == 1);
        for (unsigned bit = 1; bit < 9; ++bit)
            assert(soft_uart_tx_level(byte, bit) == ((byte >> (bit - 1)) & 1));
    }
    for (unsigned bit = 0; bit < 10; ++bit) {
        unsigned width = soft_uart_bit_boundary(bit + 1) - soft_uart_bit_boundary(bit);
        assert(width == 26 || width == 27);
    }
    puts("PASS: software UART waveforms, 256-byte frames, jitter, clock drift, timer wrap, framing, gaps, overflow, resync, TX 8N1");
    return 0;
}

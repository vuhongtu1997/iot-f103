#include "board.h"
#include "protocol.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "soft_bus.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "node.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

#if !CONFIG_NODE_RS485_SOFT
static QueueHandle_t events;
#endif
static uint32_t rx_frames, tx_frames, uart_errors;

void board_init(void)
{
    ESP_ERROR_CHECK(CONFIG_NODE_UART_RX != CONFIG_NODE_UART_TX &&
                    CONFIG_NODE_UART_DE != CONFIG_NODE_UART_RX &&
                    CONFIG_NODE_UART_DE != CONFIG_NODE_UART_TX &&
                    CONFIG_NODE_RESET_GPIO != CONFIG_NODE_UART_RX &&
                    CONFIG_NODE_RESET_GPIO != CONFIG_NODE_UART_TX &&
                    CONFIG_NODE_RESET_GPIO != CONFIG_NODE_UART_DE ? ESP_OK : ESP_ERR_INVALID_ARG);
#if CONFIG_NODE_RS485_SOFT
    soft_bus_init();
#else
    const uart_config_t uart = {
        .baud_rate = BUS_BAUD, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, CONFIG_NODE_UART_TX,
        CONFIG_NODE_UART_RX, CONFIG_NODE_UART_DE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 1024, 0, 20, &events, 0));
    ESP_ERROR_CHECK(uart_set_mode(UART_NUM_1, UART_MODE_RS485_HALF_DUPLEX));
    /* 4 symbols of silence delimit RTU frames; handle timeout events, not task timing. */
    ESP_ERROR_CHECK(uart_set_rx_timeout(UART_NUM_1, 4));
    uart_set_always_rx_timeout(UART_NUM_1, true);
#endif
    const gpio_config_t button = {
        .pin_bit_mask = 1ULL << CONFIG_NODE_RESET_GPIO, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&button));
    sensors_init();
}

uint32_t millis(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
void delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
void board_reset(void) { esp_restart(); }
int board_factory_button_pressed(void) { return !gpio_get_level(CONFIG_NODE_RESET_GPIO); }
void board_uid(uint8_t out[12])
{
    memcpy(out, "C3NODE", 6);
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(out + 6));
}
int bus_receive(uint8_t *out, size_t cap)
{
    static uint32_t last_status;
    const uint32_t now = millis();
    if (now - last_status >= 10000) {
        last_status = now;
#if CONFIG_NODE_RS485_SOFT
        uart_errors = soft_bus_errors();
#endif
        ESP_LOGI("node", "Alive: address=%u rx_frames=%lu tx_frames=%lu uart_errors=%lu",
                 (unsigned)config_address(), (unsigned long)rx_frames,
                 (unsigned long)tx_frames, (unsigned long)uart_errors);
    }
#if CONFIG_NODE_RS485_SOFT
    int size = soft_bus_receive(out, cap);
    if (size) rx_frames++;
    return size;
#else
    static uint8_t frame[FRAME_MAX];
    static size_t used;
    static int bad;
    uart_event_t event;
    if (!xQueueReceive(events, &event, pdMS_TO_TICKS(10)))
        return 0;
    if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
        uart_errors++;
        uart_flush_input(UART_NUM_1);
        xQueueReset(events);
        used = 0;
        bad = 0;
    } else if (event.type == UART_PARITY_ERR || event.type == UART_FRAME_ERR) {
        uart_errors++;
        bad = 1;
    } else if (event.type == UART_DATA) {
        size_t remaining = event.size;
        uint8_t data[128];
        while (remaining) {
            size_t want = remaining < sizeof data ? remaining : sizeof data;
            int got = uart_read_bytes(UART_NUM_1, data, want, 0);
            if (got <= 0) { bad = 1; break; }
            if (used + got <= sizeof frame) {
                memcpy(frame + used, data, got);
                used += got;
            } else { bad = 1; }
            remaining -= got;
        }
        if (event.timeout_flag) {
            int size = !bad && used <= cap ? (int)used : 0;
            if (size) rx_frames++;
            if (size) memcpy(out, frame, size);
            used = 0;
            bad = 0;
            return size;
        }
    }
    return 0;
#endif
}
void bus_send(const uint8_t *data, size_t len)
{
#if CONFIG_NODE_RS485_SOFT
    soft_bus_send(data, len);
#else
    const int written = uart_write_bytes(UART_NUM_1, data, len);
    ESP_ERROR_CHECK(written == (int)len ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(200)));
#endif
    tx_frames++;
}

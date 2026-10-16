#include "sdkconfig.h"
#if CONFIG_NODE_RS485_SOFT
#include "soft_bus.h"
#include "soft_uart.h"
#include "protocol.h"
#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "esp_intr_alloc.h"
#include "soc/gpio_reg.h"
#include "soc/soc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

_Static_assert(BUS_BAUD == SOFT_UART_BAUD, "Soft bus baud must match master");
_Static_assert(FRAME_MAX <= SOFT_UART_FRAME_MAX, "Soft bus frame capacity");
#define EDGE_COUNT 2048u
typedef struct { uint32_t time; uint8_t level; } edge_t;
static DRAM_ATTR edge_t edges[EDGE_COUNT];
static volatile unsigned head, tail;
static volatile int overflow, transmitting;
static portMUX_TYPE ring_lock = portMUX_INITIALIZER_UNLOCKED;
static soft_uart_rx_t decoder;
static rmt_channel_handle_t tx_channel;
static rmt_encoder_handle_t tx_encoder;
static rmt_symbol_word_t symbols[FRAME_MAX * 5];

static void IRAM_ATTR rx_edge(void *arg)
{
    (void)arg;
    if (transmitting) return;
    unsigned next = (head + 1) % EDGE_COUNT;
    if (next == tail) { overflow = 1; return; }
    edges[head].time = (uint32_t)esp_timer_get_time();
    edges[head].level = !!(REG_READ(GPIO_IN_REG) & (1u << CONFIG_NODE_UART_RX));
    __asm__ volatile ("" ::: "memory");
    head = next;
}

void soft_bus_init(void)
{
    const gpio_config_t output = {
        .pin_bit_mask = 1ULL << CONFIG_NODE_UART_DE, .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&output));
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_NODE_UART_DE, 0));
    const gpio_config_t input = {
        .pin_bit_mask = 1ULL << CONFIG_NODE_UART_RX, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&input));
    soft_uart_rx_init(&decoder, (uint32_t)esp_timer_get_time(),
                      gpio_get_level(CONFIG_NODE_UART_RX));
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3));
    ESP_ERROR_CHECK(gpio_isr_handler_add(CONFIG_NODE_UART_RX, rx_edge, NULL));
    const rmt_tx_channel_config_t tx = {
        .gpio_num = CONFIG_NODE_UART_TX, .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, .mem_block_symbols = 48, .trans_queue_depth = 1,
        .intr_priority = 3, .flags.init_level = 1,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx, &tx_channel));
    const rmt_copy_encoder_config_t copy = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy, &tx_encoder));
    ESP_ERROR_CHECK(rmt_enable(tx_channel));
    /* Establish idle through the RMT matrix itself, with DE still disabled. */
    const rmt_symbol_word_t idle = {.level0 = 1, .duration0 = 1, .level1 = 1, .duration1 = 1};
    const rmt_transmit_config_t idle_tx = {.flags.eot_level = 1};
    ESP_ERROR_CHECK(rmt_transmit(tx_channel, tx_encoder, &idle, sizeof idle, &idle_tx));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(tx_channel, 200));
}

int soft_bus_receive(uint8_t *out, size_t capacity)
{
    for (;;) {
        edge_t edge;
        uint32_t now;
        int lost, available;
        taskENTER_CRITICAL(&ring_lock);
        lost = overflow;
        if (lost) { tail = head; overflow = 0; }
        available = head != tail;
        if (available) { edge = edges[tail]; tail = (tail + 1) % EDGE_COUNT; }
        /* An ISR after this snapshot has a later timestamp. Never advance the
         * decoder beyond an edge which has not yet been consumed. */
        now = (uint32_t)esp_timer_get_time();
        int level = !!(REG_READ(GPIO_IN_REG) & (1u << CONFIG_NODE_UART_RX));
        taskEXIT_CRITICAL(&ring_lock);
        if (lost) soft_uart_rx_lost(&decoder, now, level);
        if (available) soft_uart_rx_edge(&decoder, edge.time, edge.level);
        else soft_uart_rx_advance(&decoder, now);
        int size = soft_uart_rx_take(&decoder, out, capacity);
        if (size) return size;
        if (!available) { vTaskDelay(1); return 0; }
    }
}

void soft_bus_send(const uint8_t *data, size_t length)
{
    ESP_ERROR_CHECK(length && length <= FRAME_MAX ? ESP_OK : ESP_ERR_INVALID_ARG);
    for (size_t byte = 0; byte < length; ++byte) {
        for (unsigned pair = 0; pair < 5; ++pair) {
            unsigned bit = pair * 2;
            symbols[byte * 5 + pair] = (rmt_symbol_word_t){
                .level0 = soft_uart_tx_level(data[byte], bit),
                .duration0 = soft_uart_bit_boundary(bit + 1) - soft_uart_bit_boundary(bit),
                .level1 = soft_uart_tx_level(data[byte], bit + 1),
                .duration1 = soft_uart_bit_boundary(bit + 2) - soft_uart_bit_boundary(bit + 1),
            };
        }
    }
    transmitting = 1;
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_NODE_UART_DE, 1));
    const rmt_transmit_config_t transmit = {.flags.eot_level = 1};
    ESP_ERROR_CHECK(rmt_transmit(tx_channel, tx_encoder, symbols,
                                length * 5 * sizeof *symbols, &transmit));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(tx_channel, 200));
    /* Receiver remains disabled until the complete stop bit has left RMT. */
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_NODE_UART_DE, 0));
    taskENTER_CRITICAL(&ring_lock);
    tail = head;
    overflow = 0;
    uint32_t errors = decoder.errors;
    soft_uart_rx_init(&decoder, (uint32_t)esp_timer_get_time(), 1);
    decoder.errors = errors;
    transmitting = 0;
    taskEXIT_CRITICAL(&ring_lock);
}
uint32_t soft_bus_errors(void) { return decoder.errors; }
#endif

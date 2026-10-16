#include "board.h"
#include "protocol.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "node.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

static QueueHandle_t events;
static adc_oneshot_unit_handle_t adc;
static adc_cali_handle_t calibration;
static uint32_t rx_frames, tx_frames, uart_errors;

void board_init(void)
{
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
    const gpio_config_t button = {
        .pin_bit_mask = 1ULL << CONFIG_NODE_RESET_GPIO, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&button));
    const adc_oneshot_unit_init_cfg_t unit = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit, &adc));
    const adc_oneshot_chan_cfg_t channel = {
        .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12,
    };
    /* ADC1 channel 0 is GPIO0 on ESP32-C3. */
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, ADC_CHANNEL_0, &channel));
    const adc_cali_curve_fitting_config_t cal = {
        .unit_id = ADC_UNIT_1, .chan = ADC_CHANNEL_0,
        .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12,
    };
    /* Missing calibration is reported as unknown mV, never a false 3.3V scale. */
    if (adc_cali_create_scheme_curve_fitting(&cal, &calibration) != ESP_OK)
        calibration = NULL;
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
uint16_t sensor_raw(void)
{
    int raw;
    return adc_oneshot_read(adc, ADC_CHANNEL_0, &raw) == ESP_OK ? raw : 0xffff;
}
uint16_t sensor_millivolts(uint16_t raw)
{
    int mv;
    if (raw == 0xffff || !calibration ||
        adc_cali_raw_to_voltage(calibration, raw, &mv) != ESP_OK || mv < 0)
        return 0xffff;
    return (uint16_t)mv;
}
int bus_receive(uint8_t *out, size_t cap)
{
    static uint32_t last_status;
    const uint32_t now = millis();
    if (now - last_status >= 10000) {
        last_status = now;
        ESP_LOGI("node", "Alive: address=%u rx_frames=%lu tx_frames=%lu uart_errors=%lu",
                 (unsigned)config_address(), (unsigned long)rx_frames,
                 (unsigned long)tx_frames, (unsigned long)uart_errors);
    }
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
}
void bus_send(const uint8_t *data, size_t len)
{
    const int written = uart_write_bytes(UART_NUM_1, data, len);
    ESP_ERROR_CHECK(written == (int)len ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(200)));
    tx_frames++;
}

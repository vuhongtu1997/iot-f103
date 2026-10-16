#include "sensors.h"
#include "sensor_codec.h"
#include "protocol.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static uint16_t sample[DIGITAL_REGISTER_COUNT];
static int64_t updated;
#if CONFIG_NODE_SENSOR_ZE03
static int64_t gas_updated[2], gas_ready;
#endif

static void check_sensor_pin(int pin)
{
    ESP_ERROR_CHECK(GPIO_IS_VALID_GPIO(pin) && pin != CONFIG_NODE_UART_RX &&
                    pin != CONFIG_NODE_UART_TX && pin != CONFIG_NODE_UART_DE &&
                    pin != CONFIG_NODE_RESET_GPIO ? ESP_OK : ESP_ERR_INVALID_ARG);
}

#if CONFIG_NODE_SENSOR_ZE03
#if CONFIG_ESP_CONSOLE_UART
#error "ZE03 uses UART0. Select USB Serial/JTAG or no console in menuconfig."
#endif
static int ze03_read(uart_port_t port, uint8_t expected, uint16_t *value)
{
    const uint8_t mode[] = {0xff, 1, 0x78, 4, 0, 0, 0, 0, 0x83};
    const uint8_t query[] = {0xff, 1, 0x86, 0, 0, 0, 0, 0, 0x79};
    if (uart_write_bytes(port, mode, sizeof mode) != sizeof mode) return 0;
    if (uart_wait_tx_done(port, pdMS_TO_TICKS(100)) != ESP_OK) return 0;
    vTaskDelay(pdMS_TO_TICKS(100));
    if (uart_flush_input(port) != ESP_OK) return 0;
    if (uart_write_bytes(port, query, sizeof query) != sizeof query) return 0;
    if (uart_wait_tx_done(port, pdMS_TO_TICKS(100)) != ESP_OK) return 0;
    uint8_t frame[9];
    size_t used = 0;
    int64_t deadline = esp_timer_get_time() + 400000;
    while (esp_timer_get_time() < deadline) {
        uint8_t byte;
        if (uart_read_bytes(port, &byte, 1, pdMS_TO_TICKS(10)) != 1) continue;
        if (!used && byte != 0xff) continue;
        frame[used++] = byte;
        if (used == sizeof frame) {
            uint8_t gas;
            if (ze03_decode(frame, sizeof frame, &gas, value) && gas == expected) return 1;
            /* Sliding resync handles garbage and fragmented frames. */
            memmove(frame, frame + 1, sizeof frame - 1);
            used--;
        }
    }
    return 0;
}

static void gas_task(void *arg)
{
    unsigned channel = (unsigned)(uintptr_t)arg;
    const uart_port_t ports[] = {UART_NUM_0, UART_NUM_1};
    const uint16_t bits[] = {SENSOR_VALID_H2S, SENSOR_VALID_NH3};
    const unsigned registers[] = {DIGITAL_H2S_REGISTER, DIGITAL_NH3_REGISTER};
    const uint8_t gases[] = {3, 2};
    for (;;) {
        uint16_t value;
        int ok = ze03_read(ports[channel], gases[channel], &value);
        int64_t now = esp_timer_get_time();
        taskENTER_CRITICAL(&lock);
        if (now < gas_ready) sample[DIGITAL_VALID_REGISTER] |= SENSOR_WARMING_UP;
        else sample[DIGITAL_VALID_REGISTER] &= ~SENSOR_WARMING_UP;
        sample[DIGITAL_VALID_REGISTER] &= ~bits[channel];
        if (ok && now >= gas_ready) {
            sample[registers[channel]] = value;
            sample[DIGITAL_VALID_REGISTER] |= bits[channel];
            gas_updated[channel] = now;
        }
        updated = now;
        taskEXIT_CRITICAL(&lock);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

#endif

#if CONFIG_NODE_SENSOR_ZS03
/* Bound every edge wait, including disconnected and stuck-low DATA. */
static int wait_level(int level, unsigned timeout_us)
{
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(CONFIG_NODE_ZS03_GPIO) != level)
        if (esp_timer_get_time() - start >= timeout_us) return 0;
    return 1;
}

static int zs03_read(int16_t *temperature, uint16_t *humidity)
{
    uint8_t data[5] = {0};
    gpio_set_level(CONFIG_NODE_ZS03_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    /* The timing-sensitive capture takes less than 6 ms on this single-core
     * node. Start pulse and task sleep remain outside the critical section. */
    taskENTER_CRITICAL(&lock);
    gpio_set_level(CONFIG_NODE_ZS03_GPIO, 1); /* open drain: release DATA */
    int ok = wait_level(0, 100) && wait_level(1, 120) && wait_level(0, 120);
    for (unsigned bit = 0; ok && bit < 40; ++bit) {
        ok = wait_level(1, 100);
        int64_t start = esp_timer_get_time();
        if (ok) ok = wait_level(0, 120);
        if (ok) data[bit / 8] = (uint8_t)((data[bit / 8] << 1) |
                                      (esp_timer_get_time() - start > 50));
    }
    taskEXIT_CRITICAL(&lock);
    return ok && zs03_decode(data, temperature, humidity);
}

static void sensor_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(2000));
    for (;;) {
        int16_t temperature;
        uint16_t humidity;
        int ok = zs03_read(&temperature, &humidity);
        taskENTER_CRITICAL(&lock);
        sample[DIGITAL_VALID_REGISTER] = ok ? SENSOR_VALID_TEMPERATURE : 0;
        if (ok) {
            sample[DIGITAL_TEMPERATURE_REGISTER] = (uint16_t)temperature;
            /* The manual states humidity is inaccurate below zero. */
            if (temperature >= 0) {
                sample[DIGITAL_HUMIDITY_REGISTER] = humidity;
                sample[DIGITAL_VALID_REGISTER] |= SENSOR_VALID_HUMIDITY;
            }
        }
        updated = esp_timer_get_time();
        taskEXIT_CRITICAL(&lock);
        vTaskDelay(pdMS_TO_TICKS(6000)); /* manual requires >5 seconds */
    }
}
#endif

void sensors_init(void)
{
    memset(sample, 0xff, sizeof sample);
    sample[DIGITAL_SCHEMA_REGISTER] = DIGITAL_SCHEMA_V1;
    sample[DIGITAL_VALID_REGISTER] = 0;
#if CONFIG_NODE_SENSOR_ZS03
    check_sensor_pin(CONFIG_NODE_ZS03_GPIO);
    const gpio_config_t data = {
        .pin_bit_mask = 1ULL << CONFIG_NODE_ZS03_GPIO,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_NODE_ZS03_GPIO, 1));
    ESP_ERROR_CHECK(gpio_config(&data));
    ESP_ERROR_CHECK(xTaskCreate(sensor_task, "zs03", 3072, NULL, 4, NULL) == pdPASS
                    ? ESP_OK : ESP_ERR_NO_MEM);
#elif CONFIG_NODE_SENSOR_WPCH01
    check_sensor_pin(CONFIG_NODE_I2C_SDA);
    check_sensor_pin(CONFIG_NODE_I2C_SCL);
    ESP_ERROR_CHECK(CONFIG_NODE_I2C_SDA != CONFIG_NODE_I2C_SCL ? ESP_OK : ESP_ERR_INVALID_ARG);
    static i2c_master_bus_handle_t bus;
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0, .sda_io_num = CONFIG_NODE_I2C_SDA,
        .scl_io_num = CONFIG_NODE_I2C_SCL, .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7, .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus));
    /* Address-only probes are diagnostic; ACK alone cannot identify a model,
     * frame format, or calibration. Never treat an ACK as a pressure sample. */
    for (unsigned address = 8; address < 0x78; ++address)
        if (i2c_master_probe(bus, address, 10) == ESP_OK)
            ESP_LOGI("sensor", "I2C device ACK at 7-bit address 0x%02x (unidentified)", address);
    sample[DIGITAL_VALID_REGISTER] = SENSOR_PROTOCOL_UNAVAILABLE;
    ESP_LOGW("sensor", "WPCH01: I2C protocol/address/scaling not supplied; pressure unavailable (SDA=%d SCL=%d)",
             CONFIG_NODE_I2C_SDA, CONFIG_NODE_I2C_SCL);
#elif CONFIG_NODE_SENSOR_ZE03
    sample[DIGITAL_VALID_REGISTER] = SENSOR_WARMING_UP;
    gas_ready = esp_timer_get_time() + 300000000LL;
    const int pins[] = {CONFIG_NODE_ZE03_H2S_RX, CONFIG_NODE_ZE03_H2S_TX,
                        CONFIG_NODE_ZE03_NH3_RX, CONFIG_NODE_ZE03_NH3_TX};
    for (unsigned i = 0; i < 4; ++i) {
        check_sensor_pin(pins[i]);
        for (unsigned j = 0; j < i; ++j)
            ESP_ERROR_CHECK(pins[i] != pins[j] ? ESP_OK : ESP_ERR_INVALID_ARG);
    }
    const uart_config_t uart = {
        .baud_rate = 9600, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT,
    };
    const uart_port_t ports[] = {UART_NUM_0, UART_NUM_1};
    for (unsigned channel = 0; channel < 2; ++channel) {
        ESP_ERROR_CHECK(uart_param_config(ports[channel], &uart));
        ESP_ERROR_CHECK(uart_set_pin(ports[channel], pins[channel * 2 + 1],
                                    pins[channel * 2], UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
        ESP_ERROR_CHECK(uart_driver_install(ports[channel], 256, 0, 0, NULL, 0));
        ESP_ERROR_CHECK(xTaskCreate(gas_task, channel ? "ze03_nh3" : "ze03_h2s",
                         3072, (void *)(uintptr_t)channel, 4, NULL) == pdPASS
                         ? ESP_OK : ESP_ERR_NO_MEM);
    }
    ESP_LOGI("sensor", "ZE03 warmup 5 min: H2S UART0 RX=%d TX=%d; NH3 UART1 RX=%d TX=%d",
             pins[0], pins[1], pins[2], pins[3]);
#endif
}

void sensor_registers(uint16_t *registers, int boot_mode)
{
    taskENTER_CRITICAL(&lock);
    memcpy(registers, sample, sizeof sample);
#if CONFIG_NODE_SENSOR_ZE03
    const uint16_t bits[] = {SENSOR_VALID_H2S, SENSOR_VALID_NH3};
    for (unsigned i = 0; i < 2; ++i)
        if (!gas_updated[i] || esp_timer_get_time() - gas_updated[i] > 6000000)
            registers[DIGITAL_VALID_REGISTER] &= ~bits[i];
#endif
    if (boot_mode || !updated || esp_timer_get_time() - updated > 15000000)
        registers[DIGITAL_VALID_REGISTER] &= SENSOR_PROTOCOL_UNAVAILABLE | SENSOR_WARMING_UP;
    taskEXIT_CRITICAL(&lock);
}

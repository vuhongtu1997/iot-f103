#include "board.h"
#include "node.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "sdkconfig.h"

void app_main(void)
{
    ESP_LOGI("node", "Starting ESP32-C3 RS485 sensor node");
    /* Fail rather than silently erase assigned addresses on an NVS error. */
    ESP_ERROR_CHECK(nvs_flash_init());
    board_init();
    const uint8_t address = config_address();
    ESP_LOGI("node", "Ready: address=%u sensor_type=%d UART1 TX=GPIO%d RX=GPIO%d baud=%d",
             (unsigned)address, CONFIG_NODE_SENSOR_TYPE,
             CONFIG_NODE_UART_TX, CONFIG_NODE_UART_RX, BUS_BAUD);
    if (!address)
        ESP_LOGI("node", "Unconfigured: waiting for master assign_address; scan does not assign an address");
    node_run(0);
}

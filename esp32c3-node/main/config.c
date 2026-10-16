#include "node.h"
#include "nvs.h"
#include "esp_err.h"
#include "esp_log.h"

uint8_t config_address(void)
{
    nvs_handle_t handle;
    uint8_t address = 0;
    /* Missing key is unconfigured; storage errors must not unlock a node. */
    ESP_ERROR_CHECK(nvs_open("node", NVS_READWRITE, &handle));
    esp_err_t err = nvs_get_u8(handle, "address", &address);
    nvs_close(handle);
    if (err != ESP_ERR_NVS_NOT_FOUND)
        ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(address <= MAX_NODES ? ESP_OK : ESP_ERR_INVALID_STATE);
    return address;
}

int config_set_address(uint8_t address)
{
    if (!address || address > MAX_NODES)
        return 0;
    uint8_t current = config_address();
    if (current)
        return current == address;
    nvs_handle_t handle;
    if (nvs_open("node", NVS_READWRITE, &handle) != ESP_OK)
        return 0;
    esp_err_t err = nvs_set_u8(handle, "address", address);
    if (err == ESP_OK)
        err = nvs_commit(handle);
    nvs_close(handle);
    if (err == ESP_OK)
        ESP_LOGI("node", "Modbus address saved: %u", (unsigned)address);
    return err == ESP_OK;
}

int config_factory_reset(void)
{
    nvs_handle_t handle;
    if (nvs_open("node", NVS_READWRITE, &handle) != ESP_OK)
        return 0;
    esp_err_t err = nvs_erase_key(handle, "address");
    if (err == ESP_ERR_NVS_NOT_FOUND)
        err = ESP_OK;
    if (err == ESP_OK)
        err = nvs_commit(handle);
    nvs_close(handle);
    if (err == ESP_OK)
        ESP_LOGI("node", "Modbus address cleared");
    return err == ESP_OK;
}

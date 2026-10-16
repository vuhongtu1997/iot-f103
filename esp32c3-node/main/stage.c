/* FC42 transport retained; native IDF OTA replaces STM32 vectors and SPI NOR. */
#include "node.h"
#include "esp_ota_ops.h"
#include <string.h>

static esp_ota_handle_t handle;
static const esp_partition_t *partition;
static uint32_t received, length, expected_crc, checksum;
static int uploading;

uint32_t stage_received(void) { return received; }
int stage_begin(const uint8_t h[16])
{
    if (uploading)
        esp_ota_abort(handle);
    uploading = 0;
    received = 0;
    partition = esp_ota_get_next_update_partition(NULL);
    length = get32(h + 4);
    if (get32(h) != MODEL_ESP32C3_NODE || !partition ||
        length < 32 || length > partition->size)
        return 0;
    if (esp_ota_begin(partition, length, &handle) != ESP_OK)
        return 0;
    expected_crc = get32(h + 8);
    checksum = 0xffffffff;
    uploading = 1;
    return 1;
}
int stage_data(uint32_t off, const uint8_t *data, size_t size)
{
    if (!uploading || !size || size > CHUNK_SIZE || off > length || size > length - off)
        return 0;
    if (off < received) {
        uint8_t check[CHUNK_SIZE];
        return size <= received - off &&
            esp_partition_read(partition, off, check, size) == ESP_OK &&
            constant_equal(check, data, size);
    }
    if (off != received)
        return 0;
    if (esp_ota_write(handle, data, size) != ESP_OK) {
        esp_ota_abort(handle);
        uploading = 0;
        return 0;
    }
    checksum = crc32_update(checksum, data, size);
    received += size;
    return 1;
}
int stage_commit(void)
{
    if (!uploading || received != length || (checksum ^ 0xffffffff) != expected_crc)
        return 0;
    uploading = 0;
    if (esp_ota_end(handle) != ESP_OK)
        return 0;
    return esp_ota_set_boot_partition(partition) == ESP_OK;
}

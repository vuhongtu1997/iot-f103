#include "gateway.h"
#include "secrets.h"
#include "sha256.h"
#include "ota_key.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
static const char *str(const cJSON *j, const char *name)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, name);
    return cJSON_IsString(v) ? v->valuestring : 0;
}
static int u32(const cJSON *j, const char *name, uint32_t *out)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, name);
    if (!cJSON_IsNumber(v) || v->valuedouble < 0 || v->valuedouble > 4294967295.0 || floor(v->valuedouble) != v->valuedouble)
        return 0;
    *out = (uint32_t)v->valuedouble;
    return 1;
}
static int download(const char *url, const uint8_t header[48], int esp_target, esp_ota_handle_t *ota)
{
    uint32_t length = get32(header + 4);
    size_t origin = strlen(OTA_ORIGIN);
    if (strncmp(url, OTA_ORIGIN, origin) || url[origin] != '/' || strncmp(OTA_ORIGIN, "https://", 8))
        return 0;
    esp_http_client_config_t cfg = {.url = url, .timeout_ms = 15000, .disable_auto_redirect = true, .buffer_size = 1024};
    if (CLOUD_CA_PEM[0])
        cfg.cert_pem = CLOUD_CA_PEM;
    else
        cfg.crt_bundle_attach = esp_crt_bundle_attach;
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client)
        return 0;
    FILE *file = 0;
    int ok = 0;
    *ota = 0;
    if (esp_http_client_open(client, 0) != ESP_OK)
        goto cleanup;
    int64_t remote = esp_http_client_fetch_headers(client);
    if (esp_http_client_get_status_code(client) != 200 || remote != (int64_t)length)
        goto cleanup;
    if (esp_target)
    {
        const esp_partition_t *part = esp_ota_get_next_update_partition(0);
        if (!part || length > part->size || esp_ota_begin(part, length, ota) != ESP_OK)
            goto cleanup;
    }
    else
    {
        file = fopen("/spiffs/node.bin", "wb");
        if (!file)
            goto cleanup;
    }
    hmac_ctx ctx;
    hmac_init(&ctx, OTA_KEY, 32);
    hmac_update(&ctx, header, 16);
    uint32_t c = 0xffffffff, total = 0;
    uint8_t b[1024], mac[32];
    while (total < length)
    {
        uint32_t want = length - total;
        if (want > sizeof b)
            want = sizeof b;
        int got = esp_http_client_read(client, (char *)b, (int)want);
        if (got <= 0)
            goto cleanup;
        c = crc32_update(c, b, (size_t)got);
        hmac_update(&ctx, b, (size_t)got);
        if (esp_target)
        {
            if (esp_ota_write(*ota, b, (size_t)got) != ESP_OK)
                goto cleanup;
        }
        else if (fwrite(b, 1, (size_t)got, file) != (size_t)got)
            goto cleanup;
        total += (uint32_t)got;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    hmac_final(&ctx, mac);
    ok = (c ^ 0xffffffff) == get32(header + 8) && constant_equal(mac, header + 16, 32);
    if (!esp_http_client_is_complete_data_received(client))
        ok = 0;
cleanup:
    if (file && fclose(file) != 0)
        ok = 0;
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (!ok && *ota)
    {
        esp_ota_abort(*ota);
        *ota = 0;
    }
    if (!ok && !esp_target)
        remove("/spiffs/node.bin");
    return ok;
}
int run_ota(const cJSON *j)
{
    const char *job = str(j, "job_id"), *target = str(j, "target"), *url = str(j, "url"), *mac = str(j, "hmac"), *uid = str(j, "uid");
    uint32_t size, crc, version;
    uint8_t header[48];
    if (!job || strlen(job) > 63 || !target || !url || strlen(url) > 511 || !mac || !u32(j, "size", &size) || !u32(j, "crc32", &crc) || !u32(j, "version", &version))
        return 0;
    int is_esp = strcmp(target, "esp32") == 0;
    if (!is_esp && strcmp(target, "stm32"))
        return 0;
    if (size < 8 || size > (is_esp ? 0x180000u : APP_MAX) || !parse_hex(mac, header + 16, 32))
        return 0;
    put32(header, is_esp ? MODEL_ESP32 : MODEL_STM32);
    put32(header + 4, size);
    put32(header + 8, crc);
    put32(header + 12, version);
    node_info *node = 0;
    uint8_t id[12];
    if (!is_esp)
    {
        if (!parse_hex(uid, id, 12))
            return 0;
        for (unsigned i = 0; i < node_count; i++)
            if (constant_equal(nodes[i].uid, id, 12))
            {
                node = &nodes[i];
                break;
            }
        uint16_t regs[12];
        if (!node || !read_node(node, regs))
            return 0;
    }
    publish_status(job, "downloading", "HTTPS download and HMAC verification");
    esp_ota_handle_t ota;
    if (!download(url, header, is_esp, &ota))
    {
        publish_status(job, "failed", "download, length, CRC or HMAC check failed");
        return 0;
    }
    if (is_esp)
    {
        const esp_partition_t *part = esp_ota_get_next_update_partition(0);
        if (esp_ota_end(ota) != ESP_OK || esp_ota_set_boot_partition(part) != ESP_OK)
        {
            publish_status(job, "failed", "ESP OTA activation failed");
            return 0;
        }
        publish_status(job, "rebooting", "new image selected; await boot status");
        vTaskDelay(pdMS_TO_TICKS(1500));
        esp_restart();
        return 1;
    }
    FILE *file = fopen("/spiffs/node.bin", "rb");
    if (!file)
        return 0;
    uint32_t off = 0, ack = 0;
    int ok = node_ota_op(node, 1, header, sizeof header, &ack);
    uint8_t payload[5 + CHUNK_SIZE];
    publish_status(job, "transferring", "RS485 transfer; polling paused");
    while (ok && off < size)
    {
        uint32_t n = size - off;
        if (n > CHUNK_SIZE)
            n = CHUNK_SIZE;
        put32(payload, off);
        payload[4] = (uint8_t)n;
        if (fread(payload + 5, 1, n, file) != n)
        {
            ok = 0;
            break;
        }
        ok = node_ota_op(node, 2, payload, n + 5, &ack) && ack == off + n;
        off += n;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    fclose(file);
    remove("/spiffs/node.bin");
    if (ok)
        ok = node_ota_op(node, 3, 0, 0, &ack);
    if (!ok)
    {
        publish_status(job, "failed", "STM32 begin, transfer or verification failed; retry entire job");
        return 0;
    }
    /* A lost commit ACK may mean installation already started. It is reported as
     * failed above; rerunning the signed job is safe. Never claim completion early. */
    publish_status(job, "installing", "bootloader copying staged image");
    int64_t deadline = esp_timer_get_time() + 60000000;
    uint16_t regs[12];
    while (esp_timer_get_time() < deadline)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        if (read_node(node, regs) && regs[4] == 0 && (((uint32_t)regs[2] << 16) | regs[3]) == version)
        {
            node->version = version;
            node->mode = 0;
            publish_status(job, "completed", "UID and running application version verified");
            return 1;
        }
    }
    publish_status(job, "failed", "new STM32 application did not confirm expected version");
    return 0;
}

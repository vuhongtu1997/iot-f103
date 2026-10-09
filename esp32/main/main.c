#include "gateway.h"
#include "secrets.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_crt_bundle.h"
#include "esp_ota_ops.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
static const char *TAG = "iot";
static esp_mqtt_client_handle_t mqtt;
static EventGroupHandle_t net_events;
static QueueHandle_t commands, telemetry;
static char command_topic[128], status_topic[128], data_topic[128], ota_topic[128];
static uint32_t boot_number, sequence, dropped;
static int startup_discovery = 1;
typedef struct
{
  char text[1024];
} command;
typedef struct
{
  char text[384];
} sample;
void uid_hex(const uint8_t *uid, char out[25])
{
  for (unsigned i = 0; i < 12; i++)
    sprintf(out + 2 * i, "%02x", uid[i]);
  out[24] = 0;
}
int parse_hex(const char *s, uint8_t *out, size_t n)
{
  if (!s || strlen(s) != 2 * n)
    return 0;
  for (size_t i = 0; i < n; i++)
  {
    unsigned v = 0;
    for (unsigned j = 0; j < 2; j++)
    {
      char c = s[2 * i + j];
      unsigned d;
      if (c >= '0' && c <= '9')
        d = (unsigned)(c - '0');
      else if (c >= 'a' && c <= 'f')
        d = (unsigned)(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F')
        d = (unsigned)(c - 'A' + 10);
      else
        return 0;
      v = v * 16 + d;
    }
    out[i] = (uint8_t)v;
  }
  return 1;
}
void publish_status(const char *job, const char *state, const char *detail)
{
  cJSON *j = cJSON_CreateObject();
  if (!j)
    return;
  cJSON_AddStringToObject(j, "gateway_id", GATEWAY_ID);
  cJSON_AddStringToObject(j, "job_id", job ? job : "");
  cJSON_AddStringToObject(j, "state", state);
  cJSON_AddStringToObject(j, "detail", detail);
  cJSON_AddNumberToObject(j, "node_count", node_count);
  char *text = cJSON_PrintUnformatted(j);
  if (text)
  {
    ESP_LOGI(TAG, "%s", text);
    if (mqtt && (xEventGroupGetBits(net_events) & 2))
      esp_mqtt_client_enqueue(mqtt, ota_topic, text, 0, 1, 0, true);
    free(text);
  }
  cJSON_Delete(j);
}
static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
  (void)arg;
  (void)data;
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
    esp_wifi_connect();
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
  {
    xEventGroupClearBits(net_events, 1);
    esp_wifi_connect();
  }
  if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    xEventGroupSetBits(net_events, 1);
}
static void mqtt_event(void *arg, esp_event_base_t base, int32_t id, void *event)
{
  (void)arg;
  (void)base;
  esp_mqtt_event_handle_t e = event;
  static command pending;
  static int receiving, total, offset;
  if (id == MQTT_EVENT_CONNECTED)
  {
    xEventGroupSetBits(net_events, 2);
    esp_mqtt_client_subscribe(mqtt, command_topic, 1);
    char boot[192];
    snprintf(boot, sizeof boot, "{\"online\":true,\"boot\":%lu,\"firmware\":\"%s\"}", (unsigned long)boot_number, esp_app_get_description()->version);
    esp_mqtt_client_publish(mqtt, status_topic, boot, 0, 1, 1);
  }
  else if (id == MQTT_EVENT_DISCONNECTED)
  {
    xEventGroupClearBits(net_events, 2);
    receiving = 0;
  }
  else if (id == MQTT_EVENT_DATA)
  {
    if (e->current_data_offset == 0)
    {
      receiving = !e->retain && e->topic_len == (int)strlen(command_topic) && !memcmp(e->topic, command_topic, (size_t)e->topic_len) && e->total_data_len > 0 && e->total_data_len < (int)sizeof pending.text;
      total = e->total_data_len;
      offset = 0;
    }
    if (!receiving)
      return;
    if (e->current_data_offset != offset || e->data_len < 0 || e->data_len > total - offset)
    {
      receiving = 0;
      return;
    }
    memcpy(pending.text + offset, e->data, (size_t)e->data_len);
    offset += e->data_len;
    if (offset == total)
    {
      pending.text[total] = 0;
      receiving = 0;
      if (xQueueSend(commands, &pending, 0) != pdTRUE)
        ESP_LOGW(TAG, "command queue full; command rejected");
    }
  }
}
static int remember_job(const char *job)
{
  /* Persist before execution: QoS1 duplicates and ESP reboot must not rerun jobs.
   * A retry requires a NEW job_id, including after a failed operation. */
  if (!job || !job[0] || strlen(job) > 63)
    return 0;
  char history[8][64] = {{0}};
  size_t n = sizeof history;
  nvs_handle_t h;
  if (nvs_open("iot", NVS_READWRITE, &h) != ESP_OK)
    return -1;
  esp_err_t err = nvs_get_blob(h, "jobs", history, &n);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
  {
    nvs_close(h);
    return -1;
  }
  for (unsigned i = 0; i < 8; i++)
    if (!strcmp(history[i], job))
    {
      nvs_close(h);
      return 0;
    }
  memmove(history + 1, history, 7 * 64);
  memset(history[0], 0, 64);
  strcpy(history[0], job);
  err = nvs_set_blob(h, "jobs", history, sizeof history);
  if (err == ESP_OK)
    err = nvs_commit(h);
  nvs_close(h);
  return err == ESP_OK ? 1 : -1;
}
int persist_registry(void)
{
  nvs_handle_t h;
  if (nvs_open("iot", NVS_READWRITE, &h) != ESP_OK)
    return 0;
  esp_err_t e = nvs_set_blob(h, "nodes_v2", nodes, node_count * sizeof *nodes);
  if (e == ESP_OK)
    e = nvs_commit(h);
  nvs_close(h);
  return e == ESP_OK;
}
static void load_registry(void)
{
  nvs_handle_t h;
  ESP_ERROR_CHECK(nvs_open("iot", NVS_READWRITE, &h));
  size_t bytes = sizeof nodes;
  esp_err_t e = nvs_get_blob(h, "nodes_v2", nodes, &bytes);
  nvs_close(h);
  if (e == ESP_ERR_NVS_NOT_FOUND)
  {
    node_count = 0;
    return;
  }
  ESP_ERROR_CHECK(e);
  if (bytes % sizeof *nodes)
    abort();
  node_count = (unsigned)(bytes / sizeof *nodes);
  for (unsigned i = 0; i < node_count; i++)
  {
    if (!nodes[i].address || nodes[i].address > MAX_NODES)
      abort();
    for (unsigned k = 0; k < i; k++)
      if (nodes[k].address == nodes[i].address)
        abort();
  }
}
static void publish_registry(void)
{
  for (unsigned i = 0; i < node_count; i++)
  {
    char uid[25];
    uid_hex(nodes[i].uid, uid);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "gateway_id", GATEWAY_ID);
    cJSON_AddStringToObject(j, "event", "node");
    cJSON_AddStringToObject(j, "uid", uid);
    cJSON_AddNumberToObject(j, "address", nodes[i].address);
    cJSON_AddNumberToObject(j, "version", nodes[i].version);
    cJSON_AddNumberToObject(j, "bootloader", nodes[i].mode);
    char *s = cJSON_PrintUnformatted(j);
    if (s)
    {
      if (mqtt && (xEventGroupGetBits(net_events) & 2))
        esp_mqtt_client_enqueue(mqtt, ota_topic, s, 0, 1, 0, true);
      free(s);
    }
    cJSON_Delete(j);
  }
}
static void poll_nodes(void)
{
  for (unsigned i = 0; i < node_count; i++)
  {
    uint16_t r[12];
    int ok = read_node(nodes + i, r);
    char uid[25];
    uid_hex(nodes[i].uid, uid);
    time_t now = time(0);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "gateway_id", GATEWAY_ID);
    cJSON_AddStringToObject(j, "uid", uid);
    cJSON_AddNumberToObject(j, "address", nodes[i].address);
    cJSON_AddNumberToObject(j, "boot", boot_number);
    cJSON_AddNumberToObject(j, "sequence", ++sequence);
    cJSON_AddNumberToObject(j, "uptime_ms", (double)(xTaskGetTickCount() * portTICK_PERIOD_MS));
    if (now > 1700000000)
      cJSON_AddNumberToObject(j, "timestamp", (double)now);
    else
      cJSON_AddNullToObject(j, "timestamp");
    cJSON_AddStringToObject(j, "quality", !ok ? "offline" : r[4]         ? "bootloader"
                                                        : r[0] == 0xffff ? "sensor_error"
                                                                         : "ok");
    if (ok && !r[4] && r[0] != 0xffff)
    {
      cJSON_AddNumberToObject(j, "adc_raw", r[0]);
      cJSON_AddNumberToObject(j, "millivolts", r[1]);
    }
    cJSON_AddNumberToObject(j, "dropped_samples", dropped);
    char *s = cJSON_PrintUnformatted(j);
    if (s)
    {
      sample entry;
      int len = snprintf(entry.text, sizeof entry.text, "%s", s);
      if (len >= 0 && len < (int)sizeof entry.text)
      {
        if (xQueueSend(telemetry, &entry, 0) != pdTRUE)
        {
          sample old;
          xQueueReceive(telemetry, &old, 0);
          dropped++;
          xQueueSend(telemetry, &entry, 0);
        }
      }
      else
        dropped++;
      free(s);
    }
    cJSON_Delete(j);
  }
}
static void drain_samples(void)
{
  if (!mqtt || !(xEventGroupGetBits(net_events) & 2))
    return;
  sample entry;
  for (unsigned i = 0; i < 8 && xQueuePeek(telemetry, &entry, 0) == pdTRUE; i++)
  {
    int id = esp_mqtt_client_enqueue(mqtt, data_topic, entry.text, 0, 1, 0, true);
    if (id < 0)
      break;
    xQueueReceive(telemetry, &entry, 0);
  }
}
static void worker(void *arg)
{
  (void)arg;
  TickType_t last = 0;
  for (;;)
  {
    if (startup_discovery)
    {
      startup_discovery = 0;
      publish_status("startup", "scanning", "read-only address scan");
      int ok = discover_nodes() && persist_registry();
      publish_status("startup", ok ? "scanned" : "failed", ok ? "registry updated; no addresses assigned" : "scan failed; registry reservations retained");
    }
    command cmd;
    if (xQueueReceive(commands, &cmd, pdMS_TO_TICKS(20)) == pdTRUE)
    {
      cJSON *j = cJSON_Parse(cmd.text);
      const cJSON *op = j ? cJSON_GetObjectItemCaseSensitive(j, "op") : 0, *job = j ? cJSON_GetObjectItemCaseSensitive(j, "job_id") : 0;
      if (!cJSON_IsString(op) || !cJSON_IsString(job))
      {
        publish_status("", "rejected", "op and job_id required");
        cJSON_Delete(j);
        continue;
      }
      int accepted = remember_job(job->valuestring);
      if (accepted != 1)
      {
        publish_status(job->valuestring, accepted == 0 ? "duplicate" : "rejected", accepted == 0 ? "use new job_id for retry" : "NVS failure");
        cJSON_Delete(j);
        continue;
      }
      if (!strcmp(op->valuestring, "scan") || !strcmp(op->valuestring, "discover"))
      {
        publish_status(job->valuestring, "scanning", "read-only scan; polling paused");
        int ok = discover_nodes() && persist_registry();
        publish_status(job->valuestring, ok ? "completed" : "failed", ok ? "registry updated; no addresses assigned" : "scan or persistence failed");
        if (ok)
          publish_registry();
      }
      else if (!strcmp(op->valuestring, "assign_address"))
      {
        const cJSON *a = cJSON_GetObjectItemCaseSensitive(j, "address");
        if (!cJSON_IsNumber(a) || a->valuedouble < 1 || a->valuedouble > MAX_NODES || a->valuedouble != a->valueint)
        {
          publish_status(job->valuestring, "rejected", "address must be integer 1..247");
        }
        else
        {
          node_info assigned;
          publish_status(job->valuestring, "assigning", "one unconfigured node must be powered; FC06 broadcast");
          int result = assign_address((uint8_t)a->valueint, &assigned);
          if (result == 1)
          {
            char uid[25], detail[128];
            uid_hex(assigned.uid, uid);
            snprintf(detail, sizeof detail, "address=%u uid=%s saved and verified", assigned.address, uid);
            publish_status(job->valuestring, "completed", detail);
            publish_registry();
          }
          else
            publish_status(job->valuestring, "failed", result == -1 ? "address occupied or reserved" : result == -2 ? "bus activity makes free-address check uncertain"
                                                                                                   : result == -5   ? "NVS failure; address may remain reserved"
                                                                                                                    : "not confirmed; do not resend broadcast; run scan to recover reservation");
        }
      }
      else if (!strcmp(op->valuestring, "ota"))
      {
        if (!run_ota(j))
          publish_status(job->valuestring, "failed", "see previous detail or invalid OTA command");
      }
      else
        publish_status(job->valuestring, "rejected", "unknown operation");
      cJSON_Delete(j);
    }
    if (xTaskGetTickCount() - last >= pdMS_TO_TICKS(POLL_INTERVAL_MS))
    {
      poll_nodes();
      last = xTaskGetTickCount();
    }
    drain_samples();
  }
}
void app_main(void)
{
  if (!strcmp(WIFI_SSID, "CHANGE_ME"))
  {
    ESP_LOGE(TAG, "configure main/secrets.h before flashing");
    return;
  }
  /* Do not erase NVS automatically on errors: dedup and boot IDs are durable. */
  ESP_ERROR_CHECK(nvs_flash_init());
  nvs_handle_t h;
  ESP_ERROR_CHECK(nvs_open("iot", NVS_READWRITE, &h));
  nvs_get_u32(h, "boot", &boot_number);
  boot_number++;
  ESP_ERROR_CHECK(nvs_set_u32(h, "boot", boot_number));
  ESP_ERROR_CHECK(nvs_commit(h));
  nvs_close(h);
  net_events = xEventGroupCreate();
  commands = xQueueCreate(8, sizeof(command));
  telemetry = xQueueCreate(64, sizeof(sample));
  if (!net_events || !commands || !telemetry)
    abort();
  snprintf(command_topic, sizeof command_topic, "iot/%s/commands", GATEWAY_ID);
  snprintf(status_topic, sizeof status_topic, "iot/%s/status", GATEWAY_ID);
  snprintf(data_topic, sizeof data_topic, "iot/%s/telemetry", GATEWAY_ID);
  snprintf(ota_topic, sizeof ota_topic, "iot/%s/ota/status", GATEWAY_ID);
  esp_vfs_spiffs_conf_t fs = {.base_path = "/spiffs", .partition_label = "storage", .max_files = 2, .format_if_mount_failed = true};
  ESP_ERROR_CHECK(esp_vfs_spiffs_register(&fs));
  bus_init();
  load_registry();
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();
  wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&init));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, 0));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, 0));
  wifi_config_t wifi = {0};
  if (strlen(WIFI_SSID) >= sizeof wifi.sta.ssid || strlen(WIFI_PASSWORD) >= sizeof wifi.sta.password)
    abort();
  strcpy((char *)wifi.sta.ssid, WIFI_SSID);
  strcpy((char *)wifi.sta.password, WIFI_PASSWORD);
  wifi.sta.pmf_cfg.capable = true;
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi));
  ESP_ERROR_CHECK(esp_wifi_start());
  if (xTaskCreate(worker, "iot_worker", 16384, 0, 5, 0) != pdPASS)
    abort();
  esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  ESP_ERROR_CHECK(esp_netif_sntp_init(&sntp));
  esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
  esp_ota_get_state_partition(esp_ota_get_running_partition(), &state);
  TickType_t start = xTaskGetTickCount();
  int confirmed = state != ESP_OTA_IMG_PENDING_VERIFY;
  for (;;)
  {
    time_t now = time(0);
    if (!mqtt && (xEventGroupGetBits(net_events) & 1) && now > 1700000000)
    {
      if (strncmp(MQTT_URI, "mqtts://", 8))
      {
        ESP_LOGE(TAG, "MQTT TLS required");
        abort();
      }
      esp_mqtt_client_config_t cfg = {.broker.address.uri = MQTT_URI, .credentials.client_id = GATEWAY_ID, .credentials.username = MQTT_USERNAME, .credentials.authentication.password = MQTT_PASSWORD, .session.keepalive = 30, .session.last_will.topic = status_topic, .session.last_will.msg = "{\"online\":false}", .session.last_will.qos = 1, .session.last_will.retain = 1, .buffer.size = 2048, .outbox.limit = 32768};
      if (CLOUD_CA_PEM[0])
        cfg.broker.verification.certificate = CLOUD_CA_PEM;
      else
        cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
      mqtt = esp_mqtt_client_init(&cfg);
      if (!mqtt)
        abort();
      ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt, ESP_EVENT_ANY_ID, mqtt_event, 0));
      ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt));
    }
    if (!confirmed && (xEventGroupGetBits(net_events) & 2))
    {
      ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
      confirmed = 1;
      publish_status("boot", "confirmed", "local initialization and MQTT TLS connection passed");
    }
    if (!confirmed && xTaskGetTickCount() - start > pdMS_TO_TICKS(180000))
    {
      ESP_LOGE(TAG, "new image did not reach MQTT within 180s: rollback");
      esp_ota_mark_app_invalid_rollback_and_reboot();
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

#include "Wifi.h"

#include "Config.h"
#include "Led.h"
#include "Log.h"
#include <stdio.h>
#include <string>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>

#include "esp_mac.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/inet.h"
#include "lwip/sys.h"
#include <atomic>
#include <vector>

#define TAG "WIFI"
static uint32_t retry_count = 0;

using namespace std;

static bool isWaitConnect = false;
static atomic<bool> networkResetInProgress(false);
static atomic<bool> suppressWifiReconnect(false);
static TaskHandle_t networkMonitorTaskHandle = nullptr;
static atomic<uint32_t> networkRevision(0);
static atomic<int> lastWifiDisconnectReason(0);

const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;

enum NetworkType
{
  NETWORK_NONE,
  NETWORK_WIFI
};

static bool is_valid_wifi_ssid(const char *ssid)
{
  return ssid && ssid[0] != '\0';
}

static bool is_wifi_connected()
{
  return wifi_event_group &&
         (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_EVENT) != 0;
}

static NetworkType get_network_type()
{
  if (is_wifi_connected())
  {
    return NETWORK_WIFI;
  }
  return NETWORK_NONE;
}

static bool apply_sta_config_and_start(wifi_config_t wifi_config)
{
  if (!is_valid_wifi_ssid(reinterpret_cast<char *>(wifi_config.sta.ssid)))
  {
    LOGW("No valid WiFi SSID configured");
    isWaitConnect = false;
    return false;
  }

  retry_count = 0;
  isWaitConnect = true;
  lastWifiDisconnectReason.store(0);
  suppressWifiReconnect.store(true);
  xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_EVENT);

  esp_err_t err = esp_wifi_disconnect();
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED &&
      err != ESP_ERR_WIFI_CONN && err != ESP_FAIL)
  {
    LOGE("esp_wifi_disconnect failed: %s", esp_err_to_name(err));
    suppressWifiReconnect.store(false);
    isWaitConnect = false;
    return false;
  }
  if (err == ESP_FAIL)
  {
    LOGD("No active STA connection to disconnect");
  }

  // WIFI_EVENT_STA_DISCONNECTED is asynchronous. Give the driver time to
  // leave the connecting state before replacing the STA configuration.
  vTaskDelay(pdMS_TO_TICKS(100));

  err = esp_wifi_set_mode(WIFI_MODE_STA);
  if (err != ESP_OK)
  {
    LOGE("esp_wifi_set_mode failed: %s", esp_err_to_name(err));
    suppressWifiReconnect.store(false);
    isWaitConnect = false;
    return false;
  }

  for (int attempt = 0; attempt < 5; attempt++)
  {
    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_ERR_WIFI_STATE)
      break;
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  if (err != ESP_OK)
  {
    LOGE("esp_wifi_set_config failed: %s", esp_err_to_name(err));
    suppressWifiReconnect.store(false);
    isWaitConnect = false;
    return false;
  }

  err = esp_wifi_start();
  if (err != ESP_OK)
  {
    LOGE("esp_wifi_start failed: %s", esp_err_to_name(err));
    suppressWifiReconnect.store(false);
    isWaitConnect = false;
    return false;
  }

  suppressWifiReconnect.store(false);
  err = esp_wifi_connect();
  if (err != ESP_OK && err != ESP_ERR_WIFI_CONN)
  {
    LOGE("esp_wifi_connect failed: %s", esp_err_to_name(err));
    isWaitConnect = false;
    return false;
  }

  return true;
}

static bool start_configured_wifi()
{
  wifi_config_t wifi_config = {};
  esp_err_t ret = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
  if (ret == ESP_OK &&
      is_valid_wifi_ssid(reinterpret_cast<char *>(wifi_config.sta.ssid)))
  {
    LOGI("WiFi config found in NVS");
    LOGI("SSID: %s", wifi_config.sta.ssid);
    return apply_sta_config_and_start(wifi_config);
  }

  const string storedSsid = Config::getInstance()->GetWifiSsid();
  const string storedPassword = Config::getInstance()->GetWifiPassword();
  if (!is_valid_wifi_ssid(storedSsid.c_str()))
  {
    LOGW("No WiFi fallback configured");
    isWaitConnect = false;
    return false;
  }

  LOGI("WiFi config found in app NVS");
  LOGI("SSID: %s", storedSsid.c_str());
  strncpy(reinterpret_cast<char *>(wifi_config.sta.ssid), storedSsid.c_str(),
          sizeof(wifi_config.sta.ssid) - 1);
  strncpy(reinterpret_cast<char *>(wifi_config.sta.password),
          storedPassword.c_str(), sizeof(wifi_config.sta.password) - 1);
  if (!storedPassword.empty())
  {
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  }

  return apply_sta_config_and_start(wifi_config);
}

static void network_monitor_task(void *)
{
  constexpr TickType_t RETRY_INTERVAL = pdMS_TO_TICKS(30000);
  TickType_t lastWifiRetry = 0;
  NetworkType previousType = get_network_type();

  while (true)
  {
    if (networkResetInProgress.load())
    {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    TickType_t now = xTaskGetTickCount();

    NetworkType currentType = get_network_type();
    if (currentType != previousType)
    {
      networkRevision.fetch_add(1);
      LOGI("Network interface changed: %d -> %d", previousType, currentType);
      previousType = currentType;
    }

    if (currentType == NETWORK_NONE)
    {
      if (lastWifiRetry == 0 || now - lastWifiRetry >= RETRY_INTERVAL)
      {
        lastWifiRetry = now;
        LOGI("Network retry: starting configured WiFi fallback");
        suppressWifiReconnect.store(false);
        start_configured_wifi();
      }
    }
    else
    {
      lastWifiRetry = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

static void start_network_monitor()
{
  if (!networkMonitorTaskHandle)
  {
    xTaskCreate(network_monitor_task, "NetworkMonitor", 4096, nullptr, 6,
                &networkMonitorTaskHandle);
  }
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT)
  {
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
    {
      wifi_config_t sta_cfg = {0};
      ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &sta_cfg));
      ESP_LOGW(TAG, "SSID: %s", sta_cfg.sta.ssid);
      if (suppressWifiReconnect.load())
      {
        LOGI("Skip automatic STA connect while updating WiFi configuration");
        break;
      }
      esp_wifi_connect();
      break;
    }
    case WIFI_EVENT_STA_DISCONNECTED:
    {
      wifi_event_sta_disconnected_t *disconnected =
          static_cast<wifi_event_sta_disconnected_t *>(event_data);
      if (disconnected)
      {
        lastWifiDisconnectReason.store(disconnected->reason);
        ESP_LOGW(TAG, "WiFi disconnected, reason=%u", disconnected->reason);
      }
      xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_EVENT);
      SetLedInternetConnected(false);

      if (suppressWifiReconnect.load())
      {
        LOGI("WiFi stopped for an intentional network switch");
        break;
      }

      retry_count++;
      ESP_LOGW(TAG, "Retry STA connect (attempt %lu)",
               static_cast<unsigned long>(retry_count));
      esp_err_t connectErr = esp_wifi_connect();
      if (connectErr != ESP_OK && connectErr != ESP_ERR_WIFI_CONN)
      {
        ESP_LOGE(TAG, "Retry STA connect failed to start: %s",
                 esp_err_to_name(connectErr));
      }
      break;
    }
    default:
      break;
    }
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Connected with IP Address:" IPSTR,
             IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
    lastWifiDisconnectReason.store(0);
    retry_count = 0;
    SetLedInternetConnected(true);
  }
}

bool Wifi::ConnectToWifi(const std::string &ssid, const std::string &pass)
{
  if (!is_valid_wifi_ssid(ssid.c_str()))
  {
    LOGW("ConnectToWifi: invalid ssid");
    return false;
  }

  wifi_config_t wifi_config = {};
  memcpy(wifi_config.sta.ssid, ssid.c_str(), ssid.length());
  memcpy(wifi_config.sta.password, pass.c_str(), pass.length());

  retry_count = 0;
  isWaitConnect = true;
  suppressWifiReconnect.store(false);

  esp_wifi_disconnect();
  esp_wifi_stop();

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_connect());

  LOGI("ConnectToWifi: start connect to ssid: %s", ssid.c_str());
  return true;
}

bool Wifi::ScanWifi(Json::Value &networks)
{
  networks = Json::arrayValue;

  wifi_scan_config_t scanConfig = {};
  esp_err_t err = esp_wifi_scan_start(&scanConfig, true);
  if (err != ESP_OK)
  {
    LOGE("WiFi scan failed to start: %s", esp_err_to_name(err));
    return false;
  }

  uint16_t apCount = 0;
  err = esp_wifi_scan_get_ap_num(&apCount);
  if (err != ESP_OK)
  {
    LOGE("WiFi scan failed to get AP count: %s", esp_err_to_name(err));
    return false;
  }

  std::vector<wifi_ap_record_t> records(apCount);
  if (apCount > 0)
  {
    err = esp_wifi_scan_get_ap_records(&apCount, records.data());
    if (err != ESP_OK)
    {
      LOGE("WiFi scan failed to get AP records: %s", esp_err_to_name(err));
      return false;
    }
  }

  for (uint16_t i = 0; i < apCount; ++i)
  {
    Json::Value network;
    network["SSID"] =
        reinterpret_cast<const char *>(records[i].ssid);
    network["RSSI"] = records[i].rssi;
    network["AUTHMODE"] = static_cast<int>(records[i].authmode);
    networks.append(network);
  }

  return true;
}

void Wifi::Init()
{
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &event_handler, NULL));

  esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  start_configured_wifi();
  start_network_monitor();
}

bool Wifi::WaitConnecting()
{
  if (!isWaitConnect || !wifi_event_group)
  {
    return false;
  }

  EventBits_t bits = xEventGroupWaitBits(
      wifi_event_group, WIFI_CONNECTED_EVENT, pdFALSE, pdTRUE,
      pdMS_TO_TICKS(15000));
  return (bits & WIFI_CONNECTED_EVENT) != 0;
}

bool Wifi::WaitStaConnecting()
{
  if (!isWaitConnect || !wifi_event_group)
  {
    return false;
  }

  EventBits_t bits = xEventGroupWaitBits(
      wifi_event_group, WIFI_CONNECTED_EVENT, pdFALSE, pdTRUE,
      pdMS_TO_TICKS(15000));
  return (bits & WIFI_CONNECTED_EVENT) != 0;
}

bool Wifi::WasLastSsidNotFound()
{
  const int reason = lastWifiDisconnectReason.load();
  return reason == WIFI_REASON_NO_AP_FOUND ||
         reason == WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY ||
         reason == WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD ||
         reason == WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD;
}

bool Wifi::IsNetworkConnected()
{
  return is_wifi_connected();
}

uint32_t Wifi::GetNetworkRevision()
{
  return networkRevision.load();
}

string Wifi::GetMacAddress()
{
  LOGD("GetMacAddress");
  uint8_t mac[6] = {0};
  ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
  char uc_Mac[100];
  sprintf((char *)uc_Mac, (const char *)"%.2x%.2x%.2x%.2x%.2x%.2x", mac[0],
          mac[1], mac[2], mac[3], mac[4], mac[5]);
  // return "f8b3b76471a8";
  return string(uc_Mac);
}

string Wifi::GetIP()
{
  char ip[32] = "10.10.10.1";
  esp_netif_t *netif = NULL;
  esp_netif_ip_info_t ip_info;
  for (int i = 0; i < esp_netif_get_nr_of_ifs(); ++i)
  {
    netif = esp_netif_next(netif);
    ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip_info));
    if (ip_info.ip.addr != 0 && ip_info.ip.addr != 0x010A0A0A) // 10.10.10.1
    {
      sprintf(ip, IPSTR, IP2STR(&ip_info.ip));
      break;
    }
  }
  LOGI("ip: %s", ip);
  return string(ip);
}

string Wifi::GetSSID()
{
  wifi_config_t wifi_config;
  if (esp_wifi_get_config(WIFI_IF_STA, &wifi_config) == ESP_OK)
  {
    return string((char *)wifi_config.sta.ssid);
  }
  return "";
}

bool Wifi::SetStaCredentials(const std::string &ssid,
                             const std::string &password)
{
  if (!is_valid_wifi_ssid(ssid.c_str()))
  {
    LOGW("SetStaCredentials failed: invalid ssid");
    return false;
  }

  wifi_config_t wifi_config = {};
  strncpy(reinterpret_cast<char *>(wifi_config.sta.ssid), ssid.c_str(),
          sizeof(wifi_config.sta.ssid) - 1);
  strncpy(reinterpret_cast<char *>(wifi_config.sta.password), password.c_str(),
          sizeof(wifi_config.sta.password) - 1);
  if (!password.empty())
  {
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  }

  LOGI("Stored STA credentials for ssid: %s", ssid.c_str());
  return apply_sta_config_and_start(wifi_config);
}

bool Wifi::ClearStoredCredentials()
{
  LOGW("Clearing stored WiFi credentials");

  suppressWifiReconnect.store(true);
  networkResetInProgress.store(true);
  isWaitConnect = false;
  retry_count = 0;
  lastWifiDisconnectReason.store(0);

  if (wifi_event_group)
  {
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_EVENT);
  }

  esp_err_t err = esp_wifi_disconnect();
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED &&
      err != ESP_ERR_WIFI_CONN && err != ESP_FAIL)
  {
    LOGW("esp_wifi_disconnect during clear returned %s", esp_err_to_name(err));
  }

  err = esp_wifi_stop();
  if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED)
  {
    LOGW("esp_wifi_stop during clear returned %s", esp_err_to_name(err));
  }

  err = esp_wifi_restore();
  if (err != ESP_OK)
  {
    LOGE("esp_wifi_restore failed: %s", esp_err_to_name(err));
    return false;
  }

  SetLedInternetConnected(false);
  LOGI("Stored WiFi credentials cleared");
  return true;
}

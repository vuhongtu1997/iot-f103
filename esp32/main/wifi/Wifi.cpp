#include "Wifi.h"

#include "Config.h"
#include "Define.h"
#include "Ethernet.h"
#include "Led.h"
#include "Log.h"
#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "esp_timer.h"
#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>

#ifdef CONFIG_BT_ENABLED
#include "esp_bt.h"
#endif
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"

#include "lwip/err.h"
#include "lwip/inet.h"
#include "lwip/sys.h"
#include <nvs_flash.h>

#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>
#include <wifi_provisioning/scheme_softap.h>

#include "cJSON.h"
#include <atomic>
#include <vector>

#define TAG "WIFI PROV"
static uint32_t retry_count = 0;

using namespace std;

static wifi_mode_t wifiMode = WIFI_MODE_NULL;
static bool isWaitConnect = false;
static bool wifiProvisionSuccess = false;
static atomic<bool> networkConfigMode(false);
static atomic<bool> suppressWifiReconnect(false);
static TaskHandle_t networkMonitorTaskHandle = nullptr;
static atomic<uint32_t> networkRevision(0);
static atomic<int> lastWifiDisconnectReason(0);

const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;

static string dormitory = "";

enum NetworkType
{
  NETWORK_NONE,
  NETWORK_ETHERNET,
  NETWORK_WIFI
};

static bool is_valid_wifi_ssid(const char *ssid)
{
  return ssid && ssid[0] != '\0' && strcmp(ssid, "Ethernet") != 0;
}

static bool is_wifi_connected()
{
  return wifi_event_group &&
         (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_EVENT) != 0;
}

static NetworkType get_network_type()
{
  if (Ethernet::IsConnected())
  {
    return NETWORK_ETHERNET;
  }
  if (is_wifi_connected())
  {
    return NETWORK_WIFI;
  }
  return NETWORK_NONE;
}

static bool apply_sta_config_and_start(wifi_config_t wifi_config,
                                       wifi_mode_t target_mode)
{
  if (!is_valid_wifi_ssid(reinterpret_cast<char *>(wifi_config.sta.ssid)))
  {
    LOGW("No valid WiFi SSID configured");
    isWaitConnect = false;
    return false;
  }

  retry_count = 0;
  isWaitConnect = true;
  wifiProvisionSuccess = false;
  lastWifiDisconnectReason.store(0);
  suppressWifiReconnect.store(true);
  wifiMode = target_mode;
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

  err = esp_wifi_set_mode(target_mode);
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
    return apply_sta_config_and_start(wifi_config, WIFI_MODE_STA);
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

  return apply_sta_config_and_start(wifi_config, WIFI_MODE_STA);
}

static void network_monitor_task(void *)
{
  constexpr TickType_t RETRY_INTERVAL = pdMS_TO_TICKS(30000);
  TickType_t lastEthernetRetry = 0;
  TickType_t lastWifiRetry = 0;
  NetworkType previousType = get_network_type();

  while (true)
  {
    if (networkConfigMode.load())
    {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    TickType_t now = xTaskGetTickCount();

    // Keep the Ethernet driver alive even while WiFi is carrying traffic.
    // The PHY must remain running to detect a cable inserted at runtime.
    if (!Ethernet::IsStarted() &&
        (lastEthernetRetry == 0 ||
         now - lastEthernetRetry >= RETRY_INTERVAL))
    {
      lastEthernetRetry = now;
      LOGI("Network monitor: starting Ethernet");
      Ethernet::Start();
    }

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

static void get_device_service_name(char *service_name, size_t max)
{
  uint8_t eth_mac[6];
  const char *ssid_prefix = SSID_PREFIX;
  esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
  snprintf(service_name, max, "%s%02X%02X%02X", ssid_prefix, eth_mac[3],
           eth_mac[4], eth_mac[5]);
}

#ifdef CONFIG_BT_ENABLED
static void rd_bluetooth_init(void)
{
  printf("enable bt_controller=========\n\n");

  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE)
  {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  }
  if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED)
  {
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
  }
}
#endif

#ifdef CONFIG_BT_ENABLED
static void rd_dis_bluetooth()
{
  printf("disable bt_controller=========\n\n");
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED)
  {
    esp_bt_controller_disable(); // dis radio ble
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED)
  {
    // esp_bt_controller_deinit(); // release driver + HW
  }
}
#endif

static string genRandRQI(int size)
{
  string rqi = "";
  srand(time(0));
  for (int i = 0; i < size; i++)
  {
    rqi += 'a' + rand() % 26;
  }
  return rqi;
}

esp_err_t custom_prov_data_handler(uint32_t session_id, const uint8_t *inbuf,
                                   ssize_t inlen, uint8_t **outbuf,
                                   ssize_t *outlen, void *priv_data)
{
  if (inbuf == NULL || inlen <= 0)
  {
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Received data: %.*s", (int)inlen, (char *)inbuf);

  /* 1. Parse JSON input */
  cJSON *root = cJSON_ParseWithLength((char *)inbuf, inlen);
  if (!root)
  {
    ESP_LOGE(TAG, "Invalid JSON");
    return ESP_FAIL;
  }

  cJSON *dorm = cJSON_GetObjectItem(root, "action");
  if (!cJSON_IsString(dorm))
  {
    ESP_LOGW(TAG, "JSON missing dormitory");
    cJSON_Delete(root);
    return ESP_FAIL;
  }

  std::string action = dorm->valuestring;

  ESP_LOGI(TAG, "Incoming action: %s", action.c_str());

  if (action == "getDevInfo")
  {
    /* 3. Lấy thông tin thiết bị */
    std::string mac = Wifi::GetMacAddress();
    const char *type = TYPE_DEVICE;

    /* 4. Build JSON trả về */
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "mac", mac.c_str());
    cJSON_AddStringToObject(resp, "type", type);

    char *json_out = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    cJSON_Delete(root);

    if (!json_out)
    {
      return ESP_ERR_NO_MEM;
    }

    *outbuf = (uint8_t *)strdup(json_out);
    *outlen = strlen(json_out) + 1;
    free(json_out);

    ESP_LOGI(TAG, "Send JSON: %s", (char *)*outbuf);
    return ESP_OK;
  }
  else
  {
    const char *msg = "ERROR";
    *outbuf = (uint8_t *)strdup(msg);
    *outlen = strlen(msg) + 1;
    ESP_LOGW(TAG, "Send ERROR");
    return ESP_OK;
  }
  cJSON_Delete(root);
  return ESP_OK;
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
#ifdef RD_RESET_PROV_MGR_ON_FAILURE
  static int retries;
#endif
  if (event_base == WIFI_PROV_EVENT)
  {
    switch (event_id)
    {
    case WIFI_PROV_START:
      ESP_LOGI(TAG, "Provisioning started");
      break;
    case WIFI_PROV_CRED_RECV:
    {
      wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
      ESP_LOGI(TAG,
               "Received Wi-Fi credentials"
               "\n\tSSID     : %s\n\tPassword : %s",
               (const char *)wifi_sta_cfg->ssid,
               (const char *)wifi_sta_cfg->password);
      break;
    }
    case WIFI_PROV_CRED_FAIL:
    {
      wifi_prov_sta_fail_reason_t *reason =
          (wifi_prov_sta_fail_reason_t *)event_data;
      ESP_LOGE(TAG,
               "Provisioning failed!\n\tReason : %s"
               "\n\tPlease reset to factory and retry provisioning",
               (*reason == WIFI_PROV_STA_AUTH_ERROR)
                   ? "Wi-Fi station authentication failed"
                   : "Wi-Fi access-point not found");
#ifdef RD_RESET_PROV_MGR_ON_FAILURE
      retries++;
      if (retries >= PROV_MGR_MAX_RETRY_CNT)
      {
        ESP_LOGE(TAG, "Failed to connect with provisioned AP, reseting "
                      "provisioned credentials");
        wifi_prov_mgr_reset_sm_state_on_failure();
        retries = 0;
      }
#endif
      break;
    }
    case WIFI_PROV_CRED_SUCCESS:
      ESP_LOGI(TAG, "Provisioning successful");
#ifdef RD_RESET_PROV_MGR_ON_FAILURE
      retries = 0;
#endif
      break;
    case WIFI_PROV_END:
      ESP_LOGW(TAG, "wifi provision end, deinit wifi_prov_mgr");
      wifi_prov_mgr_deinit();
      vTaskDelay(pdMS_TO_TICKS(2000));
#ifdef CONFIG_BT_ENABLED
      rd_dis_bluetooth();
#endif
      break;
    default:
      break;
    }
  }
  else if (event_base == WIFI_EVENT)
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
      wifiProvisionSuccess = false;
      SetLedInternetConnected(Ethernet::IsConnected());

      if (suppressWifiReconnect.load())
      {
        LOGI("WiFi stopped for an intentional network switch");
        break;
      }

      if (Ethernet::IsConnected())
      {
        LOGI("WiFi disconnected while Ethernet is available");
        retry_count = 0;
        isWaitConnect = false;
        esp_wifi_stop();
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
    case WIFI_EVENT_AP_STACONNECTED:
    {
      break;
    }
    case WIFI_EVENT_AP_STADISCONNECTED:
    {
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
    wifiProvisionSuccess = true;
    lastWifiDisconnectReason.store(0);
    retry_count = 0;
    SetLedInternetConnected(true);
  }
  else if (event_base == PROTOCOMM_SECURITY_SESSION_EVENT)
  {
    switch (event_id)
    {
    case PROTOCOMM_SECURITY_SESSION_SETUP_OK:
      ESP_LOGI(TAG, "Secured session established!");
      break;
    case PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS:
      ESP_LOGE(TAG, "Received invalid security parameters for establishing "
                    "secure session!");
      break;
    case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH:
      ESP_LOGE(TAG, "Received incorrect username and/or PoP for establishing "
                    "secure session!");
      break;
    default:
      break;
    }
  }
}

void Wifi::WifiProvisionConfig(int typeTransport)
{
  ESP_LOGW(TAG, "Provisioning active — restarting...");
  networkConfigMode.store(true);
  suppressWifiReconnect.store(true);

  wifi_prov_mgr_deinit();

#ifdef CONFIG_BT_ENABLED
  rd_dis_bluetooth();
#endif

  vTaskDelay(pdMS_TO_TICKS(500));

  // Disconnect & stop WiFi
  esp_err_t err = esp_wifi_disconnect();
  if (err != ESP_OK &&
      err != ESP_ERR_WIFI_NOT_STARTED &&
      err != ESP_ERR_WIFI_CONN)
  {
    ESP_LOGW(TAG, "esp_wifi_disconnect returned %d", err);
  }

  err = esp_wifi_stop();
  if (err == ESP_ERR_WIFI_NOT_STARTED)
  {
    ESP_LOGI(TAG, "WiFi already stopped");
  }
  else if (err != ESP_OK)
  {
    ESP_LOGW(TAG, "esp_wifi_stop returned %d", err);
  }
  else
  {
    ESP_LOGI(TAG, "esp_wifi_stop OK");
  }

  vTaskDelay(pdMS_TO_TICKS(100));

  wifiProvisionSuccess = false;

  // Create AP netif if using SoftAP
  if (typeTransport == WIFI_PROVISION_TRANSPORT_SOFTAP)
  {
    if (esp_netif_get_handle_from_ifkey("WIFI_AP_DEF") == NULL)
    {
      ESP_LOGI(TAG, "Creating default WiFi AP netif...");
      esp_netif_create_default_wifi_ap();
    }
    else
    {
      ESP_LOGI(TAG, "WiFi AP netif already exists.");
    }
  }

  // Provisioning config
  wifi_prov_mgr_config_t config = {
      .scheme = wifi_prov_scheme_softap,
      .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE};

  ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

  char service_name[15];
  get_device_service_name(service_name, sizeof(service_name));

  wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
  wifi_prov_security1_params_t *sec_params = POP_PARAMS;

  const char *service_key = NULL;
  service_key = PASS_WIFI;

  wifi_prov_mgr_endpoint_create("custom-data");

  ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
      security,
      (const void *)sec_params,
      service_name,
      service_key));

  wifi_prov_mgr_endpoint_register(
      "custom-data",
      custom_prov_data_handler,
      NULL);

  ESP_LOGI(TAG, "Provisioning started successfully");
}

void Wifi::WifiProvisionStop()
{
  wifi_prov_mgr_endpoint_unregister("custom-data");
  wifi_prov_mgr_stop_provisioning();
}

bool Wifi::IsSuccessWifiProvision() { return wifiProvisionSuccess; }

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
  wifiProvisionSuccess = false;
  suppressWifiReconnect.store(false);
  wifiMode = WIFI_MODE_STA;

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

void Wifi::Init(bool configMode)
{
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  networkConfigMode.store(configMode);
  bool ethernetConnected = false;
  if (Ethernet::Start())
  {
    if (!configMode)
    {
      LOGI("Waiting up to 10 seconds for Ethernet DHCP");
      ethernetConnected = Ethernet::WaitForIp(5000);
      if (ethernetConnected)
      {
        LOGI("Using Ethernet connection: %s", Ethernet::GetIP().c_str());
      }
      else
      {
        LOGW("Ethernet has no IP, starting WiFi fallback");
      }
    }
  }

  wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT,
                                             ESP_EVENT_ANY_ID, &event_handler,
                                             NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &event_handler, NULL));

  esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  if (configMode)
  {
    isWaitConnect = false;
    return;
  }

  if (ethernetConnected)
  {
    isWaitConnect = true;
    start_network_monitor();
    return;
  }

  start_configured_wifi();
  start_network_monitor();
}

bool Wifi::WaitConnecting()
{
  if (Ethernet::IsConnected())
  {
    return true;
  }
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

void Wifi::WifiStartAP(void)
{
  LOGD("WifiStartAP");

  networkConfigMode.store(true);
  suppressWifiReconnect.store(true);

  /* ===== STOP WIFI CŨ ===== */
  esp_wifi_disconnect();
  esp_wifi_stop();

  /* ===== TẠO AP NETIF ===== */
  esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  if (ap_netif == NULL)
  {
    ap_netif = esp_netif_create_default_wifi_ap();
  }
  assert(ap_netif);

  /* ===== CẤU HÌNH SSID ===== */
  wifi_config_t ap_wifi_config = {};
  ap_wifi_config.ap.channel = 1;
  ap_wifi_config.ap.authmode = WIFI_AUTH_OPEN;
  ap_wifi_config.ap.max_connection = 2;

  string mac = Wifi::GetMacAddress();
  snprintf((char *)ap_wifi_config.ap.ssid, sizeof(ap_wifi_config.ap.ssid),
           "HC_MINI_%s", mac.substr(mac.size() - 4, 4).c_str());
  ap_wifi_config.ap.ssid_len = strlen((char *)ap_wifi_config.ap.ssid);

  // uppercase SSID
  for (int i = 0; i < strlen((char *)ap_wifi_config.ap.ssid); i++)
  {
    ap_wifi_config.ap.ssid[i] = toupper(ap_wifi_config.ap.ssid[i]);
  }

  ap_wifi_config.ap.ssid_len = strlen((char *)ap_wifi_config.ap.ssid);

  /* ===== SET MODE AP ===== */
  wifiMode = WIFI_MODE_AP;
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

  /* ===== SET CONFIG AP ===== */
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_wifi_config));

  /* ===== START WIFI ===== */
  ESP_ERROR_CHECK(esp_wifi_start());

  /* ===== CẤU HÌNH IP ===== */
  ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));

  esp_netif_ip_info_t ip_info;
  IP4_ADDR(&ip_info.ip, 192, 168, 12, 1);
  IP4_ADDR(&ip_info.gw, 192, 168, 12, 1);
  IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

  ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));

  ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));

  /* ===== LOG ===== */
  LOGI("AP started");
  LOGI("SSID: %s", ap_wifi_config.ap.ssid);
  LOGI("IP: 192.168.12.1");
}

void Wifi::WifiStartSta(void)
{
  wifi_config_t wifi_config;
  LOGD("WifiStartSta");
  if (wifiMode != WIFI_MODE_STA)
  {
    LOGD("esp_wifi_stop");
    ESP_ERROR_CHECK(esp_wifi_stop());
    wifiMode = WIFI_MODE_STA;
  }
  esp_err_t ret = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
  if (ret == ESP_OK)
  {
    LOGI("%s", wifi_config.sta.ssid);
    LOGI("%s", wifi_config.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(wifiMode));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
  }
}

bool Wifi::DisableConfigAccessPoint(void)
{
  wifi_mode_t currentMode = WIFI_MODE_NULL;
  esp_err_t err = esp_wifi_get_mode(&currentMode);
  if (err != ESP_OK)
  {
    LOGE("DisableConfigAccessPoint get mode failed: %s",
         esp_err_to_name(err));
    return false;
  }

  if (currentMode == WIFI_MODE_STA)
  {
    wifiMode = WIFI_MODE_STA;
    networkConfigMode.store(false);
    suppressWifiReconnect.store(false);
    start_network_monitor();
    return true;
  }

  if (currentMode == WIFI_MODE_AP)
  {
    if (!Ethernet::IsConnected())
    {
      LOGW("Cannot stop config AP without an active Ethernet connection");
      return false;
    }

    err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED)
    {
      LOGE("DisableConfigAccessPoint stop AP failed: %s",
           esp_err_to_name(err));
      return false;
    }

    wifiMode = WIFI_MODE_NULL;
    networkConfigMode.store(false);
    suppressWifiReconnect.store(false);
    start_network_monitor();
    LOGI("Config SoftAP stopped; Ethernet remains active");
    return true;
  }

  if (currentMode != WIFI_MODE_APSTA)
  {
    LOGW("DisableConfigAccessPoint skipped in WiFi mode %d", currentMode);
    return false;
  }

  err = esp_wifi_set_mode(WIFI_MODE_STA);
  if (err != ESP_OK)
  {
    LOGE("DisableConfigAccessPoint failed: %s", esp_err_to_name(err));
    return false;
  }

  wifiMode = WIFI_MODE_STA;
  networkConfigMode.store(false);
  suppressWifiReconnect.store(false);
  start_network_monitor();
  LOGI("Config SoftAP stopped; STA connection remains active");
  return true;
}

bool Wifi::WifiIsAPMode(void) { return wifiMode == WIFI_MODE_AP; }

bool Wifi::WifiIsStaMode(void) { return wifiMode == WIFI_MODE_STA; }

bool Wifi::IsNetworkConnected()
{
  return Ethernet::IsConnected() || is_wifi_connected();
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
  string ethernetIp = Ethernet::GetIP();
  if (!ethernetIp.empty())
  {
    LOGI("ip: %s", ethernetIp.c_str());
    return ethernetIp;
  }

  // LOGD("GetIP");
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
  if (Ethernet::IsConnected())
  {
    return "Ethernet";
  }

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

  wifi_mode_t current_mode;
  esp_err_t mode_err = esp_wifi_get_mode(&current_mode);
  if (mode_err != ESP_OK)
  {
    LOGE("SetStaCredentials get mode failed: %s", esp_err_to_name(mode_err));
    return false;
  }

  wifi_mode_t target_mode = WIFI_MODE_STA;
  if (current_mode == WIFI_MODE_AP || current_mode == WIFI_MODE_APSTA)
  {
    target_mode = WIFI_MODE_APSTA;
  }

  LOGI("Stored STA credentials for ssid: %s", ssid.c_str());
  return apply_sta_config_and_start(wifi_config, target_mode);
}

bool Wifi::ClearStoredCredentials()
{
  LOGW("Clearing stored WiFi credentials");

  suppressWifiReconnect.store(true);
  networkConfigMode.store(true);
  isWaitConnect = false;
  wifiProvisionSuccess = false;
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

  wifiMode = WIFI_MODE_NULL;
  SetLedInternetConnected(false);
  LOGI("Stored WiFi credentials cleared");
  return true;
}

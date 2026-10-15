#include "Gateway.h"
#include "Base64.h"
#include "ButtonSignal.h"
#include "Config.h"
#include "Database.h"
#include "DeviceManager.h"
#include "protocol.h"
#include "Log.h"
#include "Util.h"
#include "Wifi.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <unistd.h>

#ifdef ESP_PLATFORM
#include "Led.h"
#endif

Gateway *Gateway::getInstance()
{
  static Gateway *gateway = nullptr;
  if (!gateway)
  {
    string mac = Wifi::GetMacAddress();
    LOGI("Gateway MAC: %s", mac.c_str());
    Config::getInstance()->ReadConfig();
    string connectInfo = "hc-" + mac;
    gateway = new Gateway(mac, Config::getInstance()->GetHost(), 1883,
                          connectInfo, connectInfo, "123456abc",
                          Config::getInstance()->GetKeepAlive());
  }
  return gateway;
}

Gateway::Gateway(string mac, string address, int port, string clientId,
                 string username, string password, int keepalive)
    : CloudProtocol(mac, address, port, clientId, username, password,
                    keepalive, false)
{
  this->mac = mac;
  id = "";
  dormitoryId = "";
  refresh_token = "";
  ble_addr = 0;
  ble_iv_index = 0;
  ble_netkey = "";
  ble_appkey = "";
  ble_devicekey = "";
  data = "";
}

Gateway::~Gateway() {}

void Gateway::Init()
{
  Database::getInstance()->GatewayRead();
  if (GetBleNetKey().empty() && GetBleAppKey().empty() &&
      GetBleDeviceKey().empty() && GetBleAddr() == 0)
  {
    Database::getInstance()->GatewayAdd();
  }
  CloudProtocol::init();
  InitMqttMessageDevice();
  InitMqttMessageRule();
  InitMqttMessageRs485();

  Database::getInstance()->DeviceRead();
  if (!ModbusBusInit())
    LOGE("RS485 Modbus initialization failed");
#ifdef CONFIG_SAVE_ATTRIBUTE
  Database::getInstance()->DeviceAttributeRead();
#endif
  Database::getInstance()->RuleRead();
}

void Gateway::OnCloudConnect(bool isConnected, bool isReconnect)
{
  LOGI("OnCloudConnect: %d", isConnected);
  if (isConnected)
  {
    Util::LedInternet(true);
#ifdef ESP_PLATFORM
    SetLedServerConnected(true);
    ButtonSignal::getInstance()->CompleteConfigMode();
#endif
    OnlineHC();
  }
  else
  {
    Util::LedInternet(false);
#ifdef ESP_PLATFORM
    SetLedServerConnected(false);
#endif
  }
}

void Gateway::ResetFactory()
{
  LOGI("ResetFactory");
  Database::getInstance()->DelDatabase();
  bool configCleared = Config::getInstance()->ClearProvisioningData();
  bool wifiCleared = Wifi::ClearStoredCredentials();
  LOGI("Factory reset: configCleared=%d wifiCleared=%d",
       configCleared ? 1 : 0, wifiCleared ? 1 : 0);
}

int Gateway::CheckOnlineThread()
{
  LOGI("Start CheckOnlineThread");
  time_t lastGatewayHeartbeat = 0;
  uint32_t lastNetworkRevision = Wifi::GetNetworkRevision();

  while (true)
  {
    time_t currentTime = time(nullptr);
    uint32_t networkRevision = Wifi::GetNetworkRevision();
    if (networkRevision != lastNetworkRevision)
    {
      lastNetworkRevision = networkRevision;
      if (Wifi::IsNetworkConnected())
        CloudProtocol::Reconnect();
    }

    if (currentTime - lastGatewayHeartbeat >= 420 &&
        CloudProtocol::IsConnected())
    {
      PushGatewayStatus();
      lastGatewayHeartbeat = currentTime;
    }

    sleep(1);
  }
  return CODE_OK;
}

void Gateway::SendStatusDev()
{
  DeviceManager::getInstance()->ForEach([&](Device *device)
                                        { PushDeviceOnlineState(device, device->IsOnline()); });
}

uint16_t Gateway::GetBleAddr() { return ble_addr; }
uint32_t Gateway::GetBleIvIndex() { return ble_iv_index; }
string Gateway::GetBleNetKey() { return ble_netkey; }
string Gateway::GetBleAppKey() { return ble_appkey; }
string Gateway::GetBleDeviceKey() { return ble_devicekey; }
string Gateway::GetDormitory() { return dormitoryId; }
string Gateway::GetId() { return id; }
string Gateway::GetVersion() { return version; }
string Gateway::GetName() { return ""; }
string Gateway::GetRefreshToken() { return refresh_token; }
string Gateway::GetData() { return data; }
string Gateway::GetMac() { return mac; }

void Gateway::SetBleAddr(uint16_t addr) { ble_addr = addr; }
void Gateway::SetBleIvIndex(uint32_t ivIndex) { ble_iv_index = ivIndex; }
void Gateway::SetBleNetkey(string netkey) { ble_netkey = netkey; }
void Gateway::SetBleAppkey(string appkey) { ble_appkey = appkey; }
void Gateway::SetBleDevicekey(string devicekey) { ble_devicekey = devicekey; }
void Gateway::SetDormitory(string dormitory) { dormitoryId = dormitory; }
void Gateway::SetRefreshToken(string token) { refresh_token = token; }
void Gateway::SetData(string value) { data = value; }
void Gateway::SetId(string value) { id = value; }
void Gateway::SetMac(string value) { mac = value; }
void Gateway::SetVersion(string value) { version = value; }
void Gateway::SetName(string name) {}

int Gateway::Do(Json::Value &dataValue)
{
  LOGD("Do data: %s", dataValue.toString().c_str());
  return CODE_FORMAT_ERROR;
}

int Gateway::PushDeviceUpdateCloud(Json::Value &dataValue)
{
  if (!CloudProtocol::IsConnected())
    return CODE_TIMEOUT;
  Json::Value payload;
  payload["cmd"] = "deviceUpdate";
  payload["rqi"] = Util::GenRandRQI(16);
  payload["data"] = dataValue;
  return CloudPublish(payload);
}

int Gateway::PushDeviceUpdateCloud(Device *device,
                                   Json::Value &telemetryValue)
{
  if (!device)
    return CODE_ERROR;

  Json::Value values = telemetryValue;
  if (telemetryValue.isArray() && !telemetryValue.empty() &&
      telemetryValue[0].isMember("values"))
    values = telemetryValue[0]["values"];
  if (!values.isObject())
    return CODE_FORMAT_ERROR;

  Json::Value deviceValue;
  deviceValue["id"] = device->GetCloudId().empty() ? device->GetId()
                                                   : device->GetCloudId();
  deviceValue["data"] = values;
  if (!deviceValue["data"].isMember(KEY_ATTRIBUTE_ONLINE_OFFLINE))
    deviceValue["data"][KEY_ATTRIBUTE_ONLINE_OFFLINE] =
        device->IsOnline() ? 1 : 0;

  Json::Value request;
  request["device"] = Json::arrayValue;
  request["device"].append(deviceValue);
  return PushDeviceUpdateCloud(request);
}

int Gateway::PushDeviceOnlineState(Device *device, bool isOnline)
{
  Json::Value state;
  state[KEY_ATTRIBUTE_ONLINE_OFFLINE] = isOnline ? 1 : 0;
  return PushDeviceUpdateCloud(device, state);
}

int Gateway::PushGatewayStatus()
{
  int deviceCount = 0;
  DeviceManager::getInstance()->ForEach([&](Device *)
                                        { deviceCount++; });
  Json::Value payload;
  payload["cmd"] = "hcInfo";
  payload["rqi"] = Util::GenRandRQI(16);
  Json::Value &info = payload["data"];
  info["version"] = STR(VERSION);
  info["ipLocal"] = Wifi::GetIP();
  info["nameWifi"] = Wifi::GetSSID();
  info["isHCmini"] = true;
  info["deviceCount"] = deviceCount;
  info["maxDevice"] = 30;
  return CloudPublish(payload);
}

int Gateway::PublishModbusTelemetry(const ModbusNodeInfo &node,
                                    const uint16_t registers[12], bool online)
{
  char uid[25];
  bool hasUid = false;
  for (size_t i = 0; i < sizeof(node.uid); ++i)
  {
    snprintf(uid + i * 2, sizeof(uid) - i * 2, "%02x", node.uid[i]);
    hasUid = hasUid || node.uid[i] != 0;
  }

  Json::Value device;
  string deviceId;
  if (hasUid)
    deviceId = uid;
  else
    deviceId = "pending-" + std::to_string(node.address);
  const string parentId = string("rs485-") + deviceId;
  device["id"] = parentId;
  device["data"]["address"] = node.address;
  device["data"]["uid"] = uid;
  device["data"]["stt"] = online ? 1 : 0;
  device["data"]["sensor_type"] = node.sensorType;
  device["data"]["sensor_type_name"] =
      DeviceModbus::GetSensorName(node.sensorType);
  if (online)
  {
    device["data"]["version"] = static_cast<Json::UInt64>(
        (static_cast<uint32_t>(registers[2]) << 16) | registers[3]);
    device["data"]["mode"] = registers[4];
    device["data"]["adc_raw"] = registers[0];
    device["data"]["millivolts"] = registers[1];
    device["data"]["quality"] = registers[4]             ? "bootloader"
                                : registers[0] == 0xffff ? "sensor_error"
                                                         : "ok";
  }
  else
  {
    device["data"]["version"] = static_cast<Json::UInt64>(node.version);
    device["data"]["mode"] = node.mode;
    device["data"]["quality"] = "offline";
  }

  Json::Value update;
  update["device"] = Json::arrayValue;
  update["device"].append(device);
  const char *sensorDeviceType =
      DeviceModbus::GetSensorDeviceType(node.sensorType);
  if (sensorDeviceType && hasUid)
  {
    Json::Value child;
    child["id"] =
        DeviceModbus::GetSensorDeviceId(parentId, node.sensorType);
    child["data"]["parent_id"] = parentId;
    child["data"]["address"] = node.address;
    child["data"]["uid"] = uid;
    child["data"]["type"] = sensorDeviceType;
    child["data"]["sensor_type"] = node.sensorType;
    child["data"]["sensor_type_name"] =
        DeviceModbus::GetSensorName(node.sensorType);
    child["data"]["stt"] = online ? 1 : 0;
    if (online)
    {
      child["data"]["quality"] =
          node.sensorType == MODBUS_SENSOR_TEMPERATURE_HUMIDITY
              ? "measurement_unavailable"
          : registers[0] == 0xffff ? "sensor_error"
                                   : "ok";
      if (node.sensorType == MODBUS_SENSOR_GAS ||
          node.sensorType == MODBUS_SENSOR_CH3)
      {
        child["data"]["raw"] = registers[0];
        child["data"]["millivolts"] = registers[1];
      }
    }
    else
    {
      child["data"]["quality"] = "offline";
    }
    update["device"].append(child);
  }
  return PushDeviceUpdateCloud(update);
}

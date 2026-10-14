#include "Gateway.h"
#include "Base64.h"
#include "BleDeviceConfigs.h"
#include "BleProtocol.h"
#include "ButtonSignal.h"
#include "Config.h"
#include "Database.h"
#include "DeviceManager.h"
#include "Log.h"
#include "Util.h"
#include "Wifi.h"
#include <algorithm>
#include <cctype>
#include <unistd.h>

#ifdef ESP_PLATFORM
#include "Led.h"
#endif

static constexpr time_t BLE_BROADCAST_HEALTH_INTERVAL = 420;
static constexpr time_t BLE_BROADCAST_HEALTH_TIMEOUT = 120;

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
                    keepalive, false),
      Udp(8181)
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
  lastTimePingGwBle = time(nullptr);
  broadcastHealthPending = false;
  broadcastHealthStartTime = 0;
  processingSceneGroup = false;
  InitUdpMessage();
}

Gateway::~Gateway() {}

void Gateway::Init()
{
  Device::InitDeviceModelList();
  Database::getInstance()->GatewayRead();
  if (GetBleNetKey().empty() && GetBleAppKey().empty() &&
      GetBleDeviceKey().empty() && GetBleAddr() == 0)
  {
    Database::getInstance()->GatewayAdd();
  }
  CloudProtocol::init();
  InitMqttMessageDevice();
  InitMqttMessageGroup();
  InitMqttMessageRoom();
  InitMqttMessageRule();
  InitMqttMessageScene();
  InitMqttMessageHc();

  Database::getInstance()->DeviceRead();
  Database::getInstance()->DeviceBleChildRead();
#ifdef CONFIG_SAVE_ATTRIBUTE
  Database::getInstance()->DeviceAttributeRead();
#endif
  Database::getInstance()->RoomRead();
  Database::getInstance()->GroupRead();
  Database::getInstance()->DeviceInGroupRead();
  Database::getInstance()->DeviceInRoomRead();
  Database::getInstance()->RuleRead();
}

void Gateway::StartUdpService() { Udp::init(); }

void Gateway::StopUdpService() { Udp::stop(); }

void Gateway::OnCloudConnect(bool isConnected, bool isReconnect)
{
  LOGI("OnCloudConnect: %d", isConnected);
  if (isConnected)
  {
    Util::LedInternet(true);
#ifdef ESP_PLATFORM
    SetLedServerConnected(true);
    ButtonSignal::getInstance()->CompleteConfigMode();
    Wifi::DisableConfigAccessPoint();
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
#ifdef CONFIG_ENABLE_BLE
  LOGI("Factory reset: BLE delete all result=%d",
       BleProtocol::getInstance()->ResetDelAll());
  LOGI("Factory reset: BLE gateway reset result=%d",
       BleProtocol::getInstance()->ResetGateway());
  BleProtocol::getInstance()->ResetFactory();
#endif
  bool configCleared = Config::getInstance()->ClearProvisioningData();
  bool wifiCleared = Wifi::ClearStoredCredentials();
  LOGI("Factory reset: configCleared=%d wifiCleared=%d",
       configCleared ? 1 : 0, wifiCleared ? 1 : 0);
}

int Gateway::RestartBleGw()
{
  LOGW("Restart Gateway Ble");
  return CODE_OK;
}

int Gateway::CheckOnlineThread()
{
  LOGI("Start CheckOnlineThread");
  time_t lastGatewayHeartbeat = 0;
  time_t lastBleBroadcastHealth = 0;
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

#ifdef CONFIG_ENABLE_BLE
    vector<Device *> devices;
    DeviceManager::getInstance()->ForEach([&](Device *device)
                                          {
      if (device->GetProtocol() == BLE_DEVICE)
        devices.push_back(device); });

    if (!devices.empty() && CloudProtocol::IsConnected() &&
        !BleProtocol::getInstance()->IsProvision() &&
        !CloudProtocol::IsBusy() && BleProtocol::getInstance()->isInitKey)
    {
      bool startBroadcast = false;
      bool finishBroadcast = false;
      {
        lock_guard<mutex> lock(broadcastHealthMtx);
        startBroadcast = !broadcastHealthPending &&
                         currentTime - lastBleBroadcastHealth >=
                             BLE_BROADCAST_HEALTH_INTERVAL;
        finishBroadcast = broadcastHealthPending &&
                          currentTime - broadcastHealthStartTime >=
                              BLE_BROADCAST_HEALTH_TIMEOUT;
      }

      if (startBroadcast &&
          BleProtocol::getInstance()->GetDevicesOnOffBroadcast() == CODE_OK)
      {
        lock_guard<mutex> lock(broadcastHealthMtx);
        broadcastHealthPending = true;
        broadcastHealthStartTime = time(nullptr);
        broadcastHealthRespondedIds.clear();
        lastBleBroadcastHealth = broadcastHealthStartTime;
      }

      if (finishBroadcast)
      {
        set<string> respondedIds;
        {
          lock_guard<mutex> lock(broadcastHealthMtx);
          broadcastHealthPending = false;
          respondedIds = broadcastHealthRespondedIds;
          broadcastHealthRespondedIds.clear();
        }
        for (Device *device : devices)
        {
          string deviceId = device->GetId().empty() ? device->GetMac()
                                                    : device->GetId();
          if (respondedIds.find(deviceId) != respondedIds.end() ||
              IsIrBleDevice(device))
            continue;
          device->lastOnlineState = false;
          PushDeviceOnlineState(device, false);
        }
      }
    }
#endif
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
  int groupCount = 0;
  GroupManager::getInstance()->ForEach([&](Group *)
                                       { groupCount++; });
  int sceneCount = 0;
  SceneManager::getInstance()->ForEach([&](Scene *)
                                       { sceneCount++; });

  Json::Value payload;
  payload["cmd"] = "hcInfo";
  payload["rqi"] = Util::GenRandRQI(16);
  Json::Value &info = payload["data"];
  info["version"] = STR(VERSION);
  info["ipLocal"] = Wifi::GetIP();
  info["nameWifi"] = Wifi::GetSSID();
  info["isHCmini"] = true;
  info["deviceCount"] = deviceCount;
  info["groupCount"] = groupCount;
  info["sceneCount"] = sceneCount;
  info["maxDevice"] = 30;
  info["maxGroup"] = 50;
  info["maxScene"] = 50;
  return CloudPublish(payload);
}

int Gateway::PublishProvisionedDevice(Device *device)
{
  if (!device)
  {
    LOGE("Cannot publish newDev: device is null");
    return CODE_ERROR;
  }
  if (!CloudProtocol::IsConnected())
  {
    LOGE("Cannot publish newDev for %s: cloud is disconnected",
         device->GetMac().c_str());
    return CODE_TIMEOUT;
  }

  Json::Value deviceData = device->GetData();
  string deviceKey = deviceData.get("devicekey", "").asString();
  if (deviceKey.empty())
    deviceKey = deviceData.get("deviceKey", "").asString();
  if (deviceKey.empty())
  {
    LOGE("Cannot publish newDev for %s: devicekey is missing",
         device->GetMac().c_str());
    return CODE_FORMAT_ERROR;
  }
  transform(deviceKey.begin(), deviceKey.end(), deviceKey.begin(),
            [](unsigned char ch)
            {
              return static_cast<char>(std::tolower(ch));
            });

  const Json::UInt meshPid = deviceData.get("meshPid", Json::UInt(deviceData.get("deviceType", 0).asUInt() & 0xFFFF)).asUInt();
  const Json::UInt meshVid = deviceData.get("meshVid", Json::UInt(device->GetVersion())).asUInt();
  const Json::UInt deviceType = deviceData.get("deviceType", 0).asUInt();

  string cloudId = deviceData.get("device_id", "").asString();
  if (cloudId.empty())
    cloudId = deviceKey;
  transform(cloudId.begin(), cloudId.end(), cloudId.begin(),
            [](unsigned char ch)
            {
              return static_cast<char>(std::tolower(ch));
            });

  Json::Value value;
  value["id"] = cloudId;
  value["addr"] = Json::UInt(device->GetAddr());
  value["mac"] = "";
  value["type"] = deviceType;
  value["data"]["devicekey"] = deviceKey;
  value["data"]["vid"] = meshVid;
  value["data"]["pid"] = meshPid;

  Json::Value payload;
  payload["cmd"] = "newDev";
  payload["rqi"] = Util::GenRandRQI(16);
  payload["data"]["device"] = Json::arrayValue;
  payload["data"]["device"].append(value);
  LOGI("Publishing provisioned device with newDev: addr=0x%04X",
       device->GetAddr());
  return CloudPublish(payload);
}

time_t Gateway::GetLastTimePingGwBle() { return lastTimePingGwBle; }
void Gateway::SetLastTimePingGwBle(time_t value) { lastTimePingGwBle = value; }

int Gateway::AddDeviceToControlResponse(Device *device,
                                        Json::Value &dataValue)
{
  if (!device)
    return CODE_ERROR;
  if (listControlObj.empty())
    return CODE_ERROR;

  lock_guard<mutex> lock(listControlObjMutex);
  bool matched = false;
  for (auto &[id, type] : listControlObj)
  {
    if (type == 0)
    {
      Group *group = GroupManager::getInstance()->GetGroupFromId(id);
      if (group && group->IsDevIn(device))
      {
        group->AddDataToListRsp(device, dataValue);
        matched = true;
      }
    }
    else if (type == 1)
    {
      Room *room = RoomManager::getInstance()->GetRoomFromId(id);
      if (room && room->IsDevIn(device))
      {
        room->AddDataToListRsp(device, dataValue);
        matched = true;
      }
    }
    else if (type == 2)
    {
      Scene *scene = SceneManager::getInstance()->GetSceneFromId(id);
      if (scene && scene->IsDevIn(device))
      {
        scene->AddDataToListRsp(device, dataValue);
        matched = true;
      }
    }
  }
  return matched ? CODE_OK : CODE_ERROR;
}

void Gateway::SetProcessingSceneGroup(bool processing)
{
  processingSceneGroup.store(processing);
}

bool Gateway::IsProcessingSceneGroup() const
{
  return processingSceneGroup.load();
}

void Gateway::MarkBleBroadcastResponse(Device *device)
{
  if (!device)
    return;
  bool shouldPublish = false;
  string deviceId = device->GetId().empty() ? device->GetMac()
                                            : device->GetId();
  {
    lock_guard<mutex> lock(broadcastHealthMtx);
    if (broadcastHealthPending)
      shouldPublish = broadcastHealthRespondedIds.insert(deviceId).second;
  }
  device->lastOnlineState = true;
  if (shouldPublish)
    PushDeviceOnlineState(device, true);
}

bool Gateway::IsIrBleDevice(Device *device)
{
  if (!device)
    return false;
  if (device->GetType() == "HG_BLE_IR")
    return true;

  Json::Value value = device->GetData();
  string pid = value.get("pid", "").asString();
  if (pid.empty() && value.isMember("devices") && value["devices"].isString())
  {
    vector<string> tokens = Util::splitString(value["devices"].asString(), '|');
    if (tokens.size() > 1)
      pid = tokens[1];
  }
  return pid == HG_BLE_IR || pid == HG_BLE_IR_TV || pid == HG_BLE_IR_FAN ||
         pid == HG_BLE_IR_AC || pid == HG_BLE_IR_REMOTE ||
         pid == HG_BLE_IR_V2 || pid == HG_BLE_IR_TV_V2 ||
         pid == HG_BLE_IR_FAN_V2 || pid == HG_BLE_IR_AC_V2 ||
         pid == HG_BLE_IR_REMOTE_V2 || pid == HG_BLE_IR_BLE ||
         pid == HG_BLE_IR_TV_V2_LAO || pid == HG_BLE_IR_FAN_V2_LAO ||
         pid == HG_BLE_IR_AC_V2_LAO || pid == HG_BLE_IR_REMOTE_V2_LAO;
}

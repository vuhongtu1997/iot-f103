#include "BleProtocol.h"
#include "AES.h"
#include "BleOpCode.h"
#include "BleDeviceConfigs.h"
#include "Config.h"
#include "Database.h"
#include "DeviceBle.h"
#include "Define.h"
#include "Gateway.h"
// #include "DeviceBleSeftPowerRemote.h"
#include "Log.h"
#include "Util.h"

#include <stdlib.h>
#include <string.h>
#include <cctype>
#include <random>
#include <thread>
#include "mbedtls/aes.h"

#ifndef bswap_16
#define bswap_16(x) __builtin_bswap16(x)
#endif
#ifndef bswap_32
#define bswap_32(x) __builtin_bswap32(x)
#endif
#include <algorithm>
#include <string.h>

#ifdef ESP_PLATFORM
#include "Led.h"
#include "esp_random.h"
#endif

using namespace std;

static BleProtocol *bleProtocol = NULL;

static string NormalizeMeshKey(string key)
{
  key.erase(remove(key.begin(), key.end(), '-'), key.end());
  return key;
}

static bool LoadMeshKey(const string &key, uint8_t output[16])
{
  const string normalized = NormalizeMeshKey(key);
  if (normalized.size() != 32)
    return false;
  for (char digit : normalized)
    if (!isxdigit(static_cast<unsigned char>(digit)))
      return false;
  try
  {
    for (size_t i = 0; i < 16; ++i)
      output[i] = static_cast<uint8_t>(
          stoul(normalized.substr(i * 2, 2), nullptr, 16));
  }
  catch (...)
  {
    return false;
  }
  return true;
}

struct StoredMeshConfig
{
  uint16_t gatewayAddr;
  uint16_t netKeyIndex;
  uint16_t appKeyIndex;
  uint32_t ivIndex;
  string netKey;
  string appKey;
  string deviceKey;
  uint16_t nextAddress;
};

static bool ParseMeshHex(const Json::Value &config, const char *name,
                         unsigned long maxValue, unsigned long &value)
{
  if (!config.isMember(name) || !config[name].isString())
    return false;
  const string hex = config[name].asString();
  if (hex.empty())
    return false;
  char *end = nullptr;
  value = strtoul(hex.c_str(), &end, 16);
  return end != hex.c_str() && *end == '\0' && value <= maxValue;
}

static bool ParseMeshNumber(const Json::Value &config, const char *name,
                            unsigned long maxValue, unsigned long &value)
{
  if (!config.isMember(name))
    return false;
  const Json::Value &field = config[name];
  if (field.isUInt())
  {
    value = field.asUInt();
    return value <= maxValue;
  }
  if (!field.isString())
    return false;
  const string raw = field.asString();
  if (raw.empty())
    return false;
  char *end = nullptr;
  value = strtoul(raw.c_str(), &end, 0);
  return end != raw.c_str() && *end == '\0' && value <= maxValue;
}

static bool ParseStoredMeshConfig(const Json::Value &config,
                                  StoredMeshConfig &mesh)
{
  if (!config.isObject())
    return false;
  if (config.isMember("homeId") && config["homeId"].isString() &&
      !Gateway::getInstance()->GetDormitory().empty() &&
      config["homeId"].asString() != Gateway::getInstance()->GetDormitory())
    return false;
  unsigned long gatewayAddr = 0, netKeyIndex = 0, appKeyIndex = 0, ivIndex = 0;
  const bool legacy = config.isMember("gw_add");
  if ((legacy &&
       (!ParseMeshHex(config, "gw_add", 0x7FFF, gatewayAddr) ||
        !ParseMeshHex(config, "gw_netkeyIndex", 0x0FFF, netKeyIndex) ||
        !ParseMeshHex(config, "gw_appkeyIndex", 0x0FFF, appKeyIndex) ||
        !ParseMeshHex(config, "gw_ivIndex", 0xFFFFFFFFUL, ivIndex))) ||
      (!legacy &&
       (!ParseMeshNumber(config, "gatewayAddress", 0x7FFF, gatewayAddr) ||
        !ParseMeshNumber(config, "networkKeyIndex", 0x0FFF, netKeyIndex) ||
        !ParseMeshNumber(config, "appKeyIndex", 0x0FFF, appKeyIndex) ||
        !ParseMeshNumber(config, "ivIndex", 0xFFFFFFFFUL, ivIndex))) ||
      gatewayAddr == 0)
    return false;
  if (!legacy && config.isMember("provisionerAddress"))
  {
    unsigned long provisionerAddr = 0;
    if (!ParseMeshNumber(config, "provisionerAddress", 0x7FFF,
                         provisionerAddr) || provisionerAddr != gatewayAddr)
      return false;
  }
  uint8_t keyBytes[16];
  const char *netKeyName = legacy ? "gw_netkey" : "networkKey";
  const char *appKeyName = legacy ? "gw_appkey" : "appKey";
  const char *deviceKeyName = legacy ? "gw_deviceKey1" : "gatewayDeviceKey";
  if (!config.isMember(netKeyName) || !config[netKeyName].isString() ||
      !config.isMember(appKeyName) || !config[appKeyName].isString() ||
      !config.isMember(deviceKeyName) || !config[deviceKeyName].isString())
    return false;
  mesh.netKey = NormalizeMeshKey(config[netKeyName].asString());
  mesh.appKey = NormalizeMeshKey(config[appKeyName].asString());
  mesh.deviceKey = NormalizeMeshKey(config[deviceKeyName].asString());
  if (!LoadMeshKey(mesh.netKey, keyBytes) ||
      !LoadMeshKey(mesh.appKey, keyBytes) ||
      !LoadMeshKey(mesh.deviceKey, keyBytes))
    return false;
  mesh.gatewayAddr = static_cast<uint16_t>(gatewayAddr);
  mesh.netKeyIndex = static_cast<uint16_t>(netKeyIndex);
  mesh.appKeyIndex = static_cast<uint16_t>(appKeyIndex);
  mesh.ivIndex = static_cast<uint32_t>(ivIndex);
  unsigned long nextAddress = 0;
  mesh.nextAddress =
      ParseMeshNumber(config, "nextAvailableUnicastAddress", 0x7FFF,
                      nextAddress) && nextAddress >= 2
          ? static_cast<uint16_t>(nextAddress)
          : 2;
  return true;
}

static bool ReadStoredMeshConfig(StoredMeshConfig &mesh)
{
  string json = Config::getInstance()->GetProvisionConfigJson();
  Json::Value config;
  Json::Reader reader;
  if (!json.empty() && reader.parse(json, config) &&
      ParseStoredMeshConfig(config, mesh))
    return true;
  json = Gateway::getInstance()->GetData();
  return !json.empty() && reader.parse(json, config) &&
         ParseStoredMeshConfig(config, mesh);
}

static void FillMeshRandom(uint8_t *output, size_t size)
{
#ifdef ESP_PLATFORM
  esp_fill_random(output, size);
#else
  std::random_device source;
  for (size_t i = 0; i < size; ++i)
    output[i] = static_cast<uint8_t>(source());
#endif
}

static bool CreateFreshHomegyMeshConfig()
{
  // A new network may replace an orphaned chip Mesh after the gateway's
  // database and NVS Mesh configuration have been cleared. Stored nodes are
  // never replaced here.
  Gateway *gateway = Gateway::getInstance();
  if (gateway->GetDormitory().empty())
  {
    LOGW("Waiting for homeId before creating a Homegy Mesh");
    return false;
  }
  if (!gateway->GetBleNetKey().empty() ||
      !gateway->GetBleAppKey().empty() ||
      !gateway->GetBleDeviceKey().empty() || gateway->GetBleAddr() != 0)
  {
    LOGE("Gateway database contains Mesh data; refusing first-run replacement");
    return false;
  }
  bool hasStoredNodes = false;
  DeviceManager::getInstance()->ForEach([&](Device *device) {
    if (device && device->GetProtocol() == BLE_DEVICE)
      hasStoredNodes = true;
  });
  if (hasStoredNodes)
  {
    LOGE("Cannot create a new Homegy Mesh while BLE nodes are stored");
    return false;
  }

  uint8_t networkKey[16], appKey[16], gatewayDeviceKey[16];
  FillMeshRandom(networkKey, sizeof(networkKey));
  FillMeshRandom(appKey, sizeof(appKey));
  FillMeshRandom(gatewayDeviceKey, sizeof(gatewayDeviceKey));
  Json::Value config;
  config["networkKey"] = Util::uuidToStr(networkKey);
  config["homeId"] = gateway->GetDormitory();
  config["networkKeyIndex"] = 0;
  config["appKey"] = Util::uuidToStr(appKey);
  config["appKeyIndex"] = 0;
  config["gatewayDeviceKey"] = Util::uuidToStr(gatewayDeviceKey);
  config["ivIndex"] = 0;
  // Allocate the first unicast address to the provisioner in this new Mesh.
  config["gatewayAddress"] = 1;
  config["provisionerAddress"] = 1;
  config["nextAvailableUnicastAddress"] = 2;
  if (!Config::getInstance()->SaveProvisionConfigJson(config.toString()))
  {
    LOGE("Could not persist new Homegy Mesh credentials");
    return false;
  }
  LOGI("Created and persisted a new Homegy Mesh for this gateway");
  return true;
}

static bool SaveNextHomegyAddress(uint16_t nextAddress)
{
  string json = Config::getInstance()->GetProvisionConfigJson();
  Json::Value config;
  Json::Reader reader;
  StoredMeshConfig mesh{};
  if (json.empty() || !reader.parse(json, config) ||
      !ParseStoredMeshConfig(config, mesh))
  {
    json = Gateway::getInstance()->GetData();
    if (json.empty() || !reader.parse(json, config) ||
        !ParseStoredMeshConfig(config, mesh))
      return false;
  }
  config["nextAvailableUnicastAddress"] = nextAddress;
  return Config::getInstance()->SaveProvisionConfigJson(config.toString());
}

struct ProvisionedDeviceInfo
{
  string pid;
  string model;
  uint8_t elementCount;
  unsigned int type;
};

static bool ResolveProvisionedDeviceInfo(uint32_t deviceType,
                                         ProvisionedDeviceInfo &info)
{
  char pid[12] = {0};
  snprintf(pid, sizeof(pid), "BLEHG%06lX",
           static_cast<unsigned long>(deviceType & 0xFFFFFF));

  const auto &configs = getBleDeviceConfigs();
  const auto config = configs.find(pid);
  if (config == configs.end())
    return false;

  info.pid = config->first;
  info.model = config->second.modelName;
  info.elementCount = static_cast<uint8_t>(config->second.elementCount);
  info.type = config->second.serverTypeId;
  return true;
}

static uint32_t AdvertisedDeviceType(const scan_device_message_t &scan)
{
  // The final three UUID bytes encode the same BLEHG product number as
  // CheckType (for example AA 06 06 for BLEHGAA0606).
  return (static_cast<uint32_t>(scan.uuid[13]) << 16) |
         (static_cast<uint32_t>(scan.uuid[14]) << 8) | scan.uuid[15];
}

static bool ReserveHomegyAddress(uint16_t &nextAddr, uint8_t elementCount,
                                 uint16_t gatewayAddr)
{
  if (elementCount == 0 || gatewayAddr == 0 || gatewayAddr > 0x7FFF)
    return false;
  uint32_t candidate = std::max<uint32_t>(nextAddr, 2);
  while (candidate + elementCount - 1 <= 0x7FFF)
  {
    const uint32_t end = candidate + elementCount - 1;
    if (candidate <= gatewayAddr && gatewayAddr <= end)
    {
      candidate = gatewayAddr + 1;
      continue;
    }
    uint32_t afterConflict = candidate;
    DeviceManager::getInstance()->ForEach([&](Device *device) {
      if (!device || device->GetProtocol() != BLE_DEVICE)
        return;
      const uint32_t deviceStart = device->GetAddr();
      const uint32_t deviceEnd = deviceStart +
          std::max<uint32_t>(device->GetNumElement(), 1) - 1;
      if (candidate <= deviceEnd && end >= deviceStart)
        afterConflict = std::max(afterConflict, deviceEnd + 1);
    });
    if (afterConflict == candidate)
    {
      nextAddr = static_cast<uint16_t>(candidate);
      return true;
    }
    candidate = afterConflict;
  }
  LOGE("ADDRESS_CONFLICT: no free unicast range for %u elements",
       static_cast<unsigned>(elementCount));
  return false;
}

BleProtocol *BleProtocol::getInstance()
{
  if (!bleProtocol)
  {
#ifdef ESP_PLATFORM
    bleProtocol = new BleProtocol(UART_NUM_1, UART_TX_PIN, UART_RX_PIN, 115200);
#else
    bleProtocol = new BleProtocol((char *)BLE_UART_PORT, B115200);
#endif
  }
  return bleProtocol;
}

#ifdef ESP_PLATFORM
BleProtocol::BleProtocol(uart_port_t num, int txPin, int rxPin, int baudrate)
    : Uart(num, txPin, rxPin, baudrate)
#else
BleProtocol::BleProtocol(char *uartPort, int baudrate)
    : Uart(uartPort, baudrate, 100000)
#endif
{
  nextAddr = 0;
  haveNewMac = false;
  haveGetMacRsp = true;
  isProvisioning = false;
  isInitKey = false;
  tid = 0;
  isCongested = false;
}

BleProtocol::~BleProtocol() {}

static void AddDeviceThread(void *data)
{
  BleProtocol *bleProtocol = (BleProtocol *)data;
  int timeout = 0;
  while (1)
  {
    if (BleProtocol::getInstance()->IsProvision())
    {
      scan_device_message_t scan_device_message;
      if (BleProtocol::getInstance()->haveNewMac)
      {
        timeout = 0;
        memcpy(&scan_device_message,
               &BleProtocol::getInstance()->scanDeviceMessage,
               sizeof(scan_device_message_t));
        BleProtocol::getInstance()->AddDevice(&scan_device_message);
#ifdef ESP_PLATFORM
        LOGI("AddDeviceThread stack free after provisioning: %u bytes",
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(NULL)));
#endif
        BleProtocol::getInstance()->haveGetMacRsp = true;
        BleProtocol::getInstance()->haveNewMac = false;
      }
      else
      {
        timeout++;
        if (timeout >= 300)
        {
          timeout = 0;
          BleProtocol::getInstance()->SetProvisioning(false);
          BleProtocol::getInstance()->StopScan();
          // Gateway::getInstance()->SendTimeoutProvisionDev();
        }
      }
      SLEEP_MS(10);
    }
    SLEEP_MS(1000);
  }
}

static void HandleOpcodeBle(void *data)
{
  BleProtocol *bleProtocol = (BleProtocol *)data;
  message_rsp_st *message_rsp = NULL;
  while (1)
  {
    if (BleProtocol::getInstance()->GetOpcodeExceptionMessage(&message_rsp) ==
        CODE_OK)
    {
      BleProtocol::getInstance()->CheckOpcodeException(message_rsp);
      free(message_rsp);
    }
    SLEEP_MS(100);
  }
}

void BleProtocol::Init()
{
  Uart::Init();
#ifdef ESP_PLATFORM
  // Provisioning builds device configuration and the MQTT payload on this task.
  // Leave headroom for the nested JSON and device-config calls.
  if (xTaskCreate(AddDeviceThread, "AddDeviceThread", 20480, this, 10, NULL) !=
      pdPASS)
  {
    LOGE("Failed to create task");
    SetLedService(false);
  }
  if (xTaskCreate(HandleOpcodeBle, "HandleOpcodeBle", 6144, this, 10, NULL) !=
      pdPASS)
  {
    LOGE("Failed to create task");
    SetLedService(false);
  }
#else
  thread addDeviceThreadThread(AddDeviceThread, this);
  addDeviceThreadThread.detach();
  thread handleOpcodeBleThread(HandleOpcodeBle, this);
  handleOpcodeBleThread.detach();
#endif

  SLEEP_MS(100); // wait for thread start
}

void BleProtocol::InitKey()
{
  LOGI("InitKey start");
  const int maxGetNetKeyRetries = 3;
  int retry = 0;
  while (GetNetKey())
  {
    retry++;
    if (retry >= maxGetNetKeyRetries)
    {
      LOGE("InitKey failed: BLE gateway did not respond after %d retries",
           maxGetNetKeyRetries);
      isInitKey = false;
      Util::LedBle(false);
      return;
    }
#ifdef ESP_PLATFORM
    FlashLedInternet();
#else
    Util::LedBle(true);
    sleep(4);
    Util::LedBle(false);
#endif
  }
  LOGI("InitKey completed: chipHasValidKeys=%d", chipHasValidKeys ? 1 : 0);
  isInitKey = true;
  CheckKeyBle();
  Util::LedBle(isInitKey);
}

bool BleProtocol::HasProvisionedMeshConfig()
{
  StoredMeshConfig mesh{};
  return isInitKey && ReadStoredMeshConfig(mesh);
}

bool BleProtocol::NeedsMeshRecovery()
{
  StoredMeshConfig mesh{};
  return chipHasValidKeys && !ReadStoredMeshConfig(mesh);
}

int BleProtocol::ConfigureProvisionedMesh(const Json::Value &config)
{
  StoredMeshConfig mesh{};
  if (!ParseStoredMeshConfig(config, mesh))
  {
    LOGE("Homegy mesh configuration is missing or invalid");
    return CODE_ERROR;
  }
  if (!Config::getInstance()->SaveProvisionConfigJson(config.toString()))
  {
    LOGE("Could not save Homegy mesh configuration");
    return CODE_ERROR;
  }
  InitKey();
  return isInitKey ? CODE_OK : CODE_ERROR;
}

void BleProtocol::CheckKeyBle()
{
  LOGI("CheckKeyBle start, chipHasValidKeys=%d",
       chipHasValidKeys ? 1 : 0);
  Gateway *gateway = Gateway::getInstance();
  StoredMeshConfig mesh{};
  if (!ReadStoredMeshConfig(mesh) && CreateFreshHomegyMeshConfig())
  {
    if (chipHasValidKeys)
    {
      LOGW("Replacing orphaned BLE chip Mesh with new Homegy credentials; "
           "old nodes must be provisioned again");
    }
    else
    {
      LOGI("First-run Homegy Mesh configuration is ready");
    }
  }
  if (ReadStoredMeshConfig(mesh))
  {
    LOGI("Restoring provisioned BLE mesh: gateway=0x%04X netKeyIndex=0x%03X "
         "appKeyIndex=0x%03X ivIndex=0x%08lX",
         mesh.gatewayAddr, mesh.netKeyIndex, mesh.appKeyIndex,
         static_cast<unsigned long>(mesh.ivIndex));
    uint8_t expectedNetKey[16];
    LoadMeshKey(mesh.netKey, expectedNetKey);
    LOGI("BLE chip mesh before restore: netKeyMatch=%d netKeyIndex=0x%03X "
         "ivIndex=0x%08lX",
         chipHasValidKeys && memcmp(pro_net_info.netKey, expectedNetKey, 16) == 0,
         chipHasValidKeys ? bswap_16(pro_net_info.key_index) : 0,
         static_cast<unsigned long>(chipHasValidKeys ?
             bswap_32(pro_net_info.iv_index) : 0));
    const bool sameNetwork = chipHasValidKeys &&
        memcmp(pro_net_info.netKey, expectedNetKey, 16) == 0 &&
        bswap_16(pro_net_info.key_index) == mesh.netKeyIndex &&
        bswap_32(pro_net_info.iv_index) == mesh.ivIndex;
    if ((!chipHasValidKeys && SetNodeParams() != CODE_OK) ||
        (!sameNetwork &&
         UpdateNetKey(mesh.gatewayAddr, mesh.netKey, mesh.ivIndex,
                      mesh.netKeyIndex) != CODE_OK) ||
        UpdateDevKey(mesh.gatewayAddr, mesh.deviceKey) != CODE_OK ||
        UpdateAppKey(mesh.appKey, mesh.appKeyIndex) != CODE_OK)
    {
      LOGE("Could not restore provisioned BLE mesh");
      isInitKey = false;
      return;
    }
    if (GetNetKey() != CODE_OK || !chipHasValidKeys ||
        memcmp(pro_net_info.netKey, expectedNetKey, 16) != 0 ||
        bswap_16(pro_net_info.key_index) != mesh.netKeyIndex ||
        bswap_32(pro_net_info.iv_index) != mesh.ivIndex)
    {
      LOGE("BLE module did not retain the configured Homegy NetKey/index/IV");
      isInitKey = false;
      return;
    }
    gateway->SetBleAddr(mesh.gatewayAddr);
    gateway->SetBleIvIndex(mesh.ivIndex);
    gateway->SetBleNetkey(mesh.netKey);
    gateway->SetBleAppkey(mesh.appKey);
    gateway->SetBleDevicekey(mesh.deviceKey);
    Database *db = Database::getInstance();
    const bool persisted =
        db->GatewayUpdateUnicast(mesh.gatewayAddr) == CODE_OK &&
        db->GatewayUpdateIvIndex(mesh.ivIndex) == CODE_OK &&
        db->GatewayUpdateNetKey(mesh.netKey) == CODE_OK &&
        db->GatewayUpdateAppKey(mesh.appKey) == CODE_OK &&
        db->GatewayUpdateDeviceKey(mesh.deviceKey) == CODE_OK;
    if (!persisted || EnableReporting() != CODE_OK)
    {
      LOGE("Could not finish Homegy gateway mesh initialization");
      isInitKey = false;
      return;
    }
    DeviceManager::getInstance()->ForEach([&](Device *device) {
      if (device && device->GetProtocol() == BLE_DEVICE &&
          UpdateDeviceKeyDev(device->GetAddr(), device->GetNumElement(),
                             device->GetDeviceKey()) != CODE_OK)
        LOGE("Restore DeviceKey failed for addr 0x%04X", device->GetAddr());
    });
    return;
  }
  if (chipHasValidKeys)
  {
    LOGE("BLE module has an existing Mesh but its AppKey/gateway configuration "
         "is unavailable; restore the matching Homegy configuration before scanning");
  }
  else
  {
    LOGE("Homegy mesh configuration is incomplete; BLE provisioning disabled");
  }
  isInitKey = false;
}

int BleProtocol::GetOpcodeExceptionMessage(message_rsp_st **data)
{
  int rs = CODE_ERROR;
  vectorCheckOpcodeMtx.lock();
  if (messageCheckOpcodeList.size() > 0)
  {
    *data = messageCheckOpcodeList[0];
    messageCheckOpcodeList.erase(messageCheckOpcodeList.begin());
    rs = CODE_OK;
  }
  vectorCheckOpcodeMtx.unlock();
  return rs;
}

void BleProtocol::CheckOpcodeException(message_rsp_st *message_rsp)
{
  Gateway::getInstance()->SetLastTimePingGwBle(time(NULL));
  switch (message_rsp->opcode)
  {
  case HCI_GATEWAY_CMD_UPDATE_MAC:
    if (IsProvision() && !haveNewMac)
    {
      memcpy(&scanDeviceMessage, message_rsp->data,
             sizeof(scan_device_message_t));
      haveNewMac = true;
      haveGetMacRsp = false;
    }
    break;

  case HCI_GATEWAY_RSP_OP_CODE:
  case HCI_GATEWAY_RSP_UNICAST_DIR:
  {
    typedef struct __attribute__((packed))
    {
      uint16_t dev_addr;
      uint16_t gw_addr;
      uint8_t data[100];
    } data_message_t;
    data_message_t *data_message = (data_message_t *)message_rsp->data;
    // BLE chip response layout: [src_addr LE (chip/gw mesh addr)][dst_addr BE
    // (target device)][opcode+data] dev_addr = bytes[0:2] = chip's own mesh
    // addr (e.g. 0x7760) gw_addr  = bytes[2:4] = device addr in big-endian
    // (e.g. `00 E4` = 0x00E4)
    uint16_t opcode = data_message->data[0] | (data_message->data[1] << 8);

    // Resolve device: try all address interpretations
    uint16_t swappedDevAddr = bswap_16(data_message->dev_addr);
    uint16_t swappedGwAddr = bswap_16(data_message->gw_addr);

    // LOGD("Resolving device for status frame: dev_addr=0x%04X, gw_addr=0x%04X, opcode=0x%04X",
    //      data_message->dev_addr, data_message->gw_addr, opcode);

    DeviceBle *deviceBle = DeviceManager::getInstance()->GetDeviceBleFromAddr(data_message->dev_addr);
    uint16_t resolvedAddr = data_message->dev_addr;

    if (!deviceBle)
    {
      deviceBle = DeviceManager::getInstance()->GetDeviceBleFromAddr(swappedDevAddr);
      if (deviceBle)
        resolvedAddr = swappedDevAddr;
    }
    if (!deviceBle)
    {
      deviceBle = DeviceManager::getInstance()->GetDeviceBleFromAddr(data_message->gw_addr);
      if (deviceBle)
        resolvedAddr = data_message->gw_addr;
    }
    if (!deviceBle)
    {
      deviceBle = DeviceManager::getInstance()->GetDeviceBleFromAddr(swappedGwAddr);
      if (deviceBle)
        resolvedAddr = swappedGwAddr;
    }

    const bool isPendingProvisionNode =
        IsProvision() &&
        (data_message->dev_addr == nextAddr || swappedDevAddr == nextAddr ||
         data_message->gw_addr == nextAddr || swappedGwAddr == nextAddr);

    if (!deviceBle && !isPendingProvisionNode)
    {
      // LOGE("Could not resolve device for status frame. Tried: 0x%04X, 0x%04X, 0x%04X, 0x%04X",
      //  data_message->dev_addr, swappedDevAddr, data_message->gw_addr, swappedGwAddr);
    }

    if (deviceBle)
    {
      bool wasOffline = !deviceBle->IsOnline();
      int payloadLen = message_rsp->len - 6;
      bool isBroadcastOnOffResponse =
          payloadLen >= 8 && data_message->data[0] == 0xE0 &&
          data_message->data[1] == 0x11 && data_message->data[2] == 0x02 &&
          data_message->data[5] == 0xF1 && data_message->data[6] == 0x00;

      // Debug logging for broadcast response pattern
      if (payloadLen >= 8 && data_message->data[0] == 0xE0 &&
          data_message->data[1] == 0x11 && data_message->data[2] == 0x02)
      {
        LOGD("Potential broadcast response from device 0x%04X:", data_message->dev_addr);
        LOGD("  len=%d, pattern: [%02X %02X %02X %02X %02X %02X %02X %02X]",
             message_rsp->len, data_message->data[0], data_message->data[1],
             data_message->data[2], data_message->data[3], data_message->data[4],
             data_message->data[5], data_message->data[6], data_message->data[7]);
        LOGD("  Check: [5]=0x%02X (expect 0xF1), [6]=0x%02X (expect 0x00), match=%s",
             data_message->data[5], data_message->data[6],
             isBroadcastOnOffResponse ? "YES" : "NO");
      }

      deviceBle->UpdateLastTimeActive();

      // Frame 0xB5 is only the gateway ack, so it must not publish online state.
      const bool isGatewayAck = message_rsp->opcode == HCI_GATEWAY_RSP_OP_CODE;

      // Bất kỳ bản tin nào từ thiết bị đều được tính là đã phản hồi bản tin broadcast
      if (!isGatewayAck)
        Gateway::getInstance()->MarkBleBroadcastResponse(deviceBle);

      if (wasOffline && !isBroadcastOnOffResponse && !isGatewayAck)
      {
        deviceBle->lastOnlineState = true;
        Gateway::getInstance()->PushDeviceOnlineState(deviceBle, true);
      }
      if (isBroadcastOnOffResponse)
      {
        deviceBle->HandleBleBroadcastStatus(data_message->data, payloadLen,
                                            resolvedAddr);
      }
      else if (deviceBle->HandleBleStatus(opcode, data_message->data,
                                          message_rsp->len - 6, resolvedAddr))
      {
      }
      else if ((opcode == G_ONOFF_SET || opcode == G_ONOFF_STATUS) &&
               data_message->data[0] == 0x82)
      {
        LOGW("Device on/off status frame: devAddr=0x%04X opcode=0x%04X len=%d "
             "data=[%02X %02X %02X %02X %02X %02X]",
             resolvedAddr, opcode, message_rsp->len, data_message->data[0],
             data_message->data[1], data_message->data[2],
             data_message->data[3], data_message->data[4],
             data_message->data[5]);
        // Bỏ qua opcode 2 byte, payload state bắt đầu từ data[2]
        // Tuy nhiên ModuleOnOff::InputData mong đợi mảng bọc chứa opcode 0x82 nên truyền vào data_message->data
        deviceBle->DeviceInputData(data_message->data, message_rsp->len - 6,
                                   resolvedAddr);
      }
      else
      {
        deviceBle->DeviceInputData(data_message->data, message_rsp->len - 6,
                                   resolvedAddr);
      }

      // A 0xB5 frame only confirms that the UART BLE gateway accepted the
      // outgoing command. Complete an acknowledged OnOff command only when
      // the mesh node itself reports Generic OnOff Status (82 04).
      if (opcode == G_ONOFF_STATUS && payloadLen >= 3)
      {
        uint8_t statusValue = data_message->data[2];
        if (payloadLen >= 5)
          statusValue = data_message->data[4];
        lastOnOffStatusAddr.store(resolvedAddr, std::memory_order_relaxed);
        lastOnOffStatusValue.store(statusValue, std::memory_order_relaxed);
        onOffStatusSequence.fetch_add(1, std::memory_order_release);
      }
    }
    else if (!isPendingProvisionNode &&
             data_message->dev_addr != Gateway::getInstance()->GetBleAddr())
    {
      LOGW("Not found device: src=0x%04X dst_raw=0x%04X dst_swapped=0x%04X",
           data_message->dev_addr, data_message->gw_addr,
           bswap_16(data_message->gw_addr));
    }
    break;
  }

  case HCI_GATEWAY_CMD_ONLINE_STS: // 0x9D
  {
    // Match root branch behavior: ignore 0x919D online-status frames.
    // Root relies on broadcast responses/timeouts and normal status frames
    // instead of consuming this frame to flip cloud online state.
    LOGI("Ignoring HCI_GATEWAY_CMD_ONLINE_STS (0x9D), len=%d", message_rsp->len);
    break;
  }

  case HCI_GATEWAY_CMD_SEND_NODE_INFO:
  {
    for (int i = 0; i < 16; i++)
    {
      deviceKey[i] = message_rsp->data[i + 4];
    }
    break;
  }
  default:
    break;
  }
}

int BleProtocol::OnMessage(unsigned char *data, int len)
{
  // LOGD("OnMessage len: %d", len);
  uint8_t *d = data;
  int l = len;
  message_rsp_st *message_rsp = NULL;
  message_rsp_st *old_message_rsp = NULL;
  bool is_dupplicate = false;
  bool match;
  Util::LedBle(false);
  Util::LedServiceLock();
#ifdef ESP_PLATFORM
  SetLedService(false);
#endif
  while (l >= 4)
  {
    message_rsp = (message_rsp_st *)d;
    if (message_rsp->len >= 2 && message_rsp->len <= 36)
    {
      if (message_rsp->magic == 0x80 || message_rsp->magic == 0x90 ||
          message_rsp->magic == 0x91 || message_rsp->magic == 0x92 ||
          message_rsp->magic == 0xfa || message_rsp->magic == 0xda)
      {
        if (haveGetMacRsp ||
            (!haveGetMacRsp &&
             message_rsp->opcode != HCI_GATEWAY_CMD_UPDATE_MAC))
        {
          uint16_t packageLen = message_rsp->len + 2;
          is_dupplicate = false;
          if (old_message_rsp && message_rsp->len == old_message_rsp->len)
          {
            is_dupplicate = true;
            for (int i = 0; i < message_rsp->len; i++)
            {
              if (message_rsp->data[i] != old_message_rsp->data[i])
              {
                is_dupplicate = false;
                break;
              }
            }
          }
          if (!is_dupplicate)
          {
            if (message_rsp->len >= 2 && message_rsp->len <= l - 2)
            {
              // LOGD("onMessage opcode: 0x%02X, len: %d", message_rsp->opcode,
              // message_rsp->len);
              messageRespListMtx.lock();
              for (auto &messageResp : messageRespList)
              {
                if (message_rsp->opcode == messageResp->opcode)
                {
                  match = true;
                  if (messageResp->compare_data)
                  {
                    int payloadLen = message_rsp->len - 2;
                    if (messageResp->compare_position < 0 ||
                        messageResp->compare_len < 0 ||
                        messageResp->compare_position +
                                messageResp->compare_len >
                            payloadLen)
                    {
                      match = false;
                    }
                    else
                    {
                      for (int i = 0; i < messageResp->compare_len; i++)
                      {
                        if (message_rsp->data[messageResp->compare_position +
                                              i] !=
                            messageResp->compare_data[i])
                          match = false;
                      }
                    }
                  }
                  if (match)
                  {
                    LOGI("Match found for opcode: 0x%02X, len=%d, "
                         "compare_pos=%d, compare_len=%d",
                         message_rsp->opcode, message_rsp->len,
                         messageResp->compare_position,
                         messageResp->compare_len);
                    if (messageResp->compare_data &&
                        messageResp->compare_len > 0)
                    {
                      std::string got = "";
                      for (int ci = 0; ci < messageResp->compare_len; ci++)
                      {
                        char buf[8];
                        sprintf(buf, "%02X",
                                message_rsp
                                    ->data[messageResp->compare_position + ci]);
                        got += buf;
                      }
                      std::string expected = "";
                      for (int ci = 0; ci < messageResp->compare_len; ci++)
                      {
                        char buf[8];
                        sprintf(buf, "%02X", messageResp->compare_data[ci]);
                        expected += buf;
                      }
                      LOGI("Compare got=%s expected=%s", got.c_str(),
                           expected.c_str());
                    }
                    messageResp->status = true;
                    if (messageResp->len)
                    {
                      *(messageResp->len) = message_rsp->len - 2;
                      if (*(messageResp->len) > 0)
                        if (messageResp->data)
                          memcpy(messageResp->data, message_rsp->data,
                                 *messageResp->len);
                    }
                  }
                  else if (messageResp->compare_data)
                  {
                    std::string got = "";
                    std::string expected = "";
                    int payloadLen = message_rsp->len - 2;
                    if (messageResp->compare_position +
                            messageResp->compare_len <=
                        payloadLen)
                    {
                      for (int ci = 0; ci < messageResp->compare_len; ci++)
                      {
                        char buf[8];
                        sprintf(buf, "%02X",
                                message_rsp->data
                                    [messageResp->compare_position + ci]);
                        got += buf;
                        sprintf(buf, "%02X", messageResp->compare_data[ci]);
                        expected += buf;
                      }
                      LOGW("Match failed for opcode 0x%02X compare mismatch "
                           "got=%s expected=%s",
                           message_rsp->opcode, got.c_str(), expected.c_str());
                    }
                    else
                    {
                      LOGW("Match failed for opcode 0x%02X compare too short "
                           "len=%d compare_pos=%d compare_len=%d",
                           message_rsp->opcode, payloadLen,
                           messageResp->compare_position,
                           messageResp->compare_len);
                    }
                  }
                }
              }
              messageRespListMtx.unlock();
              // CheckOpcodeException(message_rsp);
              vectorCheckOpcodeMtx.lock();
              if (messageCheckOpcodeList.size() <
                  BLE_CHECK_OPCODE_BUFFER_MAX_SIZE)
              {
#ifdef ESP_PLATFORM
                message_rsp_st *messageCheckOpcode =
                    (message_rsp_st *)heap_caps_malloc_prefer(
                        packageLen, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM,
                        MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
#else
                message_rsp_st *messageCheckOpcode =
                    (message_rsp_st *)malloc(packageLen);
#endif
                if (messageCheckOpcode)
                {
                  memcpy(messageCheckOpcode, message_rsp, packageLen);
                  messageCheckOpcodeList.push_back(messageCheckOpcode);
                }
                else
                {
                  LOGE("malloc failed at OnMessage with len=%d", packageLen);
                }
              }
              vectorCheckOpcodeMtx.unlock();
            }
            else if (message_rsp->len < 2 || message_rsp->len > 36)
            {
              LOGW("Wrong uart data");
              l = 0;
              break;
            }
            else
            {
              break;
            }
          }
          old_message_rsp = message_rsp;
          l -= packageLen;
          d += packageLen;
        }
        else
        {
          d++;
          l--;
        }
      }
      else
      {
        d++;
        l--;
      }
    }
    else
    {
      d++;
      l--;
    }
    usleep(300);
  }
  Util::LedBle(true);
  Util::LedServiceUnlock();
#ifdef ESP_PLATFORM
  SLEEP_MS(70);
  SetLedService(true);
#endif
  return l;
}

int BleProtocol::SendMessage(uint16_t opReq, uint8_t *dataReq, int lenReq,
                             uint8_t opRsp, uint8_t *dataRsp, int *lenRsp,
                             uint32_t timeout, uint8_t *compare_data,
                             int compare_position, int compare_len)
{
  uint32_t countDelay = timeout / 10;
  mtxWaitSendUart.lock();
  int rs = CODE_OK;
  message_rsp_list_st message_rsp_list = {
      .status = false,
      .opcode = opRsp,
      .len = lenRsp,
      .data = dataRsp,
      .compare_data = compare_data,
      .compare_position = compare_position,
      .compare_len = compare_len,
  };
  if (opRsp)
  {
    // TODO: add mutex
    messageRespListMtx.lock();
    messageRespList.push_back(&message_rsp_list);
    messageRespListMtx.unlock();
  }
  message_req_st message_req = {
      .opcode = opReq,
  };
  for (int i = 0; i < lenReq; i++)
  {
    message_req.data[i] = dataReq[i];
  }
  LOGD("SendMessage start: opReq=0x%04X opRsp=0x%02X lenReq=%d timeout=%d "
       "compare_pos=%d compare_len=%d",
       opReq, opRsp, lenReq, timeout, compare_position, compare_len);
  if (compare_data && compare_len > 0)
  {
    std::string cmp = "";
    for (int i = 0; i < compare_len; i++)
    {
      char tmp[8];
      sprintf(tmp, "%02X", compare_data[i]);
      cmp += tmp;
    }
    LOGD("SendMessage compare_data=%s", cmp.c_str());
  }
  Write((uint8_t *)&message_req, lenReq + 2);
  if (opRsp)
  {
    while (!message_rsp_list.status && countDelay--)
    {
      SLEEP_MS(10);
    }
    if (!message_rsp_list.status)
    {
      LOGE("SendMessage timeout waiting for opRsp=0x%02X opReq=0x%04X", opRsp,
           opReq);
      rs = CODE_ERROR;
    }
    messageRespListMtx.lock();
    messageRespList.erase(remove(messageRespList.begin(), messageRespList.end(),
                                 &message_rsp_list),
                          messageRespList.end());
    messageRespListMtx.unlock();
  }
  else
  {
    usleep(1000 * timeout);
  }
  mtxWaitSendUart.unlock();
  return rs;
  // return Write(dataReq, lenReq);
}

int BleProtocol::GetNetKey()
{
  LOGD("GetNetKey");
  uint8_t d = HCI_GATEWAY_CMD_GET_PRO_SELF_STS;
  uint8_t dataRsp[100];
  int lenRsp;
  int rs = SendMessage(SYSTEM_REQ, &d, 1, HCI_GATEWAY_CMD_PRO_STS_RSP, dataRsp, &lenRsp, 5000);
  if (rs == CODE_OK)
  {
    if (lenRsp < static_cast<int>(sizeof(pro_net_info_t) + 1))
    {
      LOGE("GetNetKey response is too short: %d", lenRsp);
      chipHasValidKeys = false;
      return CODE_ERROR;
    }
    chipHasValidKeys = dataRsp[0] == 0x01;
    memcpy(&pro_net_info.netKey[0], &dataRsp[1], sizeof(pro_net_info_t));

    nextAddr = pro_net_info.unicast_address;
    if (nextAddr == 0)
      nextAddr = 2;
    uint32_t maxAddr = DeviceManager::getInstance()->GetMaxAddrBle();
    if (nextAddr <= maxAddr)
      nextAddr = std::min<uint32_t>(maxAddr + BLE_MAX_ELEMENT, 0xFFFF);
    LOGW("nextAddr: 0x%04X - %d", nextAddr, nextAddr);
    LOGI("GetNetKey parsed: chipHasValidKeys=%d ivIndex=0x%08lX",
         chipHasValidKeys ? 1 : 0,
         static_cast<unsigned long>(bswap_32(pro_net_info.iv_index)));

    memcpy(netKey, pro_net_info.netKey, sizeof(netKey));
  }
  else
  {
    LOGE("Send GetNetKey error, rs: %d", rs);
    rs = CODE_ERROR;
  }
  return rs;
}

int BleProtocol::SetNodeResponsLive()
{
  LOGD("SetNodeResponsLive");
  uint8_t d = 0x01;
  return SendMessage(0xFFEC, &d, 1, 0, 0, 0, 1000);
}

int BleProtocol::StartScan()
{
  LOGD("StartScan BLE");
  uint8_t d = HCI_GATEWAY_CMD_START;
  // SetProvisioning(true);
  int rs = SendMessage(SYSTEM_REQ, &d, 1, 0, 0, 0, 0);
  if (rs)
  {
    LOGE("Send start scan error, rs: %d", rs);
  }
  return rs;
}

int BleProtocol::StopScan()
{
  LOGD("StopScan");
  uint8_t d = HCI_GATEWAY_CMD_STOP;
  SetProvisioning(false);
  int rs = SendMessage(SYSTEM_REQ, &d, 1, 0, 0, 0, 0);
  if (rs)
  {
    LOGE("Send stop scan error, rs: %d", rs);
  }
  return rs;
}

int BleProtocol::ResetBle()
{
  LOGD("ResetBle");
  uint8_t d = HCI_GATEWAY_CMD_RESET;
  int rs = SendMessage(SYSTEM_REQ, &d, 1, 0, 0, 0, 8000);
  if (rs)
  {
    LOGE("Send reset factory error, rs: %d", rs);
  }
  return rs;
}

int BleProtocol::ResetFactory()
{
  LOGD("ResetFactory");
  memset(&pro_net_info, 0x00, sizeof(pro_net_info));
  memset(netKey, 0x00, sizeof(netKey));
  memset(appKey, 0x00, sizeof(appKey));
  memset(gwKey, 0x00, sizeof(gwKey));
  memset(deviceKey, 0x00, sizeof(deviceKey));
  nextAddr = 0;
  chipHasValidKeys = false;
  isInitKey = false;
  return CODE_OK;
}

bool BleProtocol::IsProvision() { return isProvisioning; }

void BleProtocol::SetProvisioning(bool isProvision)
{
  this->isProvisioning = isProvision;
}

int BleProtocol::AddDevice(scan_device_message_t *scan_device_message)
{
  if (!isInitKey)
  {
    LOGE("Cannot provision device: BLE mesh initialization failed");
    return CODE_ERROR;
  }
  // TODO: convert to non-blocking func
  LOGD("AddDevice");
  // #ifdef ESP_PLATFORM
  // 	Led::TaskLedService(MODE_BLINK);
  // #endif
  uint16_t version = 0;
  uint32_t deviceType = 0;
  // Telink reports the scan MAC least-significant byte first. Store the
  // canonical MAC; CheckType uses the original scan bytes as reverseMac.
  uint8_t realMac[6];
  for (size_t i = 0; i < sizeof(realMac); ++i)
    realMac[i] = scan_device_message->mac[sizeof(realMac) - 1 - i];
  string mac = Util::ConvertU32ToHexString(realMac, sizeof(realMac));
  LOGI("Scan device mac 0x%s, rssi: %i", mac.c_str(),
       scan_device_message->rssi);
  const bool zeroMac = std::all_of(realMac, realMac + 6,
                                   [](uint8_t b) { return b == 0; });
  const bool broadcastMac = std::all_of(realMac, realMac + 6,
                                        [](uint8_t b) { return b == 0xFF; });
  if (mac.size() != 12 || zeroMac || broadcastMac)
  {
    LOGE("INVALID_MAC in BLE scan result");
    return CODE_ERROR;
  }
  ProvisionedDeviceInfo advertisedInfo{};
  const uint32_t advertisedType = AdvertisedDeviceType(*scan_device_message);
  if (!ResolveProvisionedDeviceInfo(advertisedType, advertisedInfo))
  {
    LOGE("UNKNOWN_PRODUCT: scan UUID type=0x%06X MAC=%s", advertisedType,
         mac.c_str());
    return CODE_ERROR;
  }
  if (DeviceManager::getInstance()->GetDeviceFromId(mac))
  {
    LOGW("Device %s is already provisioned; skipping duplicate activation",
         mac.c_str());
    return CODE_ERROR;
  }
  int rs = CODE_ERROR;
  if (IsProvision() && !SelectMac(scan_device_message->mac))
  {
    if (IsProvision() && !GetNetKey())
    {
      StoredMeshConfig meshForAddress{};
      if (!ReadStoredMeshConfig(meshForAddress))
        return CODE_ERROR;
      nextAddr = std::max(nextAddr, meshForAddress.nextAddress);
      if (IsProvision() &&
          ReserveHomegyAddress(nextAddr, advertisedInfo.elementCount,
                               Gateway::getInstance()->GetBleAddr()) &&
          !Provision(nextAddr))
      {
        if (IsProvision() && !BindingAll())
        {
          uint16_t gatewayAddr = Gateway::getInstance()->GetBleAddr();
          uint16_t indexDevice = DeviceManager::getInstance()->GetDeviceCount() + 1;
          int saveGwRs = CODE_ERROR;
          for (int retry = 0; retry < 2 && IsProvision(); ++retry)
          {
            saveGwRs = SetGwAddr(nextAddr, gatewayAddr, indexDevice);
            if (saveGwRs == CODE_OK)
              break;
          }
          if (IsProvision() && saveGwRs == CODE_OK)
          {
            int checkTypeRs = CODE_ERROR;
            for (int retry = 0; retry < 2 && IsProvision(); ++retry)
            {
              checkTypeRs = GetDeviceType(scan_device_message->mac, nextAddr,
                                          deviceType, version);
              if (checkTypeRs == CODE_OK)
                break;
            }
            if (IsProvision() && checkTypeRs == CODE_OK)
            {
              ProvisionedDeviceInfo deviceInfo{};
              if (!ResolveProvisionedDeviceInfo(deviceType, deviceInfo))
              {
                if (!ResolveProvisionedDeviceInfo(advertisedType, deviceInfo))
                {
                  LOGE("Unsupported device type: CheckType=0x%06X scan=0x%06X",
                       deviceType, advertisedType);
                  goto add_device_done;
                }
                LOGW("CheckType returned unsupported type 0x%06X; using scan "
                     "UUID type 0x%06X", deviceType, advertisedType);
                deviceType = advertisedType;
                version = (static_cast<uint16_t>(scan_device_message->uuid[11])
                           << 8) |
                          scan_device_message->uuid[12];
              }
              LOGI("Resolved provisioned device: type=0x%06X pid=%s model=%s "
                   "elements=%u",
                   deviceType, deviceInfo.pid.c_str(), deviceInfo.model.c_str(),
                   static_cast<unsigned>(deviceInfo.elementCount));

              const string deviceKeyString =
                  Util::uuidToStr((uint8_t *)deviceKey);
              const string deviceUuid = Util::ConvertU32ToHexString(
                  scan_device_message->uuid, sizeof(scan_device_message->uuid));
              Json::Value dataJson;
              dataJson["devicekey"] = deviceKeyString;
              dataJson["deviceKey"] = deviceKeyString;
              // The new cloud core does not allocate another identifier for a
              // freshly provisioned node. Keep the BLE DeviceKey as the stable
              // cloud id, matching the newDev contract.
              dataJson["device_id"] = deviceKeyString;
              dataJson["id"] = deviceKeyString;
              dataJson["deviceUuid"] = deviceUuid;
              dataJson["pid"] = deviceInfo.pid;
              dataJson["productId"] = deviceInfo.pid;
              dataJson["deviceType"] = Json::UInt(deviceInfo.type);
              // CheckType returns the product number in the low 16 bits and
              // the device version separately. These are the numeric Mesh
              // PID/VID fields expected by the new cloud API.
              dataJson["meshPid"] = Json::UInt(deviceType & 0xFFFF);
              dataJson["meshVid"] = Json::UInt(version);
              dataJson["meshAddress"] = nextAddr;
              dataJson["bleAddressStart"] = nextAddr;
              dataJson["bleAddressEnd"] =
                  nextAddr + deviceInfo.elementCount - 1;
              dataJson["elementCount"] = deviceInfo.elementCount;
              dataJson["elements"] = deviceInfo.elementCount;
              dataJson["gatewayAddress"] = gatewayAddr;
              dataJson["indexDevice"] = indexDevice;
              StoredMeshConfig provisionedMesh{};
              if (!ReadStoredMeshConfig(provisionedMesh))
              {
                LOGE("Homegy mesh configuration disappeared during provisioning");
                goto add_device_done;
              }
              dataJson["appKeyIndex"] = provisionedMesh.appKeyIndex;
              dataJson["version"] = version;
              dataJson["pageIndex"] = 1;
              dataJson["macAddress"] = mac;
              dataJson["devId"] = mac;
              string model = deviceInfo.model;
              string name = mac;
              Device *device = DeviceManager::getInstance()->AddDeviceNoReset(
                  mac, model, name, nextAddr, version, &dataJson);
              if (device)
              {
                if (Database::getInstance()->DeviceAdd(device) == CODE_OK)
                {
                  const uint16_t followingAddress =
                      nextAddr + deviceInfo.elementCount;
                  if (!SaveNextHomegyAddress(followingAddress))
                  {
                    LOGE("PERSIST_FAILED: next Homegy unicast address");
                    rs = CODE_ERROR;
                  }
                  else
                  {
                    const int cloudRs =
                        Gateway::getInstance()->PublishProvisionedDevice(device);
                    if (cloudRs != CODE_OK)
                      LOGW("newDev publish failed for locally provisioned device %s, rs=%d",
                           mac.c_str(), cloudRs);
                    rs = CODE_OK;
                  }
                }
                else
                {
                  LOGE("PERSIST_FAILED for provisioned device %s", mac.c_str());
                  DeviceManager::getInstance()->DelDevice(device);
                }
              }
            }
          }
        }
      add_device_done:
        if (rs != CODE_OK)
          LOGE("Homegy provisioning incomplete at address 0x%04X; node retained for recovery",
               nextAddr);
      }
    }
  }

  if (IsProvision())
  {
    StartScan();
  }
  else
  {
    StopScan();
  }

  // #ifdef ESP_PLATFORM
  // 	Led::SetModeLedService(MODE_ON);
  // 	Led::Led::ServiceLedOnOff(MODE_ON);
  // #endif

  return rs;
}

int BleProtocol::AddPairDevice(uint32_t parentAddr, uint32_t childAddr)
{
  if (ScanStopSeftPowerRemote(parentAddr, 1) == CODE_OK)
  {
    uint16_t adrMax =
        DeviceManager::getInstance()->GetMaxAddrBle() + BLE_MAX_ELEMENT;
    LOGD("Max address is %d", adrMax);
    if (SaveSeftPowerRemote(scanDevicePairMessage, adrMax) == CODE_OK)
    {
      uint16_t version = 0;
      // uint32_t deviceType = 0;
      char tempId[100] = {0};
      sprintf((char *)tempId, "%02x%02x%02x%02x-0000-0000-0000-000000000000",
              scanDevicePairMessage.mac[0], scanDevicePairMessage.mac[1],
              scanDevicePairMessage.mac[2], scanDevicePairMessage.mac[3]);
      tempId[36] = '\0';
      string uuid(tempId);

      char tempMac[100] = {0};
      sprintf((char *)tempMac, "%02x%02x%02x%02x0000",
              scanDevicePairMessage.mac[0], scanDevicePairMessage.mac[1],
              scanDevicePairMessage.mac[2], scanDevicePairMessage.mac[3]);
      tempMac[12] = '\0';
      string mac(tempMac);

      // switch (scanDevicePairMessage.type)
      // {
      // case 1:
      //   deviceType = BLE_SEFTPOWER_REMOTE_1;
      //   break;
      // case 2:
      //   deviceType = BLE_SEFTPOWER_REMOTE_2;
      //   break;
      // case 3:
      //   deviceType = BLE_SEFTPOWER_REMOTE_3;
      //   break;
      // }
      // Device *device = DeviceManager::getInstance()->AddDevice(mac,
      // deviceType,
      //                                                          childAddr,
      //                                                          257);
      // if (device) {
      //   Device *parent =
      //       DeviceManager::getInstance()->GetDeviceBleFromAddr(parentAddr);
      //   DeviceBleSeftPowerRemote *deviceBleSeftPowerRemote =
      //       dynamic_cast<DeviceBleSeftPowerRemote *>(device);
      //   if (parent && deviceBleSeftPowerRemote) {
      //     deviceBleSeftPowerRemote->SetParentDev(parent);
      //     Json::Value dataValue;
      //     dataValue["parentId"] = parent->GetId();
      //     Database::getInstance()->DeviceAdd(device);
      //     Gateway::getInstance()->DeviceAddNew(device, dataValue);
      //     Database::getInstance()->DeviceBleChildAdd(deviceBleSeftPowerRemote,
      //                                                parent, "");
      //   }
      // }
    }
    // else
    // ScanStopSeftPowerRemote(parentAddr, 0);
  }
  else
    ScanStopSeftPowerRemote(parentAddr, 0);
  return CODE_OK;
}

int BleProtocol::SelectMac(uint8_t *mac)
{
  LOGD("SelectMac");
  uint8_t data[7];
  data[0] = HCI_GATEWAY_CMD_SET_ADV_FILTER;
  for (int i = 0; i < 6; i++)
  {
    data[i + 1] = mac[i];
  }
  return SendMessage(SYSTEM_REQ, data, sizeof(data), 0, 0, 0, 1000);
}

int BleProtocol::Provision(uint16_t deviceAddr)
{
  LOGD("Provision: deviceAddr 0x%x", deviceAddr);
  uint8_t dataRsp[100];
  int lenRsp;
  typedef struct __attribute__((packed))
  {
    uint8_t opcode;
    pro_net_info_t data_pro;
  } provision_message_t;
  provision_message_t provision_message;
  memset(&provision_message, 0x00, sizeof(provision_message));
  provision_message.opcode = HCI_GATEWAY_CMD_SET_NODE_PARA;
  memcpy(&provision_message.data_pro.netKey[0], &pro_net_info.netKey[0],
         sizeof(pro_net_info_t));
  provision_message.data_pro.unicast_address = deviceAddr;
  int rs = SendMessage(SYSTEM_REQ, (uint8_t *)&provision_message,
                       sizeof(provision_message_t),
                       HCI_GATEWAY_CMD_PROVISION_EVT, dataRsp, &lenRsp, 30000);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint8_t status;
      uint8_t data[24];
    } provision_rsp_message_t;
    provision_rsp_message_t *provision_rsp_message =
        (provision_rsp_message_t *)dataRsp;
    if (provision_rsp_message->status)
    {
      LOGD("Provision OK");
      return CODE_OK;
    }
    else
    {
      LOGW("Must hard reset Ble module");
    }
  }
  LOGW("Provision err");
  return CODE_ERROR;
}

int BleProtocol::BindingAll()
{
  LOGD("BindingAll");
  uint8_t dataRsp[100];
  int lenRsp;
  typedef struct __attribute__((packed))
  {
    uint8_t opcode;
    uint8_t rev[3];
    uint8_t appKey[16];
  } binding_all_message_t;
  binding_all_message_t binding_all_message;
  memset(&binding_all_message, 0x00, sizeof(binding_all_message));
  binding_all_message.opcode = HCI_GATEWAY_CMD_START_KEYBIND;
  for (int i = 0; i < 16; i++)
  {
    binding_all_message.appKey[i] = appKey[i];
  }
  int rs = SendMessage(SYSTEM_REQ, (uint8_t *)&binding_all_message, sizeof(binding_all_message_t), HCI_GATEWAY_CMD_KEY_BIND_EVT, dataRsp, &lenRsp, 30000);
  if (rs == CODE_OK)
  {
    if (lenRsp == 1 && dataRsp[0] == 1)
    {
      SLEEP_MS(500);
      LOGD("BindingAll OK");
      return CODE_OK;
    }
  }
  LOGW("BindingAll err");
  return CODE_ERROR;
}

int BleProtocol::SetGwAddr(uint16_t devAddr, uint16_t gwAddrSet, uint16_t indexDevice)
{
  LOGD("SetGwAddr device=0x%04X gateway=0x%04X index=%u", devAddr,
       gwAddrSet, indexDevice);
  uint8_t data[] = {
      0x00,
      0x00,
      0x00,
      0x00,
      0x02,
      0x00,
      static_cast<uint8_t>(devAddr),
      static_cast<uint8_t>(devAddr >> 8),
      0xE0,
      0x11,
      0x02,
      0xE1,
      0x00,
      0x02,
      0x00,
      static_cast<uint8_t>(gwAddrSet),
      static_cast<uint8_t>(gwAddrSet >> 8),
      static_cast<uint8_t>(indexDevice),
      static_cast<uint8_t>(indexDevice >> 8),
  };
  uint8_t dataRsp[100] = {0};
  int lenRsp = 0;
  uint8_t responseMatch[] = {
      static_cast<uint8_t>(devAddr),
      static_cast<uint8_t>(devAddr >> 8),
      // HCI 0x81 reports source LE16 and destination BE16.
      static_cast<uint8_t>(gwAddrSet >> 8),
      static_cast<uint8_t>(gwAddrSet),
      0xE1,
      0x11,
      0x02,
      0x02,
      0x00,
  };

  int rs = SendMessage(APP_REQ, data, sizeof(data), HCI_GATEWAY_RSP_UNICAST_DIR,
                       dataRsp, &lenRsp, 5000, responseMatch, 0,
                       sizeof(responseMatch));
  // UART frame length includes magic (0x91) and HCI opcode (0x81).
  // SendMessage strips those two bytes, therefore the 0x0F SaveGW response
  // shown on UART is returned here as a 13-byte payload.
  if (rs == CODE_OK && lenRsp == 13 && dataRsp[9] != 0xFF)
  {
    LOGD("SetGwAddr confirmed by device 0x%04X", devAddr);
    return CODE_OK;
  }

  LOGW("SetGwAddr did not receive vendor response from device 0x%04X",
       devAddr);
  return CODE_ERROR;
}

/**
 * @brief Gen Ble security key (6 bytes)
 *
 * @param mac mac of destination device
 * @param devAddr unicast addr of destination device
 * @param out out buffer to write key
 */
static bool genSecurityKey(const uint8_t *mac, uint16_t devAddr, uint8_t *out)
{
  if (mac == nullptr || out == nullptr)
    return false;
  static const uint8_t hgKey[16] = {
      'H', 'G', 'Y', 'b', 'l', 'u', 'e', 't',
      'o', 'o', 't', 'h', 'v', 'e', 'r', '2'};
  uint8_t input[16] = {0};
  memcpy(input, "hgysmart", 8);
  // The BLE scan MAC is already in reverse byte order relative to the
  // canonical MAC saved for cloud/device identity.
  memcpy(input + 8, mac, 6);
  input[14] = static_cast<uint8_t>(devAddr);
  input[15] = static_cast<uint8_t>(devAddr >> 8);
  uint8_t encrypted[16];

  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);

  int ret = mbedtls_aes_setkey_enc(&aes, hgKey, 128);

  if (ret != 0)
  {
    mbedtls_aes_free(&aes);
    return false;
  }
  ret = mbedtls_aes_crypt_ecb(
      &aes,
      MBEDTLS_AES_ENCRYPT,
      input,
      encrypted);

  mbedtls_aes_free(&aes);

  if (ret != 0)
    return false;

  // Lấy byte 10...15
  for (int i = 0; i < 6; i++)
  {
    out[i] = encrypted[10 + i];
  }

  return true;
}

int BleProtocol::GetDeviceType(uint8_t *mac, uint16_t devAddr,
                               uint32_t &deviceType, uint16_t &deviceVersion)
{
  LOGD("GetDeviceType");
  uint8_t checkTypeCode[6];
  LOGD("Generating security key for MAC and device address - MAC: %02X:%02X:%02X:%02X:%02X:%02X, devAddr: 0x%04X",
       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], devAddr);
  if (!genSecurityKey(mac, devAddr, checkTypeCode))
  {
    LOGE("Could not generate CheckType security key");
    return CODE_ERROR;
  }
  LOGD("Security key generated: %02X%02X%02X%02X%02X%02X",
       checkTypeCode[0], checkTypeCode[1], checkTypeCode[2],
       checkTypeCode[3], checkTypeCode[4], checkTypeCode[5]);
  uint8_t data[] = {
      0x00,
      0x00,
      0x00,
      0x00,
      0x02,
      0x00,
      static_cast<uint8_t>(devAddr),
      static_cast<uint8_t>(devAddr >> 8),
      0xE0,
      0x11,
      0x02,
      0xE1,
      0x00,
      0x03,
      0x00,
      checkTypeCode[0],
      checkTypeCode[1],
      checkTypeCode[2],
      checkTypeCode[3],
      checkTypeCode[4],
      checkTypeCode[5],
  };
  uint8_t dataRsp[100] = {0};
  int lenRsp = 0;
  uint16_t gwAddr = Gateway::getInstance()->GetBleAddr();
  if (gwAddr == 0)
    return CODE_ERROR;
  uint8_t responseMatch[] = {
      static_cast<uint8_t>(devAddr),
      static_cast<uint8_t>(devAddr >> 8),
      static_cast<uint8_t>(gwAddr >> 8),
      static_cast<uint8_t>(gwAddr),
      0xE1,
      0x11,
      0x02,
      0x03,
      0x00,
  };
  int rs = SendMessage(APP_REQ, data, sizeof(data), HCI_GATEWAY_RSP_UNICAST_DIR,
                       dataRsp, &lenRsp, 5000, responseMatch, 0,
                       sizeof(responseMatch));
  if (rs == CODE_OK && lenRsp == 15 && dataRsp[9] != 0xFF)
  {
    // CheckType encodes the 16-bit product number little-endian after the
    // one-byte vendor prefix. For example AA 04 08 means BLEHGAA0804.
    deviceType = (static_cast<uint32_t>(dataRsp[9]) << 16) |
                 (static_cast<uint32_t>(dataRsp[11]) << 8) | dataRsp[10];
    deviceVersion = (dataRsp[13] << 8) | dataRsp[14];
    LOGD("GetDeviceType OK, deviceType: 0x%06X, version: %d", deviceType,
         deviceVersion);
    return CODE_OK;
  }
  LOGW("GetDeviceType err: rs=%d len=%d status=%d", rs, lenRsp,
       lenRsp > 9 ? dataRsp[9] : -1);
  return CODE_ERROR;
}
int BleProtocol::ResetDev(uint16_t devAddr)
{
  LOGD("DeleteNode addr: 0x%04X", devAddr);
  uint8_t dataRsp[100];
  int lenRsp;

  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t payload[7];
  } delete_node_message_t;

  delete_node_message_t del_msg;
  memset(&del_msg, 0x00, sizeof(del_msg));

  del_msg.ble_message_header.devAddr = bswap_16(devAddr); // Big Endian

  // Match hc_lux GW_DeleteDevice frame after APP_REQ:
  // 00 00 00 00 00 00 <addr_hi> <addr_lo> E0 11 02 00 00 40 00
  uint8_t del_payload[] = {0xE0, 0x11, 0x02, 0x00, 0x00, 0x40, 0x00};
  memcpy(del_msg.payload, del_payload, sizeof(del_payload));

  int rs =
      SendMessage(APP_REQ, (uint8_t *)&del_msg, sizeof(delete_node_message_t),
                  0, dataRsp, &lenRsp, 1000);
  if (rs == CODE_OK)
  {
    LOGD("DeleteNode OK");
    return CODE_OK;
  }
  return CODE_ERROR;
}

int BleProtocol::ResetDelAll()
{
  LOGD("Reset all dev addr");
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint8_t data[6];
  } reset_message_t;
  reset_message_t reset_message = {0};
  memset(&reset_message, 0x00, sizeof(reset_message));
  reset_message.ble_message_header.devAddr = 0xFFFF;
  reset_message.opcodeVendor = RD_OPCODE_PROVISION;
  reset_message.vendorId = RD_VENDOR_ID;
  reset_message.opcodeRsp = RD_OPCODE_PROVISION_RSP;
  reset_message.header = 0xFFFF;
  for (int i = 0; i < 6; i++)
  {
    reset_message.data[i] = i + 1;
  }

  int rs = SendMessage(APP_REQ, (uint8_t *)&reset_message,
                       sizeof(reset_message_t), 0, 0, 0, 1000);
  if (rs == CODE_OK)
  {
    return CODE_OK;
  }
  return CODE_ERROR;
}

int BleProtocol::GetTTL(uint16_t devAddr)
{
  LOGI("GetTTL");
  uint8_t dataRsp[100];
  int lenRsp;
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint16_t opcode;
  } ttl_message_t;
  ttl_message_t ttl_message = {0};
  memset(&ttl_message, 0x00, sizeof(ttl_message));
  uint8_t getOnOffHeader[6] = {
      0x01,
      0x00, // gwAddr LITTLE ENDIAN (as seen in log)
      (uint8_t)(devAddr >> 8),
      (uint8_t)(devAddr & 0xFF), // devAddr BIG ENDIAN
      0x80,
      0x0c // Opcode CFG_DEFAULT_TTL_GET echo (80 0C)
  };
  ttl_message.ble_message_header.devAddr =
      (uint16_t)((devAddr >> 8) | (devAddr << 8)); // Swap to Big Endian for TX
  ttl_message.opcode = CFG_DEFAULT_TTL_GET;
  int rs = SendMessage(APP_REQ, (uint8_t *)&ttl_message, sizeof(ttl_message_t),
                       HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 800,
                       &getOnOffHeader[2], 2,
                       4); // Start at devAddr (index 2 in header), match 4
                           // bytes (devAddr + opcode) at offset 2 in RX
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t gwAddr;
      uint16_t devAddr;
      uint16_t opcode;
      uint8_t data[1];
    } ttl_rsp_message_t;
    ttl_rsp_message_t *ttl_rsp_message = (ttl_rsp_message_t *)dataRsp;
    if (ttl_rsp_message->opcode == CFG_DEFAULT_TTL_STATUS ||
        ttl_rsp_message->opcode == CFG_DEFAULT_TTL_GET)
    {
      return CODE_OK;
    }
  }
  LOGE("Get TTL error");
  return CODE_ERROR;
}

int BleProtocol::SendOnlineCheck(uint16_t devAddr, uint32_t typeDev,
                                 uint16_t version)
{
  // LOGV("SendOnlineCheck addr: 0x%04X", devAddr);
  switch (typeDev)
  {
  case BLE_DOWNLIGHT_SMT:
  case BLE_DOWNLIGHT_RGBCW:
    BleProtocol::UpdateLights(devAddr);
    break;
  case BLE_SWITCH_RGB_1_V2:
  case BLE_SWITCH_RGB_2_V2:
  case BLE_SWITCH_RGB_3_V2:
  case BLE_SWITCH_RGB_4_V2:
    BleProtocol::UpdateStatusRelaySwitch(devAddr, typeDev);
    break;
  default:
    BleProtocol::GetTTL(devAddr);
    break;
  }
  return CODE_OK;
}

int BleProtocol::GetDevicesOnOffBroadcast()
{
  uint8_t data[] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xE0,
      0x11, 0x02, 0x00, 0x00, 0xF1, 0x00, 0x01, 0x00};
  return SendMessage(APP_REQ, data, sizeof(data), 0, 0, 0, 1000);
}

int BleProtocol::SetOnOffLight(uint16_t devAddr, uint8_t onoff,
                               uint16_t transition, bool ack)
{
  lock_guard<mutex> commandLock(mtxOnOffCommand);
  const bool wasCongested = isCongested.exchange(true);
  LOGD("Set OnOff addr: 0x%04X value %d", devAddr, onoff);
  typedef struct __attribute__((packed))
  {
    uint8_t reserved[6];
    uint16_t devAddr;
    uint16_t opcode;
    uint8_t onoff;
  } onoff_message_t;
  onoff_message_t onoff_message;
  memset(&onoff_message, 0x00, sizeof(onoff_message));

  // Provisioning and RX store addresses in logical host order (0x0011).
  // The APP_REQ destination field is little-endian, therefore assigning the
  // host value emits 11 00. Swapping here emitted 00 11 and addressed node
  // 0x1100 instead.
  onoff_message.devAddr = devAddr;
  onoff_message.onoff = onoff;

  int rs = CODE_ERROR;
  if (ack)
  {
    onoff_message.opcode = G_ONOFF_SET;
    uint32_t observedSequence =
        onOffStatusSequence.load(std::memory_order_acquire);

    for (int attempt = 1; attempt <= 2 && rs != CODE_OK; ++attempt)
    {
      LOGI("SetOnOffLight attempt %d/2: addr=0x%04X value=%u", attempt,
           devAddr, onoff);
      if (SendMessage(APP_REQ, reinterpret_cast<uint8_t *>(&onoff_message),
                      sizeof(onoff_message), 0, nullptr, nullptr, 0) != CODE_OK)
        continue;

      for (int waitedMs = 0; waitedMs < 1000; waitedMs += 10)
      {
        SLEEP_MS(10);
        const uint32_t currentSequence =
            onOffStatusSequence.load(std::memory_order_acquire);
        if (currentSequence == observedSequence)
          continue;
        observedSequence = currentSequence;
        if (lastOnOffStatusAddr.load(std::memory_order_relaxed) == devAddr &&
            lastOnOffStatusValue.load(std::memory_order_relaxed) == onoff)
        {
          LOGI("SetOnOffLight verified node status: addr=0x%04X value=%u",
               devAddr, onoff);
          rs = CODE_OK;
          break;
        }
      }
    }
  }
  else
  {
    onoff_message.opcode = G_ONOFF_SET_NOACK;
    LOGI("SetOnOffLight sending NOACK command: devAddr=0x%04X onoff=%d",
         devAddr, onoff);
    rs = SendMessage(APP_REQ, reinterpret_cast<uint8_t *>(&onoff_message),
                     sizeof(onoff_message), 0, nullptr, nullptr, 0);
  }

  isCongested = wasCongested;
  if (rs == CODE_OK)
  {
    return CODE_OK;
  }
  LOGW("SetOnOff err");
  return CODE_ERROR;
}

int BleProtocol::ControlCurtain(uint16_t devAddr, uint8_t openClose,
                                uint8_t position)
{
  if (openClose > 2 || position > 100)
    return CODE_ERROR;

  typedef struct __attribute__((packed))
  {
    uint8_t reserved[6];
    uint16_t devAddr;
    uint16_t opcode;
    uint8_t openClose;
    uint8_t reservedValue;
    uint8_t position;
  } curtain_message_t;

  curtain_message_t message = {};
  message.devAddr = bswap_16(devAddr);
  message.opcode = G_ONOFF_SET;
  message.openClose = openClose;
  message.position = position;

  LOGI("ControlCurtain addr=0x%04X openClose=%d position=%d", devAddr,
       openClose, position);
  return SendMessage(APP_REQ, reinterpret_cast<uint8_t *>(&message),
                     sizeof(message), 0, nullptr, nullptr, 10);
}

int BleProtocol::ControlLegacyCurtain(uint16_t devAddr, uint8_t openClose)
{
  if (openClose > 2)
    return CODE_ERROR;

  // BLEHGAA0105 follows the hc_lux Generic OnOff frame, without position.
  uint8_t data[] = {
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      static_cast<uint8_t>(devAddr >> 8),
      static_cast<uint8_t>(devAddr),
      0x82,
      0x02,
      openClose,
  };

  LOGI("ControlLegacyCurtain addr=0x%04X openClose=%u", devAddr,
       openClose);
  return SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 10);
}

int BleProtocol::SetCurtainTime(uint16_t devAddr, uint16_t seconds)
{
  if (seconds == 0)
    return CODE_ERROR;

  uint8_t data[] = {
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      static_cast<uint8_t>(devAddr >> 8),
      static_cast<uint8_t>(devAddr),
      0xE0,
      0x11,
      0x02,
      0x00,
      0x00,
      0xE0,
      0x00,
      static_cast<uint8_t>(seconds),
      static_cast<uint8_t>(seconds >> 8),
  };

  LOGI("SetCurtainTime addr=0x%04X seconds=%u", devAddr, seconds);
  return SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 10);
}

int BleProtocol::SendCurtainConfig(uint16_t devAddr, uint8_t command,
                                   uint8_t value)
{
  uint8_t data[] = {
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      static_cast<uint8_t>(devAddr >> 8),
      static_cast<uint8_t>(devAddr),
      0xE0,
      0x11,
      0x02,
      0x00,
      0x00,
      command,
      0x00,
      value,
  };
  uint8_t rspCompare[] = {
      static_cast<uint8_t>(devAddr >> 8),
      static_cast<uint8_t>(devAddr),
      0xE0,
      0x00,
      command,
      0x00,
      value,
  };

  return SendMessage(APP_REQ, data, sizeof(data), HCI_GATEWAY_CMD_SEND_BACK_VC,
                     nullptr, nullptr, 1000, rspCompare, 2,
                     sizeof(rspCompare));
}

int BleProtocol::SetCurtainTravelTime(uint16_t devAddr, uint8_t seconds)
{
  if (seconds == 0)
    return CODE_ERROR;

  LOGI("SetCurtainTravelTime addr=0x%04X seconds=%u", devAddr, seconds);
  return SendCurtainConfig(devAddr, 0x17, seconds);
}

int BleProtocol::SetCurtainType(uint16_t devAddr, uint8_t curtainType)
{
  LOGI("SetCurtainType addr=0x%04X type=%u", devAddr, curtainType);
  return SendCurtainConfig(devAddr, 0xE0, curtainType);
}

int BleProtocol::SetCurtainDirection(uint16_t devAddr, uint8_t direction)
{
  if (direction > 1)
    return CODE_ERROR;

  LOGI("SetCurtainDirection addr=0x%04X direction=%u", devAddr, direction);
  return SendCurtainConfig(devAddr, 0xE2, direction);
}

int BleProtocol::GetOnOffLight(uint16_t devAddr)
{
  LOGD("Polling OnOff Get addr: 0x%04X", devAddr);
  uint8_t dataRsp[100];
  int lenRsp;
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint16_t opcode;
  } onoff_message_t;
  onoff_message_t onoff_message;
  memset(&onoff_message, 0x00, sizeof(onoff_message_t));

  // Byte 8-9: Target Address (Sử dụng cấu trúc ble_message_header_t)
  onoff_message.ble_message_header.devAddr = bswap_16(devAddr); // Big Endian

  // Byte 10-11 Opcode: GENERIC_ONOFF_GET = 0x8201 (Cần gửi 82 01)
  onoff_message.opcode = bswap_16(G_ONOFF_GET);

  // Filter cho phản hồi Generic OnOff Status (82 04). Địa chỉ có thể bị
  // đảo endian tùy frame nên chỉ match opcode tại payload offset 4.
  uint8_t onOffHeader[] = {0x82, 0x04};

  int rs = SendMessage(APP_REQ, (uint8_t *)&onoff_message,
                       sizeof(onoff_message_t), HCI_GATEWAY_RSP_OP_CODE,
                       dataRsp, &lenRsp, 800, onOffHeader, 4,
                       sizeof(onOffHeader));
  if (rs == CODE_OK)
  {
    return CODE_OK;
  }
  return CODE_ERROR;
}

int BleProtocol::SetDimmingLight(uint16_t devAddr, uint16_t dim,
                                 uint16_t transition, bool ack)
{
  LOGD("Dimming addr: 0x%04X value %d", devAddr, dim);
  uint8_t dataRsp[100];
  int lenRsp;
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint16_t opcode;
    uint16_t dim;
    uint8_t offset;
    uint16_t transition;
  } dim_message_t;
  dim_message_t dim_message;
  memset(&dim_message, 0x00, sizeof(dim_message));
  if (ack)
  {
    // Status frame (0x81): <dev LE><gw LE> 82 4E 82 4C 01 <dim LE> ...
    uint8_t dimmingHeader[] = {(uint8_t)(devAddr & 0xFF),
                               (uint8_t)((devAddr >> 8) & 0xFF),
                               1,
                               0,
                               0x82,
                               0x4E};

    dim_message.ble_message_header.devAddr = devAddr;
    dim_message.opcode = LIGHTNESS_SET;
    dim_message.dim = dim;
    dim_message.offset = 0;
    dim_message.transition = transition;
    int rs = SendMessage(APP_REQ, (uint8_t *)&dim_message,
                         sizeof(dim_message_t), HCI_GATEWAY_RSP_UNICAST_DIR,
                         dataRsp, &lenRsp, 1000, dimmingHeader, 0,
                         sizeof(dimmingHeader));
    if (rs == CODE_OK)
    {
      if (lenRsp >= 11 && (dataRsp[9] | (dataRsp[10] << 8)) == dim)
      {
        return CODE_OK;
      }
      LOGW("dim resp state not match with input control");
    }
  }
  else
  {
    dim_message.ble_message_header.devAddr = (uint16_t)((devAddr >> 8) | (devAddr << 8)); // Big Endian
    dim_message.opcode = LIGHTNESS_SET_NOACK;
    dim_message.dim = dim;
    dim_message.offset = 0;
    dim_message.transition = transition;
    int rs = SendMessage(APP_REQ, (uint8_t *)&dim_message,
                         sizeof(dim_message_t), 0, dataRsp, &lenRsp, 1000);
    if (rs == CODE_OK)
    {
      return CODE_OK;
    }
  }
  LOGW("Dimming err");
  return CODE_ERROR;
}

int BleProtocol::SetCctLight(uint16_t devAddr, uint16_t cct, uint16_t transition, bool ack)
{
  LOGD("Set Cct addr: 0x%04X value %d", devAddr, cct);
	uint8_t dataRsp[100];
	int lenRsp;
	typedef struct __attribute__((packed))
	{
		ble_message_header_t ble_message_header;
		uint16_t opcode;
		uint16_t cct;
		uint8_t offset[3];
		uint16_t transition;
	} cct_message_t;
	cct_message_t cct_message;
	memset(&cct_message, 0x00, sizeof(cct_message));
	if (ack)
	{
		uint8_t cctHeader[] = {(uint8_t)(devAddr & 0xFF), (uint8_t)((devAddr >> 8) & 0xFF), 1, 00, 0x82, 0x66};
		cct_message.ble_message_header.devAddr = devAddr;
		cct_message.opcode = LIGHT_CTL_TEMP_SET;
		cct_message.cct = cct;
		for (int count = 0; count < 3; count++)
		{
			cct_message.offset[count] = 0;
		}
		cct_message.transition = transition;
		int rs = SendMessage(APP_REQ, (uint8_t *)&cct_message, sizeof(cct_message_t), HCI_GATEWAY_RSP_UNICAST_DIR, dataRsp, &lenRsp, 1000, cctHeader, 0, 6);
		if (rs == CODE_OK)
		{
			// Status frame: <dev LE><gw LE> 82 66 82 64 01 FF FF <cct LE>
			if (lenRsp >= 13 && cct == (dataRsp[11] | (dataRsp[12] << 8)))
			{
				return CODE_OK;
			}
			LOGW("cct resp state not match with input control");
		}
	}
	else if (ack == false)
	{
		cct_message.ble_message_header.devAddr = devAddr;
		cct_message.opcode = LIGHT_CTL_TEMP_SET_NOACK;
		cct_message.cct = cct;
		for (int count = 0; count < 3; count++)
		{
			cct_message.offset[count] = 0;
		}
		cct_message.transition = transition;
		int rs = SendMessage(APP_REQ, (uint8_t *)&cct_message, sizeof(cct_message_t), 0, dataRsp, &lenRsp, 1000);
		if (rs == CODE_OK)
		{
			return CODE_OK;
		}
	}
	LOGW("Cct err");
	return CODE_ERROR;
}

int BleProtocol::SetHSLLight(uint16_t devAddr, uint16_t H, uint16_t S,
                             uint16_t L, uint16_t transition, bool ack)
{
  LOGD("HSL addr: 0x%04X value HSL: %d-%d-%d", devAddr, H, S, L);
  uint8_t dataRsp[100];
  int lenRsp;
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint16_t opcode;
    uint16_t l;
    uint16_t h;
    uint16_t s;
    uint8_t offset;
    uint16_t transition;
  } hsl_message_t;
  hsl_message_t hsl_message = {0};
  memset(&hsl_message, 0x00, sizeof(hsl_message));
  if (ack)
  {
    uint8_t hslHeader[] = {(uint8_t)(devAddr & 0xFF),
                           (uint8_t)((devAddr >> 8) & 0xFF),
                           1,
                           0,
                           0x82,
                           0x78};
    hsl_message.ble_message_header.devAddr = (uint16_t)((devAddr >> 8) | (devAddr << 8)); // Big Endian
    hsl_message.opcode = LIGHT_HSL_SET;
    hsl_message.l = L;
    hsl_message.h = H;
    hsl_message.s = S;
    hsl_message.offset = 0;
    hsl_message.transition = transition;
    int rs = SendMessage(APP_REQ, (uint8_t *)&hsl_message,
                         sizeof(hsl_message_t), HCI_GATEWAY_RSP_UNICAST_DIR,
                         dataRsp, &lenRsp, 1000, hslHeader, 0, 6);
    if (rs == CODE_OK)
    {
      // Status frame: <dev LE><gw LE> 82 78 82 76 <l LE><h LE><s LE>
      if (lenRsp >= 14)
      {
        const uint16_t rspL = dataRsp[8] | (dataRsp[9] << 8);
        const uint16_t rspH = dataRsp[10] | (dataRsp[11] << 8);
        const uint16_t rspS = dataRsp[12] | (dataRsp[13] << 8);
        if (rspH == H && rspL == L && rspS == S)
        {
          return CODE_OK;
        }
      }
      LOGW("hsl resp state not match with input control");
    }
  }
  else
  {
    hsl_message.ble_message_header.devAddr = (uint16_t)((devAddr >> 8) | (devAddr << 8)); // Big Endian
    hsl_message.opcode = LIGHT_HSL_SET_NOACK;
    hsl_message.l = L;
    hsl_message.h = H;
    hsl_message.s = S;
    hsl_message.offset = 0;
    hsl_message.transition = transition;
    int rs = SendMessage(APP_REQ, (uint8_t *)&hsl_message,
                         sizeof(hsl_message_t), 0, dataRsp, &lenRsp, 1000);
    if (rs == CODE_OK)
    {
      return CODE_OK;
    }
  }
  LOGW("Set hsl err");
  return CODE_ERROR;
}

int BleProtocol::SetCctDimLight(uint16_t devAddr, uint16_t cct, uint16_t dim,
                                uint16_t transition, bool ack)
{
  LOGD("Set Dim cct addr: 0x%04X", devAddr);
  uint8_t dataRsp[100];
  int lenRsp;
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint16_t opcode;
    uint16_t dim;
    uint16_t cct;
    uint8_t offset;
    uint16_t transition;
  } dimcct_message_t;
  dimcct_message_t dimcct_message = {0};
  memset(&dimcct_message, 0x00, sizeof(dimcct_message));
  if (ack)
  {
    uint8_t dimcctHeader[] = {(uint8_t)((devAddr >> 8) & 0xFF),
                              (uint8_t)(devAddr & 0xFF),
                              0x82,
                              0x5E};
    dimcct_message.ble_message_header.devAddr = (uint16_t)((devAddr >> 8) | (devAddr << 8)); // Big Endian
    dimcct_message.opcode = LIGHT_CTL_SET;
    dimcct_message.dim = dim;
    dimcct_message.cct = cct;
    dimcct_message.offset = 0;
    dimcct_message.transition = transition;
    int rs = SendMessage(APP_REQ, (uint8_t *)&dimcct_message,
                         sizeof(dimcct_message_t), HCI_GATEWAY_RSP_OP_CODE,
                         dataRsp, &lenRsp, 1000, dimcctHeader, 2,
                         sizeof(dimcctHeader));
    if (rs == CODE_OK)
    {
      for (int i = 0; i + 5 < lenRsp; i++)
      {
        if (dataRsp[i] == 0x82 && dataRsp[i + 1] == 0x5E)
        {
          uint16_t rspDim = dataRsp[i + 2] | (dataRsp[i + 3] << 8);
          uint16_t rspCct = dataRsp[i + 4] | (dataRsp[i + 5] << 8);
          if (rspDim == dim && rspCct == cct)
            return CODE_OK;
        }
      }
      LOGW("dim cct resp state not match with input control");
    }
  }
  else
  {
    dimcct_message.ble_message_header.devAddr = (uint16_t)((devAddr >> 8) | (devAddr << 8)); // Big Endian
    dimcct_message.opcode = LIGHT_CTL_SET_NOACK;
    dimcct_message.dim = dim;
    dimcct_message.cct = cct;
    dimcct_message.offset = 0;
    dimcct_message.transition = transition;
    int rs = SendMessage(APP_REQ, (uint8_t *)&dimcct_message,
                         sizeof(dimcct_message_t), 0, dataRsp, &lenRsp, 1000);
    if (rs == CODE_OK)
    {
      return CODE_OK;
    }
  }
  LOGW("Set dim cct err");
  return CODE_ERROR;
}

int BleProtocol::SetGroupDimmingLight(uint16_t groupAddr, uint16_t dim)
{
  uint8_t data[] = {
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      static_cast<uint8_t>(groupAddr >> 8),
      static_cast<uint8_t>(groupAddr),
      0x82,
      0x4C,
      static_cast<uint8_t>(dim),
      static_cast<uint8_t>(dim >> 8),
  };
  LOGI("Set group dim addr=0x%04X dim=%u", groupAddr, dim);
  return SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 1000);
}

int BleProtocol::SetGroupCctLight(uint16_t groupAddr, uint16_t cct)
{
  uint8_t data[] = {
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      static_cast<uint8_t>(groupAddr >> 8),
      static_cast<uint8_t>(groupAddr),
      0x82,
      0x64,
      static_cast<uint8_t>(cct),
      static_cast<uint8_t>(cct >> 8),
  };
  LOGI("Set group cct addr=0x%04X cct=%u", groupAddr, cct);
  return SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 1000);
}

int BleProtocol::SetGroupCctDimLight(uint16_t groupAddr, uint16_t dim,
                                     uint16_t cct)
{
  uint8_t data[] = {
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      static_cast<uint8_t>(groupAddr >> 8),
      static_cast<uint8_t>(groupAddr),
      0x82,
      0x5E,
      static_cast<uint8_t>(dim),
      static_cast<uint8_t>(dim >> 8),
      static_cast<uint8_t>(cct),
      static_cast<uint8_t>(cct >> 8),
  };
  LOGI("Set group dim/cct addr=0x%04X dim=%u cct=%u", groupAddr, dim, cct);
  return SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 1000);
}

int BleProtocol::SetGroupHslLight(uint16_t groupAddr, const std::string &hsl)
{
  if (hsl.size() < 12)
    return CODE_ERROR;

  char *end = nullptr;
  uint16_t first = static_cast<uint16_t>(strtoul(hsl.substr(0, 4).c_str(), &end, 16));
  if (!end || *end != '\0')
    return CODE_ERROR;
  uint16_t second = static_cast<uint16_t>(strtoul(hsl.substr(4, 4).c_str(), &end, 16));
  if (!end || *end != '\0')
    return CODE_ERROR;
  uint16_t third = static_cast<uint16_t>(strtoul(hsl.substr(8, 4).c_str(), &end, 16));
  if (!end || *end != '\0')
    return CODE_ERROR;

  uint8_t data[] = {
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      static_cast<uint8_t>(groupAddr >> 8),
      static_cast<uint8_t>(groupAddr),
      0x82,
      0x76,
      static_cast<uint8_t>(first),
      static_cast<uint8_t>(first >> 8),
      static_cast<uint8_t>(second),
      static_cast<uint8_t>(second >> 8),
      static_cast<uint8_t>(third),
      static_cast<uint8_t>(third >> 8),
  };
  LOGI("Set group hsl addr=0x%04X hsl=%s", groupAddr, hsl.c_str());
  return SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 1000);
}

int BleProtocol::SetGroupRgbMode(uint16_t groupAddr, uint8_t modeRgb)
{
  uint8_t data[] = {
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      static_cast<uint8_t>(groupAddr >> 8),
      static_cast<uint8_t>(groupAddr),
      0x82,
      0x50,
      0x19,
      0x09,
      modeRgb,
  };
  LOGI("Set group rgb mode addr=0x%04X mode=%u", groupAddr, modeRgb);
  return SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 1000);
}

int BleProtocol::SetLevelDim(uint16_t devAddr, uint8_t dimMax, uint8_t dimMin)
{
  LOGD("Set Level Dim addr: 0x%04X, dimMax: %d, dimMin: %d", devAddr, dimMax,
       dimMin);
  uint8_t dataRsp[100];
  int lenRsp;
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint16_t opcode;
    uint8_t header;
    uint16_t magic;
    uint8_t dimMax;
    uint8_t dimMin;
  } dim_message_t;
  dim_message_t dim_message = {0};
  memset(&dim_message, 0x00, sizeof(dim_message));

  uint8_t dimHeader[] = {(uint8_t)(devAddr & 0xFF),
                         (uint8_t)((devAddr >> 8) & 0xFF),
                         1,
                         0,
                         0x82,
                         0x52};
  dim_message.ble_message_header.devAddr = (uint16_t)((devAddr >> 8) | (devAddr << 8)); // Big Endian
  dim_message.opcode = LIGHTNESS_LINEAR_SET;
  dim_message.header = 8;
  dim_message.dimMax = dimMax;
  dim_message.dimMin = dimMin;
  int rs = SendMessage(APP_REQ, (uint8_t *)&dim_message, sizeof(dim_message_t),
                       HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 1000,
                       dimHeader, 0, 6);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint16_t opcode;
      uint8_t header;
      uint16_t magic;
      uint8_t dimMax;
      uint8_t dimMin;
    } dim_rsp_message_t;
    dim_rsp_message_t *dim_rsp_message = (dim_rsp_message_t *)dataRsp;
    if (dim_rsp_message->opcode == LIGHTNESS_LINEAR_STATUS &&
        dim_rsp_message->dimMax == dimMax &&
        dim_rsp_message->dimMin == dimMin)
    {
      return CODE_OK;
    }
    LOGW("dim level resp state not match with input control");
  }
  LOGW("Set dim level err");
  return CODE_ERROR;
}

int BleProtocol::SetupDimLevels(uint16_t devAddr, const uint8_t levels[5])
{
  uint8_t data[] = {
      0xE8,
      0xFF,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      static_cast<uint8_t>((devAddr >> 8) & 0xFF),
      static_cast<uint8_t>(devAddr & 0xFF),
      0xE0,
      0x11,
      0x02,
      0x00,
      0x00,
      0x14,
      0x00,
      levels[0],
      levels[1],
      levels[2],
      levels[3],
      levels[4],
  };

  LOGI("SetupDimLevels 0x%04X: %u, %u, %u, %u, %u", devAddr, levels[0],
       levels[1], levels[2], levels[3], levels[4]);
  return SendRawData(data, sizeof(data));
}

int BleProtocol::AddDev2Group(uint16_t devAddr, uint16_t element,
                              uint16_t group)
{
  LOGD("Add dev addr: 0x%04X  with element: 0x%04x to group: 0x%04X", devAddr,
       element, group);
  uint8_t dataRsp[100] = {0};
  int lenRsp = 0;
  uint16_t gwAddr = Gateway::getInstance()->GetBleAddr();
  uint8_t rspHeader[] = {(uint8_t)((devAddr >> 8) & 0xFF),
                         (uint8_t)(devAddr & 0xFF),
                         (uint8_t)((gwAddr >> 8) & 0xFF),
                         (uint8_t)(gwAddr & 0xFF),
                         0x80,
                         0x1F};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint16_t opcode;
    uint16_t element;
    uint16_t group;
    uint16_t offset;
  } addgroup_message_t;
  addgroup_message_t addgroup_message = {0};
  memset(&addgroup_message, 0x00, sizeof(addgroup_message));
  addgroup_message.ble_message_header.devAddr =
      (uint16_t)((devAddr >> 8) | (devAddr << 8)); // Big Endian
  addgroup_message.opcode = CFG_MODEL_SUB_ADD;
  addgroup_message.element = element;
  addgroup_message.group = group;
  addgroup_message.offset = 0x1000;

  typedef struct __attribute__((packed))
  {
    uint16_t devAddr;
    uint16_t gwAddr;
    uint16_t opcode;
    uint8_t status;
    uint16_t element;
    uint16_t group;
  } group_rsp_message_t;

  for (int attempt = 1; attempt <= 2; attempt++)
  {
    lenRsp = 0;
    int rs = SendMessage(APP_REQ, (uint8_t *)&addgroup_message,
                         sizeof(addgroup_message_t),
                         HCI_GATEWAY_RSP_UNICAST_DIR,
                         dataRsp, &lenRsp, 2000, rspHeader, 0,
                         sizeof(rspHeader));
    if (rs == CODE_OK && lenRsp >= (int)sizeof(group_rsp_message_t))
    {
      group_rsp_message_t *rsp = (group_rsp_message_t *)dataRsp;
      if (rsp->opcode == CFG_MODEL_SUB_STATUS && rsp->status == 0 &&
          rsp->element == element && rsp->group == group)
      {
        LOGI("Add group confirmed: dev=0x%04X element=0x%04X group=0x%04X",
             devAddr, element, group);
        return CODE_OK;
      }
      LOGW("Add group response mismatch: status=%u element=0x%04X "
           "group=0x%04X",
           rsp->status, rsp->element, rsp->group);
    }
    else
    {
      LOGW("Add group attempt %d/2 timed out: dev=0x%04X element=0x%04X "
           "group=0x%04X",
           attempt, devAddr, element, group);
    }
  }
  return CODE_ERROR;
}

int BleProtocol::DelDev2Group(uint16_t devAddr, uint16_t element,
                              uint16_t group)
{
  LOGD("Del dev addr: 0x%04X  with element: 0x%04x to group: 0x%04X", devAddr,
       element, group);
  uint8_t dataRsp[100] = {0};
  int lenRsp = 0;
  uint16_t gwAddr = Gateway::getInstance()->GetBleAddr();
  uint8_t rspHeader[] = {(uint8_t)((devAddr >> 8) & 0xFF),
                         (uint8_t)(devAddr & 0xFF),
                         (uint8_t)((gwAddr >> 8) & 0xFF),
                         (uint8_t)(gwAddr & 0xFF),
                         0x80,
                         0x1F};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint16_t opcode;
    uint16_t element;
    uint16_t group;
    uint16_t offset;
  } delgroup_message_t;
  delgroup_message_t delgroup_message = {0};
  memset(&delgroup_message, 0x00, sizeof(delgroup_message));
  delgroup_message.ble_message_header.devAddr =
      (uint16_t)((devAddr >> 8) | (devAddr << 8)); // Big Endian
  delgroup_message.opcode = CFG_MODEL_SUB_DEL;
  delgroup_message.element = element;
  delgroup_message.group = group;
  delgroup_message.offset = 0x1000;

  typedef struct __attribute__((packed))
  {
    uint16_t devAddr;
    uint16_t gwAddr;
    uint16_t opcode;
    uint8_t status;
    uint16_t element;
    uint16_t group;
  } group_rsp_message_t;

  for (int attempt = 1; attempt <= 2; attempt++)
  {
    lenRsp = 0;
    int rs = SendMessage(APP_REQ, (uint8_t *)&delgroup_message,
                         sizeof(delgroup_message_t),
                         HCI_GATEWAY_RSP_UNICAST_DIR,
                         dataRsp, &lenRsp, 2000, rspHeader, 0,
                         sizeof(rspHeader));
    if (rs == CODE_OK && lenRsp >= (int)sizeof(group_rsp_message_t))
    {
      group_rsp_message_t *rsp = (group_rsp_message_t *)dataRsp;
      if (rsp->opcode == CFG_MODEL_SUB_STATUS && rsp->status == 0 &&
          rsp->element == element && rsp->group == group)
      {
        LOGI("Remove group confirmed: dev=0x%04X element=0x%04X "
             "group=0x%04X",
             devAddr, element, group);
        return CODE_OK;
      }
      LOGW("Remove group response mismatch: status=%u element=0x%04X "
           "group=0x%04X",
           rsp->status, rsp->element, rsp->group);
    }
    else
    {
      LOGW("Remove group attempt %d/2 timed out: dev=0x%04X element=0x%04X "
           "group=0x%04X",
           attempt, devAddr, element, group);
    }
  }
  return CODE_ERROR;
}

int BleProtocol::SetScene(uint16_t devAddr, uint16_t scene, uint8_t modeRgb)
{
  LOGD("Set scene addr: 0x%04X to scene: 0x%04X, modergb: %d", devAddr, scene,
       modeRgb);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t setSceneHeader[] = {(uint8_t)(devAddr & 0xFF),
                              (uint8_t)((devAddr >> 8) & 0xFF),
                              1,
                              0,
                              0x82,
                              0x45};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint16_t opcode;
    uint16_t scene;
    uint8_t modeRgb;
    uint16_t offset;
  } setscene_message_t;
  setscene_message_t setscene_message = {0};
  memset(&setscene_message, 0x00, sizeof(setscene_message));
  setscene_message.ble_message_header.devAddr = devAddr;
  setscene_message.opcode = SCENE_STORE;
  setscene_message.scene = scene;
  setscene_message.modeRgb = modeRgb;
  int rs = SendMessage(APP_REQ, (uint8_t *)&setscene_message,
                       sizeof(setscene_message_t), HCI_GATEWAY_RSP_OP_CODE,
                       dataRsp, &lenRsp, 1000, setSceneHeader, 0, 6);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint16_t opcode;
      uint8_t offset;
      uint16_t scene;
    } setscene_rsp_message_t;
    setscene_rsp_message_t *setscene_rsp_message =
        (setscene_rsp_message_t *)dataRsp;
    if (setscene_rsp_message->scene == scene)
    {
      return CODE_OK;
    }
    else
    {
      LOGW("set scene resp state not match with input control");
    }
  }
  LOGW("Set scene err");
  return CODE_ERROR;
}

// TODO: BelProtocol DelScene
int BleProtocol::DelScene(uint16_t devAddr, uint16_t scene)
{
  LOGD("Del scene addr: 0x%04X to scene: 0x%04X", devAddr, scene);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t delSceneHeader[] = {(uint8_t)(devAddr & 0xFF),
                              (uint8_t)((devAddr >> 8) & 0xFF),
                              1,
                              0,
                              0x82,
                              0x45};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint16_t opcode;
    uint16_t scene;
  } delscene_message_t;
  delscene_message_t delscene_message = {0};
  memset(&delscene_message, 0x00, sizeof(delscene_message));
  delscene_message.ble_message_header.devAddr = devAddr;
  delscene_message.opcode = SCENE_DEL;
  delscene_message.scene = scene;
  int rs = SendMessage(APP_REQ, (uint8_t *)&delscene_message,
                       sizeof(delscene_message_t), HCI_GATEWAY_RSP_OP_CODE,
                       dataRsp, &lenRsp, 1000, delSceneHeader, 0, 6);
  if (rs == CODE_OK)
  {
    return CODE_OK;
  }
  LOGW("Del scene err");
  return CODE_ERROR;
}

// TODO: BelProtocol ActiveScene
// add delay time
int BleProtocol::CallScene(uint16_t devAddr, uint16_t scene,
                           uint16_t transition, bool ack)
{
  LOGD("Call scene: 0x%04X", scene);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t callSceneHeader[] = {(uint8_t)(devAddr & 0xFF),
                               (uint8_t)((devAddr >> 8) & 0xFF),
                               1,
                               0,
                               0x5e,
                               0x00};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint16_t opcode;
    uint16_t scene;
    uint8_t offset;
    uint16_t transition;
  } callscene_message_t;
  callscene_message_t callscene_message = {0};
  memset(&callscene_message, 0x00, sizeof(callscene_message));
  if (ack)
  {
    callscene_message.ble_message_header.devAddr = devAddr;
    callscene_message.opcode = SCENE_RECALL;
    callscene_message.scene = bswap_16(scene);
    callscene_message.offset = 0;
    callscene_message.transition = 0;
    int rs = SendMessage(APP_REQ, (uint8_t *)&callscene_message,
                         sizeof(callscene_message_t), HCI_GATEWAY_RSP_OP_CODE,
                         dataRsp, &lenRsp, 1000, callSceneHeader, 0, 6);
    if (rs == CODE_OK)
    {
      typedef struct __attribute__((packed))
      {
        uint16_t devAddr;
        uint16_t gwAddr;
        uint16_t opcode;
        uint8_t data[8];
      } callscene_rsp_message_t;
      callscene_rsp_message_t *callscene_rsp_message =
          (callscene_rsp_message_t *)dataRsp;
      if (lenRsp == 11 || lenRsp == 13)
      {
        if (scene == (callscene_rsp_message->data[2] |
                      (callscene_rsp_message->data[3] << 8)))
        {
          return CODE_OK;
        }
        else
        {
          LOGW("call scene resp state not match with input control");
        }
      }
      else
      {
        if (scene == (callscene_rsp_message->data[0] |
                      (callscene_rsp_message->data[1] << 8)))
        {
          return CODE_OK;
        }
        else
        {
          LOGW("call scene resp state not match with input control");
        }
      }
    }
  }
  else
  {
    callscene_message.ble_message_header.devAddr = devAddr;
    callscene_message.opcode = SCENE_RECALL;
    callscene_message.scene = bswap_16(scene);
    callscene_message.offset = 0;
    callscene_message.transition = 0;
    int rs = SendMessage(APP_REQ, (uint8_t *)&callscene_message,
                         sizeof(callscene_message_t), 0, dataRsp, &lenRsp, 400);
    if (rs == CODE_OK)
    {
      return CODE_OK;
    }
  }

  LOGW("Call scene err");
  return CODE_ERROR;
}

int BleProtocol::CallModeRgb(uint16_t devAddr, uint8_t modeRgb)
{
  LOGD("Call modeRgb: %d, addr: 0x%04X ", modeRgb, devAddr);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t modeRgbHeader[] = {(uint8_t)(devAddr & 0xFF),
                             (uint8_t)((devAddr >> 8) & 0xFF),
                             1,
                             0,
                             0x82,
                             0x52};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint16_t opcode;
    uint16_t header;
    uint8_t mode;
  } modergb_message_t;
  modergb_message_t modergb_message = {0};
  memset(&modergb_message, 0x00, sizeof(modergb_message));
  modergb_message.ble_message_header.devAddr = devAddr;
  modergb_message.opcode = LIGHTNESS_LINEAR_SET;
  modergb_message.header = HEADER_CALLMODE_RGB;
  modergb_message.mode = modeRgb;
  int rs = SendMessage(APP_REQ, (uint8_t *)&modergb_message,
                       sizeof(modergb_message_t), HCI_GATEWAY_RSP_OP_CODE,
                       dataRsp, &lenRsp, 1000, modeRgbHeader, 0, 6);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint16_t opcode;
      uint16_t header;
      uint8_t mode;
    } modergb_rsp_message_t;
    modergb_rsp_message_t *modergb_rsp_message =
        (modergb_rsp_message_t *)dataRsp;
    if (modergb_rsp_message->header == HEADER_CALLMODE_RGB)
    {
      if (modergb_rsp_message->mode == modeRgb)
      {
        return CODE_OK;
      }
      LOGW("call mode rgb resp state not match with input control");
    }
  }
  LOGW("call mode rgb err");
  return CODE_ERROR;
}

int BleProtocol::UpdateLights(uint16_t devAddr)
{
  // LOGI("Update lights addr: 0x%04X ", devAddr);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t updateHeader[] = {(uint8_t)(devAddr & 0xFF),
                            (uint8_t)((devAddr >> 8) & 0xFF),
                            1,
                            0,
                            0x82,
                            0x52};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint16_t opcode;
    uint8_t header;
    uint8_t data[7];
  } update_message_t;
  update_message_t update_message = {0};
  memset(&update_message, 0x00, sizeof(update_message));
  update_message.ble_message_header.devAddr = devAddr;
  update_message.opcode = LIGHTNESS_LINEAR_SET;
  update_message.header = 0x02;
  int rs = SendMessage(APP_REQ, (uint8_t *)&update_message,
                       sizeof(update_message_t), HCI_GATEWAY_RSP_OP_CODE,
                       dataRsp, &lenRsp, 800, updateHeader, 0, 6);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint16_t opcode;
      uint8_t header;
    } update_rsp_message_t;
    update_rsp_message_t *update_rsp_message = (update_rsp_message_t *)dataRsp;
    if (update_rsp_message->header == 0x02)
    {
      return CODE_OK;
    }
    LOGW("update lights resp state not match with input control");
  }
  LOGW("update lights 0x%04X err", devAddr);
  return CODE_ERROR;
}

int BleProtocol::UpdateStatusSensorsPm(uint16_t devAddr)
{
  LOGD("Update status sensor addr: 0x%04X ", devAddr);
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint8_t data[6];
  } update_message_t;
  update_message_t update_message = {0};
  memset(&update_message, 0x00, sizeof(update_message));
  update_message.ble_message_header.devAddr = devAddr;
  update_message.opcodeVendor = RD_OPCODE_CONFIG;
  update_message.vendorId = RD_VENDOR_ID;
  update_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  update_message.header = GET_STATUS_SENSOR_PM;
  int rs =
      SendMessage(APP_REQ, (uint8_t *)&update_message, sizeof(update_message_t),
                  HCI_GATEWAY_RSP_OP_CODE, 0, 0, 1000);
  if (rs == CODE_OK)
  {
    return CODE_OK;
  }
  LOGW("update status sensor err");
  return CODE_ERROR;
}

int BleProtocol::SetScenePirLightSensor(uint16_t devAddr, uint8_t condition,
                                        uint8_t pir, uint16_t lowLux,
                                        uint16_t highLux, uint16_t scene,
                                        uint8_t type)
{
  LOGD("SetScenePirLightSensor condition: %d, pir: %d, lowLux: %d, highLux: %d",
       condition, pir, lowLux, highLux);
  typedef struct __attribute__((packed))
  {
    union
    {
      uint32_t data;
      struct
      {
        uint32_t store : 8;          // 8 bit not use
        uint32_t Lux_hi : 10;        // 10 bit lux hi
        uint32_t Lux_low : 10;       // 10 bit lux low
        uint32_t Light_Conditon : 3; // 7 bit low
        uint32_t Pir_Conditon : 1;   // 1 bit hight
      };
    };
  } RD_Sensor_data_tdef;

  RD_Sensor_data_tdef data_scene_pir_light;
  data_scene_pir_light.Pir_Conditon = (uint32_t)pir;
  data_scene_pir_light.Light_Conditon = (uint32_t)(condition & 0x0000007);
  data_scene_pir_light.Lux_low = (uint32_t)(lowLux);
  data_scene_pir_light.Lux_hi = (uint32_t)(highLux);

  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t sceneLightPirHeader[] = {(uint8_t)(devAddr & 0xFF),
                                   (uint8_t)((devAddr >> 8) & 0xFF),
                                   1,
                                   0,
                                   RD_OPCODE_CONFIG_RSP,
                                   0x11,
                                   0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint16_t sceneId;
    uint8_t infoScene[3];
    uint8_t type;
  } scene_light_pir_message_t;
  scene_light_pir_message_t scene_light_pir_message = {0};
  memset(&scene_light_pir_message, 0x00, sizeof(scene_light_pir_message));
  scene_light_pir_message.ble_message_header.devAddr = devAddr;
  scene_light_pir_message.opcodeVendor = RD_OPCODE_CONFIG;
  scene_light_pir_message.vendorId = RD_VENDOR_ID;
  scene_light_pir_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  scene_light_pir_message.header = RD_HEADER_CONFIG_SET_SCENE_PIR_LIGHT_SENSOR;
  scene_light_pir_message.sceneId = scene;
  scene_light_pir_message.infoScene[0] =
      (data_scene_pir_light.data >> 24) & 0xFF;
  scene_light_pir_message.infoScene[1] =
      (data_scene_pir_light.data >> 16) & 0xFF;
  scene_light_pir_message.infoScene[2] =
      (data_scene_pir_light.data >> 8) & 0xFF;
  int rs =
      SendMessage(APP_REQ, (uint8_t *)&scene_light_pir_message,
                  sizeof(scene_light_pir_message_t), HCI_GATEWAY_RSP_OP_CODE,
                  dataRsp, &lenRsp, 2000, sceneLightPirHeader, 0, 7);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint8_t opcodeRsp;
      uint16_t vendorId;
      uint16_t header;
      uint16_t scene;
    } scene_light_pir_rsp_message_t;
    scene_light_pir_rsp_message_t *scene_light_pir_rsp_message =
        (scene_light_pir_rsp_message_t *)&dataRsp;
    if (scene_light_pir_rsp_message->header ==
            RD_HEADER_CONFIG_SET_SCENE_PIR_LIGHT_SENSOR &&
        scene_light_pir_rsp_message->scene == scene)
    {
      return CODE_OK;
    }
    else
    {
      LOGW("Scene light pir rsp not match control");
    }
  }
  else
  {
    LOGW("Scene light pir error");
  }
  return CODE_ERROR;
}

int BleProtocol::DelScenePirLightSensor(uint16_t devAddr, uint16_t scene)
{
  LOGD("Del scene light pir");
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t sceneLightPirHeader[] = {(uint8_t)(devAddr & 0xFF),
                                   (uint8_t)((devAddr >> 8) & 0xFF),
                                   1,
                                   0,
                                   RD_OPCODE_CONFIG_RSP,
                                   0x11,
                                   0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint16_t sceneId;
  } scene_light_pir_message_t;
  scene_light_pir_message_t scene_light_pir_message = {0};
  memset(&scene_light_pir_message, 0x00, sizeof(scene_light_pir_message));
  scene_light_pir_message.ble_message_header.devAddr = devAddr;
  scene_light_pir_message.opcodeVendor = RD_OPCODE_CONFIG;
  scene_light_pir_message.vendorId = RD_VENDOR_ID;
  scene_light_pir_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  scene_light_pir_message.header = RD_HEADER_CONFIG_DEL_SCENE_PIR_LIGHT_SENSOR;
  scene_light_pir_message.sceneId = scene;
  int rs =
      SendMessage(APP_REQ, (uint8_t *)&scene_light_pir_message,
                  sizeof(scene_light_pir_message_t), HCI_GATEWAY_RSP_OP_CODE,
                  dataRsp, &lenRsp, 2000, sceneLightPirHeader, 0, 7);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint8_t opcodeRsp;
      uint16_t vendorId;
      uint16_t header;
      uint16_t scene;
    } scene_light_pir_rsp_message_t;
    scene_light_pir_rsp_message_t *scene_light_pir_rsp_message =
        (scene_light_pir_rsp_message_t *)&dataRsp;
    if (scene_light_pir_rsp_message->header ==
            RD_HEADER_CONFIG_DEL_SCENE_PIR_LIGHT_SENSOR &&
        scene_light_pir_rsp_message->scene == scene)
    {
      return CODE_OK;
    }
    else
    {
      LOGW("Del Scene light pir rsp not match control");
    }
  }
  else
  {
    LOGW("Del Scene light pir error");
  }
  return CODE_ERROR;
}

int BleProtocol::TimeActionPirLightSensor(uint16_t devAddr, uint16_t time)
{
  LOGD("TimeActionPirLightSensor 0x%04X", devAddr);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t timeActionHeader[] = {(uint8_t)(devAddr & 0xFF),
                                (uint8_t)((devAddr >> 8) & 0xFF),
                                1,
                                0,
                                RD_OPCODE_CONFIG_RSP,
                                0x11,
                                0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint16_t time;
  } time_action_message_t;
  time_action_message_t time_action_message = {0};
  memset(&time_action_message, 0x00, sizeof(time_action_message));
  time_action_message.ble_message_header.devAddr = devAddr;
  time_action_message.opcodeVendor = RD_OPCODE_CONFIG;
  time_action_message.vendorId = RD_VENDOR_ID;
  time_action_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  time_action_message.header =
      RD_HEADER_CONFIG_SET_TIME_ACTION_PIR_LIGHT_SENSOR;
  time_action_message.time = time;
  int rs = SendMessage(APP_REQ, (uint8_t *)&time_action_message,
                       sizeof(time_action_message_t), HCI_GATEWAY_RSP_OP_CODE,
                       dataRsp, &lenRsp, 1000, timeActionHeader, 0, 7);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint8_t opcodeRsp;
      uint16_t vendorId;
      uint16_t header;
      uint16_t time;
    } time_action_rsp_message_t;
    time_action_rsp_message_t *time_action_rsp_message =
        (time_action_rsp_message_t *)dataRsp;
    if (time_action_rsp_message->header ==
            RD_HEADER_CONFIG_SET_TIME_ACTION_PIR_LIGHT_SENSOR &&
        time_action_rsp_message->time == time)
    {
      return CODE_OK;
    }
    LOGW("time action pir light resp state not match with input control");
  }
  LOGW("time action pir light err");
  return CODE_ERROR;
}

int BleProtocol::SetModeActionPirLightSensor(uint16_t devAddr, uint8_t mode)
{
  LOGD("ModeActionPirLightSensor 0x%04X", devAddr);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t modeActionHeader[] = {(uint8_t)(devAddr & 0xFF),
                                (uint8_t)((devAddr >> 8) & 0xFF),
                                1,
                                0,
                                RD_OPCODE_CONFIG_RSP,
                                0x11,
                                0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint8_t mode;
    uint8_t future[5];
  } mode_action_message_t;
  mode_action_message_t mode_action_message = {0};
  memset(&mode_action_message, 0x00, sizeof(mode_action_message));
  mode_action_message.ble_message_header.devAddr = devAddr;
  mode_action_message.opcodeVendor = RD_OPCODE_CONFIG;
  mode_action_message.vendorId = RD_VENDOR_ID;
  mode_action_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  mode_action_message.header =
      RD_HEADER_CONFIG_SET_MODE_ACTION_PIR_LIGHT_SENSOR;
  mode_action_message.mode = mode;
  int rs = SendMessage(APP_REQ, (uint8_t *)&mode_action_message,
                       sizeof(mode_action_message_t), HCI_GATEWAY_RSP_OP_CODE,
                       dataRsp, &lenRsp, 1000, modeActionHeader, 0, 7);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint8_t opcodeRsp;
      uint16_t vendorId;
      uint16_t header;
      uint8_t mode;
    } mode_action_rsp_message_t;
    mode_action_rsp_message_t *mode_action_rsp_message =
        (mode_action_rsp_message_t *)dataRsp;
    if (mode_action_rsp_message->header ==
            RD_HEADER_CONFIG_SET_MODE_ACTION_PIR_LIGHT_SENSOR &&
        mode_action_rsp_message->mode == mode)
    {
      return CODE_OK;
    }
    LOGW("mode action pir light resp state not match with input control");
  }
  LOGW("mode action pir light err");
  return CODE_ERROR;
}

int BleProtocol::SetSensiPirLightSensor(uint16_t devAddr, uint8_t sensi)
{
  LOGD("Set sensiPirLightSensor: 0x%04X, sensi: %d", devAddr, sensi);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t sensiHeader[] = {(uint8_t)(devAddr & 0xFF),
                           (uint8_t)((devAddr >> 8) & 0xFF),
                           1,
                           0,
                           RD_OPCODE_CONFIG_RSP,
                           0x11,
                           0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint16_t sensi;
  } sensi_message_t;
  sensi_message_t sensi_message = {0};
  memset(&sensi_message, 0x00, sizeof(sensi_message));
  sensi_message.ble_message_header.devAddr = devAddr;
  sensi_message.opcodeVendor = RD_OPCODE_CONFIG;
  sensi_message.vendorId = RD_VENDOR_ID;
  sensi_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  sensi_message.header = RD_HEADER_CONFIG_SET_SENSI_PIR_LIGHT_SENSOR;
  sensi_message.sensi = sensi;
  int rs = SendMessage(APP_REQ, (uint8_t *)&sensi_message,
                       sizeof(sensi_message_t), HCI_GATEWAY_RSP_OP_CODE,
                       dataRsp, &lenRsp, 1000, sensiHeader, 0, 7);
  if (rs == CODE_OK)
  {
    return CODE_OK;
  }
  else
    LOGW("Set sensi error");
  return CODE_ERROR;
}

int BleProtocol::SetDistanceSensor(uint16_t devAddr, uint8_t distance)
{
  LOGD("Set distance sensor: 0x%04X, distance: %d", devAddr, distance);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t distanceHeader[] = {(uint8_t)(devAddr & 0xFF),
                              (uint8_t)((devAddr >> 8) & 0xFF),
                              1,
                              0,
                              RD_OPCODE_CONFIG_RSP,
                              0x11,
                              0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint16_t distance;
  } distance_message_t;
  distance_message_t distance_message = {0};
  memset(&distance_message, 0x00, sizeof(distance_message));
  distance_message.ble_message_header.devAddr = devAddr;
  distance_message.opcodeVendor = RD_OPCODE_CONFIG;
  distance_message.vendorId = RD_VENDOR_ID;
  distance_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  distance_message.header = RD_HEADER_CONFIG_SET_DISTANCE_RADA_SENSOR;
  distance_message.distance = distance;
  int rs = SendMessage(APP_REQ, (uint8_t *)&distance_message,
                       sizeof(distance_message_t), HCI_GATEWAY_RSP_OP_CODE,
                       dataRsp, &lenRsp, 1000, distanceHeader, 0, 7);
  if (rs == CODE_OK)
  {
    return CODE_OK;
  }
  else
    LOGW("Set distance error");
  return CODE_ERROR;
}

int BleProtocol::SetTimeRspSensor(uint16_t devAddr, uint16_t time)
{
  LOGD("Set time rsp sensor: 0x%04X, time: %d", devAddr, time);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t timeRspHeader[] = {(uint8_t)(devAddr & 0xFF),
                             (uint8_t)((devAddr >> 8) & 0xFF),
                             1,
                             0,
                             RD_OPCODE_CONFIG_RSP,
                             0x11,
                             0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint16_t time;
  } timeRsp_message_t;
  timeRsp_message_t timeRsp_message = {0};
  memset(&timeRsp_message, 0x00, sizeof(timeRsp_message));
  timeRsp_message.ble_message_header.devAddr = devAddr;
  timeRsp_message.opcodeVendor = RD_OPCODE_CONFIG;
  timeRsp_message.vendorId = RD_VENDOR_ID;
  timeRsp_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  timeRsp_message.header = RD_HEADER_CONFIG_TIME_RSP_SENSOR;
  timeRsp_message.time = time;
  int rs = SendMessage(APP_REQ, (uint8_t *)&timeRsp_message,
                       sizeof(timeRsp_message_t), HCI_GATEWAY_RSP_OP_CODE,
                       dataRsp, &lenRsp, 1000, timeRspHeader, 0, 7);
  if (rs == CODE_OK)
  {
    return CODE_OK;
  }
  else
    LOGW("Set time rsp error");
  return CODE_ERROR;
}

int BleProtocol::ControlIR(uint16_t devAddr, int commandType, int brandId,
                           int remoteId, int temp, int mode, int fan,
                           int swing)
{
  uint8_t data[] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xFF, 0xFF, 0xE4, 0x11, 0x02, 0x00, 0x00,
      0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  data[6] = static_cast<uint8_t>(devAddr >> 8);
  data[7] = static_cast<uint8_t>(devAddr & 0xFF);
  data[13] = static_cast<uint8_t>(commandType);
  data[14] = static_cast<uint8_t>(brandId);
  data[15] = static_cast<uint8_t>(brandId >> 8);
  data[16] = static_cast<uint8_t>(remoteId);
  data[17] = static_cast<uint8_t>(temp);
  data[18] = static_cast<uint8_t>(mode);
  data[19] = static_cast<uint8_t>(fan);
  data[20] = static_cast<uint8_t>(swing);

  return SendMessage(APP_REQ, data, sizeof(data), 0, 0, 0, 10);
}

int BleProtocol::ControlIRCmd(const std::string &command)
{
  if (command.empty() || (command.size() % 2) != 0)
    return CODE_FORMAT_ERROR;

  std::vector<uint8_t> data(command.size() / 2, 0);
  for (size_t i = 0; i < command.size(); i += 2)
  {
    if (!isxdigit(static_cast<unsigned char>(command[i])) ||
        !isxdigit(static_cast<unsigned char>(command[i + 1])))
    {
      return CODE_FORMAT_ERROR;
    }
    data[i / 2] =
        static_cast<uint8_t>(std::stoi(command.substr(i, 2), nullptr, 16));
  }

  mtxWaitSendUart.lock();
  Write(data.data(), data.size());
  mtxWaitSendUart.unlock();
  return CODE_OK;
}

int BleProtocol::LearnIRV3Code(uint16_t devAddr, uint8_t department,
                               uint16_t code)
{
  if (code == 0)
    return CODE_FORMAT_ERROR;

  uint8_t data[] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xFF, 0xFF,
      0xE4, 0x11, 0x02, 0xE5, 0x00,
      0x55, 0x03,
      0x00, 0x00, 0x00};

  data[6] = static_cast<uint8_t>(devAddr >> 8);
  data[7] = static_cast<uint8_t>(devAddr & 0xFF);
  data[15] = department;
  data[16] = static_cast<uint8_t>(code);
  data[17] = static_cast<uint8_t>(code >> 8);

  LOGI("LearnIRV3Code addr=0x%04X department=%u code=%u", devAddr,
       department, code);
  return SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 10);
}

int BleProtocol::DeleteIRV3Code(uint16_t devAddr, uint16_t id, uint8_t type)
{
  uint8_t data[] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xFF, 0xFF,
      0xE4, 0x11, 0x02, 0xE5, 0x00,
      0x55, 0x04,
      0x00, 0x00, 0x00};

  data[6] = static_cast<uint8_t>(devAddr >> 8);
  data[7] = static_cast<uint8_t>(devAddr & 0xFF);
  data[15] = static_cast<uint8_t>(id);
  data[16] = static_cast<uint8_t>(id >> 8);
  data[17] = type;

  LOGI("DeleteIRV3Code addr=0x%04X id=%u type=%u", devAddr, id, type);
  return SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 10);
}

int BleProtocol::GetIRV3Codes(uint16_t devAddr)
{
  uint8_t data[] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xFF, 0xFF,
      0xE4, 0x11, 0x02, 0xE5, 0x00,
      0x55, 0x06};

  data[6] = static_cast<uint8_t>(devAddr >> 8);
  data[7] = static_cast<uint8_t>(devAddr & 0xFF);

  LOGI("GetIRV3Codes addr=0x%04X", devAddr);
  return SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 10);
}

int BleProtocol::ScanStopSeftPowerRemote(uint16_t devAddr, uint8_t status)
{
  LOGD("Scan SeftPowerRemote: 0x%04X, status: %d", devAddr, status);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t dataCompare[] = {(uint8_t)(devAddr & 0xFF),
                           (uint8_t)((devAddr >> 8) & 0xFF),
                           1,
                           0,
                           RD_OPCODE_CONFIG_RSP,
                           0x11,
                           0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint8_t status;
    uint8_t future[5];
  } scan_message_t;
  scan_message_t scan_message = {0};
  memset(&scan_message, 0x00, sizeof(scan_message));
  scan_message.ble_message_header.devAddr = devAddr;
  scan_message.opcodeVendor = RD_OPCODE_CONFIG;
  scan_message.vendorId = RD_VENDOR_ID;
  scan_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  scan_message.header = RD_HEADER_SEFTPOWER_REMOTE_SCAN;
  scan_message.status = status;
  uint16_t timeout = 10000;
  if (!status)
    timeout = 1000;
  int rs = SendMessage(APP_REQ, (uint8_t *)&scan_message,
                       sizeof(scan_message_t), HCI_GATEWAY_RSP_OP_CODE, dataRsp,
                       &lenRsp, timeout, dataCompare, 0, 7);
  if (rs == CODE_OK)
  {
    memcpy(&scanDevicePairMessage, dataRsp, sizeof(scan_device_pair_message_t));
    // LOGW("parent: %d", scanDevicePairMessage.parentAddr);
    // LOGW("mac: %02x%02x%02x%02x", scanDevicePairMessage.mac[0],
    // scanDevicePairMessage.mac[1], scanDevicePairMessage.mac[2],
    // scanDevicePairMessage.mac[3]); LOGW("type: %d",
    // scanDevicePairMessage.type);
  }
  return rs;
}

int BleProtocol::SaveSeftPowerRemote(scan_device_pair_message_t scanMessage,
                                     uint16_t childDev)
{
  LOGD("Save SeftPowerRemote");
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t dataCompare[] = {(uint8_t)(scanMessage.parentAddr & 0xFF),
                           (uint8_t)((scanMessage.parentAddr >> 8) & 0xFF),
                           1,
                           0,
                           RD_OPCODE_CONFIG_RSP,
                           0x11,
                           0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint8_t mac[4];
    uint16_t childDev;
  } save_message_t;
  save_message_t save_message = {0};
  memset(&save_message, 0x00, sizeof(save_message));
  save_message.ble_message_header.devAddr = scanMessage.parentAddr;
  save_message.opcodeVendor = RD_OPCODE_CONFIG;
  save_message.vendorId = RD_VENDOR_ID;
  save_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  save_message.header = RD_HEADER_SEFTPOWER_REMOTE_SAVE;
  save_message.childDev = childDev;
  for (int i = 0; i < 4; i++)
  {
    save_message.mac[i] = scanMessage.mac[i];
  }
  int rs = SendMessage(APP_REQ, (uint8_t *)&save_message,
                       sizeof(save_message_t), HCI_GATEWAY_RSP_OP_CODE, dataRsp,
                       &lenRsp, 3000, dataCompare, 0, 7);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint8_t opcodeRsp;
      uint16_t vendorId;
      uint16_t header;
      uint8_t mac[4];
      uint8_t status;
    } save_rsp_message_t;
    save_rsp_message_t *save_rsp_message = (save_rsp_message_t *)dataRsp;
    if (save_rsp_message->header == RD_HEADER_SEFTPOWER_REMOTE_SAVE)
    {
      uint32_t parentAddrRsp =
          save_rsp_message->mac[0] | save_rsp_message->mac[1] << 8 |
          save_rsp_message->mac[2] << 16 | save_rsp_message->mac[3] << 24;
      uint32_t parentAddr = scanMessage.mac[0] | scanMessage.mac[1] << 8 |
                            scanMessage.mac[2] << 16 | scanMessage.mac[3] << 24;
      if ((parentAddrRsp != parentAddr) || (save_rsp_message->status == 0))
        rs = CODE_ERROR;
    }
  }
  return rs;
}

int BleProtocol::SetSceneSeftPowerRemote(uint16_t devAddr,
                                         uint16_t seftPowerAddr, uint8_t button,
                                         uint8_t mode, uint16_t scene)
{
  LOGD("SetSceneSeftPowerRemote parent: 0x%04X, device: 0x%04X, button: %d, "
       "mode: %d, scene: %d",
       devAddr, seftPowerAddr, button, mode, scene);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t dataCompare[] = {(uint8_t)(devAddr & 0xFF),
                           (uint8_t)((devAddr >> 8) & 0xFF),
                           1,
                           0,
                           RD_OPCODE_CONFIG_RSP,
                           0x11,
                           0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint16_t childDev;
    uint8_t button;
    uint8_t mode;
    uint16_t scene;
  } scene_message_t;
  scene_message_t scene_message = {0};
  memset(&scene_message, 0x00, sizeof(scene_message));
  scene_message.ble_message_header.devAddr = devAddr;
  scene_message.opcodeVendor = RD_OPCODE_CONFIG;
  scene_message.vendorId = RD_VENDOR_ID;
  scene_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  scene_message.header = RD_HEADER_SEFTPOWER_REMOTE_SET_SCENE;
  scene_message.childDev = seftPowerAddr;
  scene_message.button = button;
  scene_message.mode = mode;
  scene_message.scene = scene;

  int rs = SendMessage(APP_REQ, (uint8_t *)&scene_message,
                       sizeof(scene_message_t), HCI_GATEWAY_RSP_OP_CODE,
                       dataRsp, &lenRsp, 1000, dataCompare, 0, 7);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint8_t opcodeRsp;
      uint16_t vendorId;
      uint16_t header;
      uint8_t status;
    } scene_rsp_message_t;
    scene_rsp_message_t *scene_rsp_message = (scene_rsp_message_t *)dataRsp;
    if (scene_rsp_message->header != RD_HEADER_SEFTPOWER_REMOTE_SET_SCENE)
      rs = CODE_ERROR;
  }
  return rs;
}

int BleProtocol::DelSceneSeftPowerRemote(uint16_t devAddr,
                                         uint16_t seftPowerAddr, uint8_t button,
                                         uint8_t mode)
{
  LOGD("DelSceneSeftPowerRemote parent: 0x%04X, device: 0x%04X, button: %d, "
       "mode: %d",
       devAddr, seftPowerAddr, button, mode);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t dataCompare[] = {(uint8_t)(devAddr & 0xFF),
                           (uint8_t)((devAddr >> 8) & 0xFF),
                           1,
                           0,
                           RD_OPCODE_CONFIG_RSP,
                           0x11,
                           0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint16_t childDev;
    uint8_t button;
    uint8_t mode;
    uint16_t feature;
  } scene_message_t;
  scene_message_t scene_message = {0};
  memset(&scene_message, 0x00, sizeof(scene_message));
  scene_message.ble_message_header.devAddr = devAddr;
  scene_message.opcodeVendor = RD_OPCODE_CONFIG;
  scene_message.vendorId = RD_VENDOR_ID;
  scene_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  scene_message.header = RD_HEADER_SEFTPOWER_REMOTE_DEL_SCENE;
  scene_message.childDev = seftPowerAddr;
  scene_message.button = button;
  scene_message.mode = mode;
  int rs = SendMessage(APP_REQ, (uint8_t *)&scene_message,
                       sizeof(scene_message_t), HCI_GATEWAY_RSP_OP_CODE,
                       dataRsp, &lenRsp, 1000, dataCompare, 0, 7);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint8_t opcodeRsp;
      uint16_t vendorId;
      uint16_t header;
      uint8_t status;
    } scene_rsp_message_t;
    scene_rsp_message_t *scene_rsp_message = (scene_rsp_message_t *)dataRsp;
    if (scene_rsp_message->header != RD_HEADER_SEFTPOWER_REMOTE_DEL_SCENE)
      rs = CODE_ERROR;
  }
  return rs;
}

int BleProtocol::ResetSeftPowerRemote(uint16_t devAddr,
                                      uint16_t seftPowerAddr)
{
  LOGD("ResetSeftPowerRemote parent: 0x%04X, device: 0x%04X", devAddr,
       seftPowerAddr);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t dataCompare[] = {(uint8_t)(devAddr & 0xFF),
                           (uint8_t)((devAddr >> 8) & 0xFF),
                           1,
                           0,
                           RD_OPCODE_CONFIG_RSP,
                           0x11,
                           0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint16_t childDev;
    uint8_t feature[4];
  } reset_message_t;
  reset_message_t reset_message = {0};
  memset(&reset_message, 0x00, sizeof(reset_message));
  reset_message.ble_message_header.devAddr = devAddr;
  reset_message.opcodeVendor = RD_OPCODE_CONFIG;
  reset_message.vendorId = RD_VENDOR_ID;
  reset_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  reset_message.header = RD_HEADER_SEFTPOWER_REMOTE_RESET;
  reset_message.childDev = seftPowerAddr;
  int rs = SendMessage(APP_REQ, (uint8_t *)&reset_message,
                       sizeof(reset_message_t), HCI_GATEWAY_RSP_OP_CODE,
                       dataRsp, &lenRsp, 1000, dataCompare, 0, 7);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint8_t opcodeRsp;
      uint16_t vendorId;
      uint16_t header;
      uint16_t childAddr;
      uint8_t status;
    } reset_rsp_message_t;
    reset_rsp_message_t *reset_rsp_message = (reset_rsp_message_t *)dataRsp;
    if (reset_rsp_message->header != RD_HEADER_SEFTPOWER_REMOTE_RESET)
      rs = CODE_ERROR;
  }
  return rs;
}

int BleProtocol::ControlRgbSwitch(uint16_t devAddr, uint8_t button, uint8_t b,
                                  uint8_t g, uint8_t r, uint8_t dimOn,
                                  uint8_t dimOff)
{
  LOGD("ControlRgbSwitch 0x%04X, button %d, r %d, g %d, b %d, dimon %d, dimOff "
       "%d",
       devAddr, button, r, g, b, dimOn, dimOff);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t controlRgbSwitchHeader[] = {(uint8_t)(devAddr & 0xFF),
                                      (uint8_t)((devAddr >> 8) & 0xFF),
                                      1,
                                      0,
                                      RD_OPCODE_CONFIG_RSP,
                                      0x11,
                                      0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint8_t button;
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t dimOn;
    uint8_t dimOff;
  } controlrgb_switch_message_t;
  controlrgb_switch_message_t controlrgb_switch_message = {0};
  memset(&controlrgb_switch_message, 0x00, sizeof(controlrgb_switch_message));
  controlrgb_switch_message.ble_message_header.devAddr = devAddr;
  controlrgb_switch_message.opcodeVendor = RD_OPCODE_CONFIG;
  controlrgb_switch_message.vendorId = RD_VENDOR_ID;
  controlrgb_switch_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  controlrgb_switch_message.header = RD_HEADER_CONFIG_CONTROL_RGB_SWITCH;
  controlrgb_switch_message.button = button;
  controlrgb_switch_message.b = b;
  controlrgb_switch_message.g = g;
  controlrgb_switch_message.r = r;
  controlrgb_switch_message.dimOn = dimOn;
  controlrgb_switch_message.dimOff = dimOff;
  int rs =
      SendMessage(APP_REQ, (uint8_t *)&controlrgb_switch_message,
                  sizeof(controlrgb_switch_message_t), HCI_GATEWAY_RSP_OP_CODE,
                  dataRsp, &lenRsp, 1000, controlRgbSwitchHeader, 0, 7);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint8_t opcodeRsp;
      uint16_t vendorId;
      uint16_t header;
      uint8_t button;
    } controlrgb_switch_rsp_message_t;
    controlrgb_switch_rsp_message_t *controlrgb_switch_rsp_message =
        (controlrgb_switch_rsp_message_t *)dataRsp;
    if (controlrgb_switch_rsp_message->header ==
            RD_HEADER_CONFIG_CONTROL_RGB_SWITCH &&
        controlrgb_switch_rsp_message->button == button)
    {
      return CODE_OK;
    }
    LOGW("control rgb switch resp state not match with input control");
  }
  LOGW("control rgb switch err");
  return CODE_ERROR;
}

int BleProtocol::ControlRelayOfSwitch(uint16_t devAddr, uint32_t type,
                                      uint8_t relay, uint8_t value)
{
  LOGD("ControlRelayOfSwitch 0x%04X, relayid %d, value %d", devAddr, relay,
       value);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t controlRelaySwitchHeader[] = {(uint8_t)(devAddr & 0xFF),
                                        (uint8_t)((devAddr >> 8) & 0xFF), 1, 0};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint8_t relay;
    uint8_t value;
  } control_relay_switch_message_t;
  control_relay_switch_message_t control_relay_switch_message = {0};
  memset(&control_relay_switch_message, 0x00,
         sizeof(control_relay_switch_message));
  control_relay_switch_message.ble_message_header.devAddr = devAddr;
  control_relay_switch_message.opcodeVendor = RD_OPCODE_CONFIG;
  control_relay_switch_message.vendorId = RD_VENDOR_ID;
  control_relay_switch_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  control_relay_switch_message.header = RD_HEADER_CONFIG_CONTROL_RELAY_SWITCH_4;
  control_relay_switch_message.relay = relay;
  control_relay_switch_message.value = value;
  int rs = SendMessage(APP_REQ, (uint8_t *)&control_relay_switch_message,
                       sizeof(control_relay_switch_message_t),
                       HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 1000,
                       controlRelaySwitchHeader, 0, 4);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint8_t opcodeRsp;
      uint16_t vendorId;
      uint16_t header;
      uint8_t relay;
      uint8_t value;
    } control_relay_switch_rsp_message_t;
    control_relay_switch_rsp_message_t *control_relay_switch_rsp_message =
        (control_relay_switch_rsp_message_t *)dataRsp;
    if ((control_relay_switch_rsp_message->header ==
             RD_HEADER_CONFIG_CONTROL_RELAY_SWITCH_4 &&
         control_relay_switch_rsp_message->relay == relay &&
         control_relay_switch_rsp_message->value == value) ||
        (control_relay_switch_rsp_message->opcodeRsp ==
             (G_ONOFF_STATUS & 0xFF) &&
         (control_relay_switch_rsp_message->vendorId & 0xFF) ==
             ((G_ONOFF_STATUS >> 8) & 0xFF)))
    {
      return CODE_OK;
    }
    LOGW("control relay switch resp state not match with input control");
  }
  LOGW("control relay switch err");
  return CODE_ERROR;
}

int BleProtocol::SetIdCombine(uint16_t devAddr, uint16_t id)
{
  LOGD("SetIdCombine 0x%04x, id %d", devAddr, id);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t SetIdCombineHeader[] = {(uint8_t)(devAddr & 0xFF),
                                  (uint8_t)((devAddr >> 8) & 0xFF),
                                  1,
                                  0,
                                  RD_OPCODE_CONFIG_RSP,
                                  0x11,
                                  0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint16_t id;
  } set_id_combine_message_t;
  set_id_combine_message_t set_id_combine_message = {0};
  memset(&set_id_combine_message, 0x00, sizeof(set_id_combine_message));
  set_id_combine_message.ble_message_header.devAddr = devAddr;
  set_id_combine_message.opcodeVendor = RD_OPCODE_CONFIG;
  set_id_combine_message.vendorId = RD_VENDOR_ID;
  set_id_combine_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  set_id_combine_message.header = RD_HEADER_CONFIG_SET_ID_COMBINE;
  set_id_combine_message.id = id;
  int rs =
      SendMessage(APP_REQ, (uint8_t *)&set_id_combine_message,
                  sizeof(set_id_combine_message_t), HCI_GATEWAY_RSP_OP_CODE,
                  dataRsp, &lenRsp, 2000, SetIdCombineHeader, 0, 7);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint8_t opcodeRsp;
      uint16_t vendorId;
      uint16_t header;
      uint16_t id;
    } set_id_combine_rsp_message_t;
    set_id_combine_rsp_message_t *set_id_combine_rsp_message =
        (set_id_combine_rsp_message_t *)dataRsp;
    if (set_id_combine_rsp_message->header == RD_HEADER_CONFIG_SET_ID_COMBINE &&
        set_id_combine_rsp_message->id == id)
    {
      return CODE_OK;
    }
    LOGW("set id combine resp state not match with input control");
  }
  LOGW("set id combine err");
  return CODE_ERROR;
}

int BleProtocol::CountDownSwitch(uint16_t devAddr, uint16_t timer,
                                 uint8_t status)
{
  LOGD("CountDownSwitch 0x%04x, timer %d, status %d", devAddr, timer, status);
  uint8_t dataRsp[100];
  int lenRsp;
  // B5 countdown responses start with gwAddr, then devAddr and opcode F2:
  //   gwLo gwHi devLo devHi F2 00 0A 07 status timerLo timerHi
  uint8_t timerHeader[] = {(uint8_t)(devAddr & 0xFF),
                           (uint8_t)((devAddr >> 8) & 0xFF),
                           RD_OPCODE_CONFIG};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint8_t status;
    uint16_t timer;
  } timer_message_t;
  timer_message_t timer_message = {0};
  memset(&timer_message, 0x00, sizeof(timer_message));
  timer_message.ble_message_header.devAddr = devAddr;
  timer_message.opcodeVendor = RD_OPCODE_CONFIG;
  timer_message.vendorId = RD_VENDOR_ID;
  timer_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  timer_message.header = RD_HEADER_CONFIG_SET_TIMER;
  timer_message.status = status;
  timer_message.timer = timer;
  int rs = SendMessage(APP_REQ, (uint8_t *)&timer_message,
                       sizeof(timer_message_t), HCI_GATEWAY_RSP_OP_CODE,
                       dataRsp, &lenRsp, 1000, timerHeader, 2, 3);
  if (rs == CODE_OK)
  {
    if (lenRsp >= 11)
    {
      uint16_t responseAddr =
          (uint16_t)dataRsp[2] | ((uint16_t)dataRsp[3] << 8);
      uint16_t responseTimer =
          (uint16_t)dataRsp[9] | ((uint16_t)dataRsp[10] << 8);
      uint8_t responseCommand = dataRsp[7];
      uint8_t responseStatus = dataRsp[8];

      if (responseAddr == devAddr &&
          dataRsp[4] == RD_OPCODE_CONFIG &&
          responseCommand == (RD_HEADER_CONFIG_SET_TIMER >> 8) &&
          responseStatus == status && responseTimer == timer)
      {
        return CODE_OK;
      }

      LOGW("CountDownSwitch response mismatch: addr=0x%04X command=0x%02X "
           "status=%u timer=%u",
           responseAddr, responseCommand, responseStatus, responseTimer);
    }
    else
    {
      LOGW("CountDownSwitch response too short: %d", lenRsp);
    }
  }
  LOGW("CountDownSwitch err");
  return CODE_ERROR;
}

int BleProtocol::UpdateStatusRelaySwitch(uint16_t devAddr, uint32_t type)
{
  // LOGD("Update status Relay Switch 0x%04x", devAddr);
  uint8_t dataRsp[100];
  int lenRsp;
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
  } request_status_message_t;
  request_status_message_t request_status_message = {0};
  memset(&request_status_message, 0x00, sizeof(request_status_message));
  request_status_message.ble_message_header.devAddr = bswap_16(devAddr);
  request_status_message.opcodeVendor = RD_OPCODE_CONFIG;
  request_status_message.vendorId = RD_VENDOR_ID;
  request_status_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  request_status_message.header = RD_HEADER_REQUEST_STATUS_SWITCH;
  int rs =
      SendMessage(APP_REQ, (uint8_t *)&request_status_message,
                  sizeof(request_status_message_t), HCI_GATEWAY_RSP_OP_CODE,
                  dataRsp, &lenRsp, 1000);
  if (rs == CODE_OK)
  {
    return CODE_OK;
  }
  LOGW("request status switch 0x%04x err", devAddr);
  return CODE_ERROR;
}

int BleProtocol::ConfigStatusStartupSwitch(uint16_t devAddr, uint8_t status,
                                           uint32_t type)
{
  LOGD("ConfigStatusStartup 0x%04x, status %d", devAddr, status);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t statusHeader[] = {(uint8_t)(devAddr & 0xFF),
                            (uint8_t)((devAddr >> 8) & 0xFF),
                            1,
                            0,
                            RD_OPCODE_CONFIG_RSP,
                            0x11,
                            0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint8_t status;
  } status_message_t;
  status_message_t status_message = {0};
  memset(&status_message, 0x00, sizeof(status_message));
  status_message.ble_message_header.devAddr = devAddr;
  status_message.opcodeVendor = RD_OPCODE_CONFIG;
  status_message.vendorId = RD_VENDOR_ID;
  status_message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  status_message.header = RD_HEADER_CONFIG_STATUS_STARTUP_SWITCH;
  status_message.status = status;
  int rs = SendMessage(APP_REQ, (uint8_t *)&status_message,
                       sizeof(status_message_t), HCI_GATEWAY_RSP_OP_CODE,
                       dataRsp, &lenRsp, 1000, statusHeader, 0, 7);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint8_t opcodeRsp;
      uint16_t vendorId;
      uint16_t header;
      uint8_t status;
    } status_rsp_message_t;
    status_rsp_message_t *status_rsp_message = (status_rsp_message_t *)dataRsp;
    if (status_rsp_message->header == RD_HEADER_CONFIG_STATUS_STARTUP_SWITCH &&
        status_rsp_message->status == status)
    {
      return CODE_OK;
    }
    LOGW("status startup resp state not match with input control");
  }
  LOGW("status startup switch err");
  return CODE_ERROR;
}

int BleProtocol::UpdateTypeSceneSwitch(uint16_t devAddr, uint8_t regType)
{
  LOGD("UpdateTypeSceneSwitch 0x%04x, regType %d", devAddr, regType);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t rspHeader[] = {(uint8_t)(devAddr & 0xFF),
                         (uint8_t)((devAddr >> 8) & 0xFF),
                         1,
                         0,
                         RD_OPCODE_CONFIG_RSP,
                         0x11,
                         0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint8_t regType;
  } scene_switch_type_message_t;
  scene_switch_type_message_t message = {0};
  message.ble_message_header.devAddr = devAddr;
  message.opcodeVendor = RD_OPCODE_CONFIG;
  message.vendorId = RD_VENDOR_ID;
  message.opcodeRsp = RD_OPCODE_CONFIG_RSP;
  message.header = HG_HEADER_000C;
  message.regType = regType;

  int rs = SendMessage(APP_REQ, (uint8_t *)&message, sizeof(message),
                       HCI_GATEWAY_RSP_OP_CODE, dataRsp, &lenRsp, 1000,
                       rspHeader, 0, 7);
  if (rs == CODE_OK)
  {
    typedef struct __attribute__((packed))
    {
      uint16_t devAddr;
      uint16_t gwAddr;
      uint8_t opcodeRsp;
      uint16_t vendorId;
      uint16_t header;
      uint8_t regType;
    } scene_switch_type_rsp_message_t;
    scene_switch_type_rsp_message_t *rsp =
        (scene_switch_type_rsp_message_t *)dataRsp;
    if (rsp->header == HG_HEADER_000C)
    {
      return CODE_OK;
    }
    LOGW("UpdateTypeSceneSwitch response mismatch");
  }
  LOGW("UpdateTypeSceneSwitch err");
  return CODE_ERROR;
}

int BleProtocol::GetInfogw()
{
  LOGD("GetInfogw");
  // Lệnh 1: RESET_GW (0xE9 FF 02) — chờ chip khởi động lại, delay 5s theo spec
  uint8_t n = 0x02;
  SendMessage(SYSTEM_REQ, &n, 1, 0, 0, 0, 5000);
  // Lệnh 2: SET_NODE_PARA_GW (0xE9 FF 16 00) — delay 1s
  uint8_t d[2] = {0x16, 0x00};
  return SendMessage(SYSTEM_REQ, d, 2, 0, 0, 0, 1000);
}

int BleProtocol::GetInfoMesh()
{
  LOGD("GetInfoMesh");
  uint8_t d = HCI_GATEWAY_CMD_GET_PRO_SELF_STS;
  uint8_t dataRsp[100];
  int lenRsp;
  return SendMessage(SYSTEM_REQ, &d, 1, HCI_GATEWAY_CMD_PRO_STS_RSP, dataRsp,
                     &lenRsp, 2000);
}

int BleProtocol::UpdateDeviceKeyDev(uint16_t devAddr, uint16_t elementCount, std::string devKeyDev)
{
  LOGD("UpdateDeviceKeyDev: addr=0x%04X", devAddr);
  devKeyDev.erase(std::remove(devKeyDev.begin(), devKeyDev.end(), '-'), devKeyDev.end());
  if (devKeyDev.size() == 32)
  {
    typedef struct __attribute__((packed))
    {
      uint8_t header;
      uint16_t devAddr;
      uint16_t element;
      uint8_t devKey[16];
    } update_devkey_device_t;
    update_devkey_device_t update_devkey_device = {
        .header = 0x12,
        .devAddr = devAddr};
    update_devkey_device.element = elementCount;
    for (int i = 0; i < devKeyDev.length(); i += 2)
    {
      std::string hexByte = devKeyDev.substr(i, 2);
      update_devkey_device.devKey[i / 2] = std::stoi(hexByte, nullptr, 16);
    }
    return SendMessage(SYSTEM_REQ, (uint8_t *)&update_devkey_device, sizeof(update_devkey_device_t), 0, 0, 0, 600);
  }
  else
  {
    LOGW("devKey error");
  }
  return CODE_ERROR;
}

int BleProtocol::UpdateDeviceKeyGateway(uint16_t gwAddr, string devKeyDev)
{
  LOGD("UpdateDeviceKeyGateway: addr=0x%04X", gwAddr);
  devKeyDev.erase(std::remove(devKeyDev.begin(), devKeyDev.end(), '-'),
                  devKeyDev.end());
  if (devKeyDev.size() == 32)
  {
    typedef struct __attribute__((packed))
    {
      uint8_t header;
      uint16_t devAddr;
      uint16_t element;
      uint8_t devKey[16];
    } update_devkey_device_t;
    update_devkey_device_t update_devkey_device = {.header = 0x12,
                                                   .devAddr = gwAddr};
    update_devkey_device.element = 0x0001;
    for (int i = 0; i < devKeyDev.length(); i += 2)
    {
      std::string hexByte = devKeyDev.substr(i, 2);
      update_devkey_device.devKey[i / 2] = std::stoi(hexByte, nullptr, 16);
    }
    return SendMessage(SYSTEM_REQ, (uint8_t *)&update_devkey_device,
                       sizeof(update_devkey_device_t), 0, 0, 0, 1000);
  }
  else
  {
    LOGW("UpdateDeviceKeyGateway error");
  }
  return CODE_ERROR;
}

int BleProtocol::UpdateNetKey(uint16_t gwAddr, string netKeyStr,
                              uint32_t ivIndex, uint16_t netKeyIdx)
{
  LOGD("UpdateNetKey: ivIndex=%lu netKeyIdx=%u",
       static_cast<unsigned long>(ivIndex), netKeyIdx);
  netKeyStr.erase(std::remove(netKeyStr.begin(), netKeyStr.end(), '-'),
                  netKeyStr.end());
  if (netKeyStr.size() == 32)
  {
    typedef struct __attribute__((packed))
    {
      uint8_t opcode;       // 1 byte (0x09)
      uint8_t netKey[16];   // 16 bytes
      uint16_t netKeyIndex; // 2 bytes (00 00)
      uint8_t ivUpdateFlag; // 1 byte (00)
      uint32_t ivIndex;     // 4 bytes (00 00 00 00)
      uint16_t gwAddr;      // 2 bytes (ví dụ 00 01)
    } set_netkey_message_t;
    set_netkey_message_t msg = {0};
    msg.opcode = 0x09;

    for (int i = 0; i < 32; i += 2)
    {
      uint8_t b = std::stoi(netKeyStr.substr(i, 2), nullptr, 16);
      msg.netKey[i / 2] = b;
      this->netKey[i / 2] = b; // Save to member
    }

    msg.netKeyIndex = (uint16_t)((netKeyIdx >> 8) | (netKeyIdx << 8));
    msg.ivUpdateFlag = 0x00;
    msg.ivIndex = (uint32_t)((ivIndex >> 24) | ((ivIndex & 0x00FF0000) >> 8) |
                             ((ivIndex & 0x0000FF00) << 8) |
                             (ivIndex << 24));              // Big Endian IV
    msg.gwAddr = (uint16_t)((gwAddr >> 8) | (gwAddr << 8)); // Big Endian Addr

    int rs = SendMessage(SYSTEM_REQ, (uint8_t *)&msg,
                         sizeof(set_netkey_message_t), 0, 0, 0, 1000);
    if (rs == CODE_OK)
    {
      LOGI("UpdateNetKey success");
    }
    else
    {
      LOGE("UpdateNetKey failed, rs: %d", rs);
    }
    return rs;
  }
  return CODE_ERROR;
}

int BleProtocol::UpdateDevKey(uint16_t gwAddr, string devKeyStr)
{
  LOGD("UpdateDevKey: addr=0x%04X", gwAddr);
  devKeyStr.erase(std::remove(devKeyStr.begin(), devKeyStr.end(), '-'),
                  devKeyStr.end());
  if (devKeyStr.size() == 32)
  {
    typedef struct __attribute__((packed))
    {
      uint8_t opcode;
      uint16_t reserved;
      uint8_t gwKey[16];
    } set_gwkey_message_t;
    set_gwkey_message_t msg = {0};
    msg.opcode = 0x0D;
    // release_3009 leaves the reserved bytes at zero.
    (void)gwAddr;
    for (int i = 0; i < 32; i += 2)
    {
      uint8_t b = std::stoi(devKeyStr.substr(i, 2), nullptr, 16);
      msg.gwKey[i / 2] = b;
      this->gwKey[i / 2] = b; // Save to member
    }
    int rs =
        SendMessage(SYSTEM_REQ, (uint8_t *)&msg, sizeof(msg), 0, 0, 0, 1000);
    if (rs == CODE_OK)
    {
      LOGI("UpdateDevKey success");
      return CODE_OK;
    }
    else
    {
      LOGE("UpdateDevKey failed, rs: %d", rs);
    }
  }
  return CODE_ERROR;
}

int BleProtocol::UpdateAppKey(string appKeyStr, uint16_t appKeyIdx)
{
  uint8_t keyBytes[16];
  if (!LoadMeshKey(appKeyStr, keyBytes))
    return CODE_ERROR;
  uint8_t request[20] = {HCI_GATEWAY_CMD_START_KEYBIND, 0x00,
                         static_cast<uint8_t>(appKeyIdx >> 8),
                         static_cast<uint8_t>(appKeyIdx)};
  memcpy(request + 4, keyBytes, sizeof(keyBytes));
  uint8_t response[100] = {0};
  int responseLen = 0;
  int rs = SendMessage(SYSTEM_REQ, request, sizeof(request),
                       HCI_GATEWAY_CMD_KEY_BIND_EVT, response, &responseLen,
                       5000);
  // The supplied startup log shows 0x02 here, followed by a successful
  // 0x01 BindingAll. release_3009 accepts the 0x8A event without inspecting
  // its status; preserve that behavior for the observed values.
  if (rs != CODE_OK || responseLen != 1 ||
      (response[0] != 1 && response[0] != 2))
  {
    LOGE("UpdateAppKey failed: rs=%d responseLen=%d status=%d", rs,
         responseLen, responseLen > 0 ? response[0] : -1);
    return CODE_ERROR;
  }
  LOGI("UpdateAppKey completed with module status=0x%02X", response[0]);
  memcpy(this->appKey, keyBytes, sizeof(keyBytes));
  SLEEP_MS(500);
  return CODE_OK;
}

int BleProtocol::EnableReporting()
{
  LOGD("EnableReporting EC FF 01");
  uint8_t data = 0x01;
  int rs = SendMessage(0xFFEC, &data, 1, 0, 0, 0, 1000); // EC FF 01
  if (rs == CODE_OK)
  {
    LOGI("EnableReporting success");
  }
  else
  {
    LOGE("EnableReporting failed, rs: %d", rs);
  }
  return rs;
}

int BleProtocol::UpdateMaxAddr(uint16_t addr)
{
  LOGD("UpdateMaxAddr: %d", addr);
  uint8_t dataRsp[100];
  int lenRsp;
  typedef struct __attribute__((packed))
  {
    uint8_t opcode;
    pro_net_info_t data;
  } provision_message_t;
  provision_message_t provision_message;
  memset(&provision_message, 0x00, sizeof(provision_message));
  provision_message.opcode = HCI_GATEWAY_CMD_SET_NODE_PARA;
  memcpy(&provision_message.data.netKey[0], &pro_net_info.netKey[0],
         sizeof(pro_net_info_t));
  provision_message.data.unicast_address = addr;
  return SendMessage(SYSTEM_REQ, (uint8_t *)&provision_message,
                     sizeof(provision_message_t), HCI_GATEWAY_CMD_PROVISION_EVT,
                     dataRsp, &lenRsp, 15000);
}

int BleProtocol::ResetGateway()
{
  LOGD("ResetGateway E9 FF 02");
  uint8_t opcode = 0x02;
  int rs = SendMessage(SYSTEM_REQ, &opcode, 1, 0, 0, 0, 5000);
  if (rs == CODE_OK)
  {
    LOGI("ResetGateway success");
  }
  else
  {
    LOGE("ResetGateway failed, rs: %d", rs);
  }
  return rs;
}

int BleProtocol::SetNodeParams()
{
  LOGD("SetNodeParams E9 FF 16 00");
  uint8_t data[2] = {0x16, 0x00};
  int rs = SendMessage(SYSTEM_REQ, data, 2, 0, 0, 0, 1000);
  if (rs == CODE_OK)
  {
    LOGI("SetNodeParams success");
  }
  else
  {
    LOGE("SetNodeParams failed, rs: %d", rs);
  }
  return rs;
}

int BleProtocol::SetConditionDeviceScene(int mode, uint16_t devAddr, uint16_t sceneId, string type, uint8_t data1, uint8_t data2)
{
  uint8_t data[] = {
      0xe8, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xff, 0xff,
      0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  LOGD("SetConditionDeviceScene mode=%d devAddr=%04X sceneId=%04X type=%s data1=%d data2=%d", mode, devAddr, sceneId, type.c_str(), data1, data2);
  if (mode == -2)
  {
    // Thêm mới condition device scene
    LOGD("Adding new condition device scene");
    uint8_t data[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xff, 0xff,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    int dataLen = sizeof(data);
    // device addr
    // APP_REQ destination is little-endian, as in SetOnOffLight.
    data[6] = static_cast<uint8_t>(devAddr);
    data[7] = static_cast<uint8_t>(devAddr >> 8);

    /*
        data[8] = opcode1
        data[9] = opcode2
        data[10] = opcode3 (if needed)
        ...

        data[16] = type
        data[17] = data1
        data[18] = data2 (if needed)
    */

    if (type == "HG_BLE_IR")
    {
      uint8_t irData[] = {
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          static_cast<uint8_t>(devAddr), static_cast<uint8_t>(devAddr >> 8),
          0xE4, 0x11, 0x02, 0x00, 0x00, 0x06,
          static_cast<uint8_t>(sceneId >> 8), static_cast<uint8_t>(sceneId),
          static_cast<uint8_t>(2 << 6), static_cast<uint8_t>(data1),
          static_cast<uint8_t>(data1 >> 8)};

      // hc_lux queues this vendor command without waiting for the generic
      // E1/11/02/50 scene response. IR devices only acknowledge the UART send.
      return SendMessage(APP_REQ, irData, sizeof(irData), 0, nullptr, nullptr,
                         10);
    }
    else if (type == HG_BLE_SWITCH_SCENE)
    {
      // Switch Scene
      data[4] = 0x20;
      data[8] = 0xE0;
      data[9] = 0x11;
      data[10] = 0x02;
      data[13] = 0x50;
      data[15] = (uint8_t)(sceneId >> 8);
      data[16] = (uint8_t)(sceneId);

      // dp_type / action
      if (data1 <= 4)
        data1 = 0;
      else
        data1 = 1;
      data[17] = data1;
    }
    else if (type == HG_BLE_CURTAIN_IH35 ||
             type == HG_BLE_CURTAIN_IH68 ||
             type == HG_BLE_CURTAIN_1_LAYER_MECHANICAL ||
             type == HG_BLE_CURTAIN_NORMAL_NEW ||
             type == HG_BLE_CURTAIN_MODULE_NEW)
    {
      // Curtain
      data[8] = 0xE0;
      data[9] = 0x11;
      data[10] = 0x02;
      data[13] = 0x50;
      data[15] = (uint8_t)(sceneId >> 8);
      data[16] = (uint8_t)(sceneId);

      // operator / position
      data[17] = data1;
    }
    else if (type == HG_BLE_SENSOR_PRESENCE ||
             type == RD_BLE_SENSOR_SMOKE ||
             type == RD_BLE_SENSOR_DOOR ||
             type == RD_BLE_SENSOR_DOOR_SMALL ||
             type == RD_BLE_SENSOR_DOOR_LARGE)
    {
      // Sensor
      data[8] = 0xE0;
      data[9] = 0x11;
      data[10] = 0x02;
      data[13] = 0x50;
      data[15] = (uint8_t)(sceneId >> 8);
      data[16] = (uint8_t)(sceneId);

      // operator / value
      data[17] = data1;
    }
    else
    {
      // Default device
      data[8] = 0xE0;
      data[9] = 0x11;
      data[10] = 0x02;
      data[13] = 0x50;
      data[15] = (uint8_t)(sceneId >> 8);
      data[16] = (uint8_t)(sceneId);

      data[17] = data1;
      dataLen = 18;
    }
    uint8_t dataRsp[100] = {0};
    int lenRsp = 0;
    uint8_t compare[] = {0xE1, 0x11, 0x02, 0x50, 0x00};
    int result = SendMessage(APP_REQ, data, dataLen,
                             HCI_GATEWAY_RSP_UNICAST_DIR, dataRsp, &lenRsp,
                             3000, compare, 4, sizeof(compare));
    if (result != CODE_OK || lenRsp < 12 || dataRsp[9] != 0 ||
        dataRsp[10] != static_cast<uint8_t>(sceneId >> 8) ||
        dataRsp[11] != static_cast<uint8_t>(sceneId))
    {
      LOGW("Set condition scene failed: scene=0x%04X status=%d len=%d",
           sceneId, lenRsp > 9 ? dataRsp[9] : -1, lenRsp);
      return CODE_ERROR;
    }
    // Scene-capable devices need time to persist the entry before the next
    // scene command. hc_lux enforces the same 1000 ms minimum interval.
    SLEEP_MS(1000);
    return CODE_OK;
  }
  else if (mode == -3)
  {
    LOGD("Deleting condition device scene");
    uint8_t data[] = {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        static_cast<uint8_t>(devAddr),
        static_cast<uint8_t>(devAddr >> 8),
        0xE0,
        0x11,
        0x02,
        0x00,
        0x00,
        0x60,
        0x00,
        static_cast<uint8_t>(sceneId >> 8),
        static_cast<uint8_t>(sceneId),
    };

    if (type == "HG_BLE_IR")
    {
      // IR
      data[8] = 0xE4;
      data[9] = 0x11;
      data[10] = 0x02;
      data[13] = 0x06;
      data[14] = (uint8_t)(sceneId >> 8);
      data[15] = (uint8_t)(sceneId);

      // type for delete is always 3
      data[16] = (1 << 6);
      return SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 10);
    }
    else
    {
      int result =
          SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 10);
      if (result == CODE_OK)
      {
        SLEEP_MS(1000);
      }
      return result;
    }
  }
  else
  {
    LOGW("Invalid mode for SetConditionDeviceScene: %d", mode);
    return CODE_ERROR;
  }
  return CODE_ERROR;
}

int BleProtocol::SetIrDeviceActionScene(int mode, uint16_t devAddr,
                                        uint16_t sceneId, int commandType,
                                        int brandId, int remoteId, int temp,
                                        int irMode, int fan, int swing)
{
  if ((mode != -2 && mode != -3) ||
      (commandType != 2 && commandType != 3) || brandId <= 0 || remoteId <= 0)
  {
    LOGW("Invalid IR scene action: mode=%d command=%d brand=%d remote=%d",
         mode, commandType, brandId, remoteId);
    return CODE_FORMAT_ERROR;
  }

  uint8_t data[] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      static_cast<uint8_t>(devAddr), static_cast<uint8_t>(devAddr >> 8),
      0xE4, 0x11, 0x02, 0x00, 0x00, 0x06,
      static_cast<uint8_t>(sceneId >> 8), static_cast<uint8_t>(sceneId),
      0x00, static_cast<uint8_t>(brandId),
      static_cast<uint8_t>(brandId >> 8), 0x00, 0x00};

  data[16] = static_cast<uint8_t>((mode == -2 ? 2 : 1) << 6);
  data[16] |= static_cast<uint8_t>(1 << 1); // command index
  data[16] |= static_cast<uint8_t>(commandType - 2);
  data[19] = static_cast<uint8_t>((remoteId << 3) | (irMode & 0x07));

  if (mode == -2)
  {
    uint8_t packedState = static_cast<uint8_t>((temp - 16) << 4);
    packedState |= static_cast<uint8_t>((std::max(fan, 1) - 1) << 2);
    packedState |= static_cast<uint8_t>((std::max(swing, 1) - 1) / 2);
    data[20] = packedState;
    LOGI("Add IR scene action dev=0x%04X scene=0x%04X command=%d",
         devAddr, sceneId, commandType);
    return SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 10);
  }

  LOGI("Delete IR scene action dev=0x%04X scene=0x%04X command=%d",
       devAddr, sceneId, commandType);
  return SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 10);
}

int BleProtocol::SetDeviceActionScene(int mode, uint16_t devAddr,
                                      uint16_t sceneId, string type,
                                      uint8_t data1, uint8_t data2)
{
  if (mode == -3)
  {
    uint8_t data[] = {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        static_cast<uint8_t>(devAddr),
        static_cast<uint8_t>(devAddr >> 8),
        0x82,
        0x9E,
        static_cast<uint8_t>(sceneId >> 8),
        static_cast<uint8_t>(sceneId),
    };
    LOGI("DeleteDeviceActionScene dev=0x%04X scene=0x%04X",
         devAddr, sceneId);
    int result =
        SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 10);
    if (result == CODE_OK)
    {
      SLEEP_MS(1000);
    }
    return result;
  }
  if (mode != -2)
  {
    LOGW("Invalid mode for SetDeviceActionScene: %d", mode);
    return CODE_ERROR;
  }

  if (type == "HG_BLE_LIGHT_WHITE" || type == "HG_BLE_SWITCH_DIMMING")
  {
    uint8_t data[] = {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        static_cast<uint8_t>(devAddr),
        static_cast<uint8_t>(devAddr >> 8),
        0x82,
        0x46,
        static_cast<uint8_t>(sceneId >> 8),
        static_cast<uint8_t>(sceneId),
    };
    LOGI("Set light scene action dev=0x%04X scene=0x%04X type=%s",
         devAddr, sceneId, type.c_str());
    int result = SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr,
                             10);
    if (result == CODE_OK)
    {
      SLEEP_MS(1000);
    }
    return result;
  }

  if (type == "HG_BLE_LIGHT_RGB")
  {
    uint8_t blinkMode = data2 == 21 ? data1 : 0;
    uint8_t data[] = {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        static_cast<uint8_t>(devAddr),
        static_cast<uint8_t>(devAddr >> 8),
        0x82,
        0x46,
        static_cast<uint8_t>(sceneId >> 8),
        static_cast<uint8_t>(sceneId),
        blinkMode,
        0x00,
        0x00,
    };
    LOGI("Set RGB scene action dev=0x%04X scene=0x%04X blink=%u",
         devAddr, sceneId, blinkMode);
    int result = SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr,
                             10);
    if (result == CODE_OK)
    {
      SLEEP_MS(1000);
    }
    return result;
  }

  if (type == "HG_BLE_CURTAIN_NEW" || type == "HG_BLE_ROLLING_DOOR_NEW")
  {
    if (data2 != 1)
      return CODE_FORMAT_ERROR;
    // New curtain Scene Store carries both the open/stop/close value and
    // position. DP 1 controls the former; a DP 1-only action leaves position 0.
    uint8_t data[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        static_cast<uint8_t>(devAddr), static_cast<uint8_t>(devAddr >> 8),
        0x82, 0x46, static_cast<uint8_t>(sceneId >> 8),
        static_cast<uint8_t>(sceneId), data1, 0x00,
    };
    uint8_t dataRsp[100] = {0};
    int lenRsp = 0;
    uint8_t compare[] = {0x82, 0x45, 0x00,
                         static_cast<uint8_t>(sceneId >> 8),
                         static_cast<uint8_t>(sceneId)};
    int result = SendMessage(APP_REQ, data, sizeof(data),
                             HCI_GATEWAY_RSP_UNICAST_DIR, dataRsp, &lenRsp,
                             3000, compare, 4, sizeof(compare));
    if (result != CODE_OK || lenRsp < 9 || dataRsp[6] != 0 ||
        dataRsp[7] != static_cast<uint8_t>(sceneId >> 8) ||
        dataRsp[8] != static_cast<uint8_t>(sceneId))
      return CODE_ERROR;
    SLEEP_MS(1000);
    return CODE_OK;
  }

  // Scene Store: target element, opcode 0x8246, scene id, action value.
  uint8_t data[] = {
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      static_cast<uint8_t>(devAddr),
      static_cast<uint8_t>(devAddr >> 8),
      0x82,
      0x46,
      static_cast<uint8_t>(sceneId >> 8),
      static_cast<uint8_t>(sceneId),
      data1,
  };

  LOGI("SetDeviceActionScene dev=0x%04X scene=0x%04X value=%u",
       devAddr, sceneId, data1);
  uint8_t dataRsp[100] = {0};
  int lenRsp = 0;
  uint8_t compare[] = {
      0x82,
      0x45,
      0x00,
      static_cast<uint8_t>(sceneId >> 8),
      static_cast<uint8_t>(sceneId),
  };
  int result = SendMessage(APP_REQ, data, sizeof(data),
                           HCI_GATEWAY_RSP_UNICAST_DIR, dataRsp, &lenRsp,
                           3000, compare, 4, sizeof(compare));
  if (result != CODE_OK || lenRsp < 9 || dataRsp[6] != 0 ||
      dataRsp[7] != static_cast<uint8_t>(sceneId >> 8) ||
      dataRsp[8] != static_cast<uint8_t>(sceneId))
  {
    LOGW("Set action scene failed: scene=0x%04X status=%d len=%d",
         sceneId, lenRsp > 6 ? dataRsp[6] : -1, lenRsp);
    return CODE_ERROR;
  }
  SLEEP_MS(1000);
  return CODE_OK;
}

int BleProtocol::SetLocalSceneState(uint16_t sceneId, bool enabled)
{
  uint8_t data[] = {
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0xFF,
      0xFF,
      0xE0,
      0x11,
      0x02,
      0x00,
      0x00,
      0x70,
      0x00,
      static_cast<uint8_t>(sceneId >> 8),
      static_cast<uint8_t>(sceneId),
      static_cast<uint8_t>(enabled ? 1 : 0),
  };

  LOGI("%s local scene 0x%04X", enabled ? "Enable" : "Disable", sceneId);
  int result = SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 10);
  if (result == CODE_OK)
  {
    SLEEP_MS(1000);
  }
  return result;
}

int BleProtocol::SendRawData(uint8_t *data, int len)
{
  LOGD("SendRawData, dataLen: %d", len);
  Write(data, len); // +2 for devAddr prefix
  return CODE_OK;
}

int BleProtocol::SetLockKids(uint16_t devAddr, bool lock)
{
  uint8_t data[] = {
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      0x00,
      static_cast<uint8_t>(devAddr >> 8),
      static_cast<uint8_t>(devAddr),
      0xE0,
      0x11,
      0x02,
      0x00,
      0x00,
      0xC0,
      0x00,
      static_cast<uint8_t>(lock ? 1 : 0),
      0x00,
  };

  LOGI("SetLockKids addr=0x%04X lock=%d", devAddr, lock);
  return SendMessage(APP_REQ, data, sizeof(data), 0, nullptr, nullptr, 10);
}

//{0xe8,0xff,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00, 0xE0,0x11,0x02,0x00,0x00,  0x05,0x00,   0x00,   0x00};
int BleProtocol::SetLockDevice(uint16_t devAddr, bool lock)
{
  // Implementation for setting lock state of a specific device
  LOGD("SetLockDevice 0x%04x, lock %d", devAddr, lock);
  uint8_t dataRsp[100];
  int lenRsp;
  uint8_t SetLockHeader[] = {(uint8_t)(devAddr & 0xFF),
                             (uint8_t)((devAddr >> 8) & 0xFF),
                             1,
                             0,
                             HG_OPCODE_CONFIG_RSP,
                             0x11,
                             0x02};
  typedef struct __attribute__((packed))
  {
    ble_message_header_t ble_message_header;
    uint8_t opcodeVendor;
    uint16_t vendorId;
    uint8_t opcodeRsp;
    uint8_t tidPos;
    uint16_t header;
    uint8_t id;
  } set_lock_message_t;
  set_lock_message_t set_lock_message = {0};
  memset(&set_lock_message, 0x00, sizeof(set_lock_message));
  set_lock_message.ble_message_header.devAddr = devAddr;
  set_lock_message.opcodeVendor = HG_OPCODE_CONFIG;
  set_lock_message.vendorId = RD_VENDOR_ID;
  set_lock_message.opcodeRsp = HG_OPCODE_CONFIG_RSP;
  set_lock_message.header = HG_HEADER_0005;
  set_lock_message.id = lock ? 1 : 0;
  int rs =
      SendMessage(APP_REQ, (uint8_t *)&set_lock_message,
                  sizeof(set_lock_message_t), HCI_GATEWAY_RSP_OP_CODE,
                  dataRsp, &lenRsp, 2000, SetLockHeader, 0, 7);
  return rs == CODE_OK ? CODE_OK : CODE_ERROR;
}

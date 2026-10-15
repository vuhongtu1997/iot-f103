#include "DeviceModbus.h"

#include "protocol.h"

#include <cctype>
#include <utility>

namespace
{
  uint32_t ReadVersion(const Json::Value *dataJson, uint32_t fallback)
  {
    if (dataJson && dataJson->isObject())
    {
      const Json::Value &value = (*dataJson)["version"];
      if (value.isUInt64())
        return static_cast<uint32_t>(value.asUInt64());
      if (value.isUInt())
        return value.asUInt();
    }
    return fallback;
  }
} // namespace

DeviceModbus::DeviceModbus(std::string id, std::string name,
                           std::string uid, uint16_t address,
                           uint32_t version, uint8_t mode,
                           Json::Value *dataJson)
    : DeviceModbus(std::move(id), std::move(name), std::move(uid), address,
                   version, mode, MODBUS_SENSOR_UNKNOWN, TYPE, dataJson)
{
}

DeviceModbus::DeviceModbus(std::string id, std::string name,
                           std::string uid, uint16_t address,
                           uint32_t version, uint8_t mode,
                           uint8_t sensorType, Json::Value *dataJson)
    : DeviceModbus(std::move(id), std::move(name), std::move(uid), address,
                   version, mode, sensorType, TYPE, dataJson)
{
}

DeviceModbus::DeviceModbus(std::string id, std::string name,
                           std::string uid, uint16_t address,
                           uint32_t version, uint8_t mode,
                           uint8_t sensorType, const char *deviceType,
                           Json::Value *dataJson)
    : Device(std::move(id), std::move(name), std::move(uid), deviceType,
             address, static_cast<uint16_t>(version & 0xffff), dataJson)
{
  protocol = MODBUS_DEVICE;
  powerSource = POWER_AC;
  this->dataJson["uid"] = mac;
  this->dataJson["version"] = static_cast<Json::UInt64>(version);
  this->dataJson["mode"] = mode;
  this->dataJson["sensor_type"] = sensorType;
  this->dataJson["sensor_type_name"] = GetSensorName(sensorType);
}

DeviceModbus::DeviceModbus(std::string id, std::string name,
                           std::string uid, uint16_t address,
                           uint16_t version, Json::Value *dataJson)
    : Device(std::move(id), std::move(name), uid, TYPE, address, version,
             dataJson)
{
  protocol = MODBUS_DEVICE;
  powerSource = POWER_AC;
  this->dataJson["uid"] = uid;
  this->dataJson["version"] =
      static_cast<Json::UInt64>(ReadVersion(dataJson, version));
  if (!this->dataJson["mode"].isUInt())
    this->dataJson["mode"] = 0;
  if (!this->dataJson["sensor_type"].isUInt())
    this->dataJson["sensor_type"] = MODBUS_SENSOR_UNKNOWN;
  this->dataJson["sensor_type_name"] =
      GetSensorName(GetSensorType());
}

std::string DeviceModbus::GetUid()
{
  return GetMac();
}

std::string DeviceModbus::GetCloudId()
{
  return GetId();
}

bool DeviceModbus::HasUid()
{
  const std::string uid = GetUid();
  return uid.size() == 24;
}

uint32_t DeviceModbus::GetFirmwareVersion()
{
  const Json::Value data = GetData();
  if (data.isObject() && data["version"].isUInt64())
    return static_cast<uint32_t>(data["version"].asUInt64());
  if (data.isObject() && data["version"].isUInt())
    return data["version"].asUInt();
  return GetVersion();
}

uint8_t DeviceModbus::GetMode()
{
  const Json::Value data = GetData();
  return data.isObject() && data["mode"].isUInt()
             ? static_cast<uint8_t>(data["mode"].asUInt())
             : 0;
}

uint8_t DeviceModbus::GetSensorType()
{
  const Json::Value data = GetData();
  return data.isObject() && data["sensor_type"].isUInt()
             ? static_cast<uint8_t>(data["sensor_type"].asUInt())
             : MODBUS_SENSOR_UNKNOWN;
}

bool DeviceModbus::IsNode() const
{
  return type == TYPE;
}

const char *DeviceModbus::GetSensorName(uint8_t sensorType)
{
  switch (sensorType)
  {
  case MODBUS_SENSOR_GAS:
    return "gas";
  case MODBUS_SENSOR_TEMPERATURE_HUMIDITY:
    return "temperature_humidity";
  case MODBUS_SENSOR_CH3:
    return "ch3";
  default:
    return "unknown";
  }
}

const char *DeviceModbus::GetSensorDeviceType(uint8_t sensorType)
{
  switch (sensorType)
  {
  case MODBUS_SENSOR_GAS:
    return GAS_TYPE;
  case MODBUS_SENSOR_TEMPERATURE_HUMIDITY:
    return TEMPERATURE_HUMIDITY_TYPE;
  case MODBUS_SENSOR_CH3:
    return CH3_TYPE;
  default:
    return nullptr;
  }
}

const char *DeviceModbus::GetSensorIdSuffix(uint8_t sensorType)
{
  switch (sensorType)
  {
  case MODBUS_SENSOR_GAS:
    return "gas";
  case MODBUS_SENSOR_TEMPERATURE_HUMIDITY:
    return "temperature-humidity";
  case MODBUS_SENSOR_CH3:
    return "ch3";
  default:
    return nullptr;
  }
}

std::string DeviceModbus::GetSensorDeviceId(const std::string &parentId,
                                            uint8_t sensorType)
{
  const char *suffix = GetSensorIdSuffix(sensorType);
  return suffix ? parentId + "-" + suffix : "";
}

bool DeviceModbus::CopyUid(uint8_t uid[12])
{
  if (!uid)
    return false;
  const std::string hexUid = GetUid();
  if (hexUid.size() != 24)
    return false;

  for (size_t i = 0; i < 12; ++i)
  {
    const unsigned char high = static_cast<unsigned char>(hexUid[2 * i]);
    const unsigned char low = static_cast<unsigned char>(hexUid[2 * i + 1]);
    if (!std::isxdigit(high) || !std::isxdigit(low))
      return false;
    auto nibble = [](unsigned char ch) -> uint8_t
    {
      if (ch >= '0' && ch <= '9')
        return static_cast<uint8_t>(ch - '0');
      ch = static_cast<unsigned char>(std::tolower(ch));
      return static_cast<uint8_t>(ch - 'a' + 10);
    };
    uid[i] = static_cast<uint8_t>((nibble(high) << 4) | nibble(low));
  }
  return true;
}

void DeviceModbus::UpdateNode(uint32_t version, uint8_t mode, bool online,
                              uint16_t adcRaw, uint16_t millivolts)
{
  dataJson["uid"] = GetUid();
  dataJson["version"] = static_cast<Json::UInt64>(version);
  dataJson["mode"] = mode;
  dataJson["stt"] = online ? 1 : 0;
  if (online)
  {
    dataJson["adc_raw"] = adcRaw;
    dataJson["millivolts"] = millivolts;
    dataJson["quality"] =
        mode != 0 ? "bootloader" : adcRaw == 0xffff ? "sensor_error"
                                                    : "ok";
  }
  else
  {
    dataJson["quality"] = "offline";
  }
  lastOnlineState = online;
}

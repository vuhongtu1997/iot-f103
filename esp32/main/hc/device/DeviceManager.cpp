#include "DeviceManager.h"

#include "DeviceModbus.h"
#include "DeviceModbusGas.h"
#include "DeviceModbusTemperatureHumidity.h"
#include "DeviceModbusCH3.h"
#include "DeviceModbusPressure.h"
#include "Log.h"

#include <algorithm>
#include <cctype>
#include <mutex>

namespace
{
  std::mutex deviceListMutex;

  void NormalizeLower(std::string &value)
  {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch)
                   { return static_cast<char>(std::tolower(ch)); });
  }

  Device *CreateDevice(std::string &id, std::string &mac, std::string &type,
                       std::string &name, uint16_t address, uint16_t version,
                       Json::Value *dataJson)
  {
    std::string uid = mac;
    uint32_t firmwareVersion = version;
    uint8_t mode = 0;
    uint8_t sensorType = 0;
    if (dataJson && dataJson->isObject())
    {
      if ((*dataJson)["uid"].isString() &&
          !(*dataJson)["uid"].asString().empty())
        uid = (*dataJson)["uid"].asString();
      if ((*dataJson)["version"].isUInt64())
        firmwareVersion =
            static_cast<uint32_t>((*dataJson)["version"].asUInt64());
      else if ((*dataJson)["version"].isUInt())
        firmwareVersion = (*dataJson)["version"].asUInt();
      if ((*dataJson)["mode"].isUInt())
        mode = static_cast<uint8_t>((*dataJson)["mode"].asUInt());
      if ((*dataJson)["sensor_type"].isUInt())
        sensorType =
            static_cast<uint8_t>((*dataJson)["sensor_type"].asUInt());
    }

    if (type == DeviceModbus::TYPE)
      return new DeviceModbus(id, name, uid, address, firmwareVersion, mode,
                              sensorType, dataJson);
    if (type == DeviceModbus::GAS_TYPE)
      return new DeviceModbusGas(id, name, uid, address, firmwareVersion,
                                 sensorType, dataJson);
    if (type == DeviceModbus::TEMPERATURE_HUMIDITY_TYPE)
      return new DeviceModbusTemperatureHumidity(
          id, name, uid, address, firmwareVersion, sensorType, dataJson);
    if (type == DeviceModbus::CH3_TYPE)
      return new DeviceModbusCH3(id, name, uid, address, firmwareVersion,
                                 sensorType, dataJson);
    if (type == DeviceModbus::PRESSURE_TYPE)
      return new DeviceModbusPressure(id, name, uid, address, firmwareVersion,
                                     sensorType, dataJson);
    return nullptr;
  }
} // namespace

DeviceManager *DeviceManager::getInstance()
{
  static DeviceManager *deviceManager = nullptr;
  if (!deviceManager)
    deviceManager = new DeviceManager();
  return deviceManager;
}

DeviceManager::DeviceManager() {}

void DeviceManager::ForEach(function<void(Device *)> func)
{
  std::lock_guard<std::mutex> lock(deviceListMutex);
  for (auto &[id, device] : deviceList)
    func(device);
}

Device *DeviceManager::AddDevice(std::string &mac, std::string &type,
                                 string &name, uint16_t addr,
                                 uint16_t version, Json::Value *dataJson)
{
  NormalizeLower(mac);
  Device *oldDevice = GetDeviceFromId(mac);
  if (oldDevice)
    DelDevice(oldDevice);

  Device *device = CreateDevice(mac, mac, type, name, addr, version, dataJson);
  if (!device)
  {
    LOGW("Device type %s not supported for mac %s address 0x%04X",
         type.c_str(), mac.c_str(), addr);
    return nullptr;
  }

  std::lock_guard<std::mutex> lock(deviceListMutex);
  auto existing = deviceList.find(device->GetId());
  if (existing != deviceList.end())
  {
    delete existing->second;
    existing->second = device;
  }
  else
    deviceList.emplace(device->GetId(), device);
  return device;
}

Device *DeviceManager::AddDeviceNoReset(std::string &mac, std::string &type,
                                        string &name, uint16_t addr,
                                        uint16_t version,
                                        Json::Value *dataJson)
{
  NormalizeLower(mac);
  Device *device = CreateDevice(mac, mac, type, name, addr, version, dataJson);
  if (!device)
  {
    LOGW("Device type %s not supported for mac %s address 0x%04X",
         type.c_str(), mac.c_str(), addr);
    return nullptr;
  }

  std::lock_guard<std::mutex> lock(deviceListMutex);
  auto existing = deviceList.find(device->GetId());
  if (existing != deviceList.end())
  {
    delete existing->second;
    existing->second = device;
  }
  else
    deviceList.emplace(device->GetId(), device);
  return device;
}

Device *DeviceManager::AddDeviceNoResetWithId(std::string id, std::string mac,
                                              std::string &type, string &name,
                                              uint16_t addr, uint16_t version,
                                              Json::Value *dataJson)
{
  NormalizeLower(id);
  NormalizeLower(mac);
  Device *device = CreateDevice(id, mac, type, name, addr, version, dataJson);
  if (!device)
  {
    LOGW("Device type %s not supported for id %s address 0x%04X",
         type.c_str(), id.c_str(), addr);
    return nullptr;
  }

  std::lock_guard<std::mutex> lock(deviceListMutex);
  auto existing = deviceList.find(id);
  if (existing != deviceList.end())
  {
    delete existing->second;
    existing->second = device;
  }
  else
    deviceList.emplace(device->GetId(), device);
  return device;
}

Device *DeviceManager::GetDeviceFromId(std::string id)
{
  NormalizeLower(id);
  std::lock_guard<std::mutex> lock(deviceListMutex);
  const auto it = deviceList.find(id);
  return it == deviceList.end() ? nullptr : it->second;
}

Device *DeviceManager::GetDeviceFromCloudId(std::string cloudId)
{
  NormalizeLower(cloudId);
  if (cloudId.empty())
    return nullptr;
  std::lock_guard<std::mutex> lock(deviceListMutex);
  for (const auto &[id, device] : deviceList)
  {
    if (device->GetCloudId() == cloudId)
      return device;
  }
  return nullptr;
}

Device *DeviceManager::GetDeviceFromMac(std::string mac)
{
  NormalizeLower(mac);
  std::lock_guard<std::mutex> lock(deviceListMutex);
  for (const auto &[id, device] : deviceList)
  {
    std::string deviceMac = device->GetMac();
    NormalizeLower(deviceMac);
    if (deviceMac == mac)
      return device;
  }
  return nullptr;
}

vector<DeviceModbus *> DeviceManager::GetListDeviceModbus()
{
  vector<DeviceModbus *> nodes;
  std::lock_guard<std::mutex> lock(deviceListMutex);
  for (const auto &[id, device] : deviceList)
  {
    if (device->GetProtocol() == MODBUS_DEVICE)
    {
      auto *modbusDevice = static_cast<DeviceModbus *>(device);
      if (modbusDevice->IsNode())
        nodes.push_back(modbusDevice);
    }
  }
  return nodes;
}

vector<DeviceModbus *> DeviceManager::GetListDeviceModbusSensors()
{
  vector<DeviceModbus *> sensors;
  std::lock_guard<std::mutex> lock(deviceListMutex);
  for (const auto &[id, device] : deviceList)
  {
    if (device->GetProtocol() == MODBUS_DEVICE)
    {
      auto *modbusDevice = static_cast<DeviceModbus *>(device);
      if (!modbusDevice->IsNode())
        sensors.push_back(modbusDevice);
    }
  }
  return sensors;
}

void DeviceManager::DelDevice(Device *device)
{
  if (!device)
    return;
  std::lock_guard<std::mutex> lock(deviceListMutex);
  const auto it = deviceList.find(device->GetId());
  if (it != deviceList.end() && it->second == device)
    deviceList.erase(it);
  delete device;
}

void DeviceManager::DetachDevice(Device *device)
{
  if (!device)
    return;
  std::lock_guard<std::mutex> lock(deviceListMutex);
  const auto it = deviceList.find(device->GetId());
  if (it != deviceList.end() && it->second == device)
    deviceList.erase(it);
}

void DeviceManager::DelAllDevice()
{
  std::lock_guard<std::mutex> lock(deviceListMutex);
  for (auto &[id, device] : deviceList)
    delete device;
  deviceList.clear();
}

int DeviceManager::GetDeviceCount()
{
  std::lock_guard<std::mutex> lock(deviceListMutex);
  return static_cast<int>(deviceList.size());
}

#pragma once

#include "Device.h"

#include <stdint.h>
#include <string>

class DeviceModbus : public Device
{
public:
  static constexpr const char *TYPE = "MODBUS_NODE";

  DeviceModbus(std::string id, std::string name, std::string uid,
               uint16_t address, uint32_t version, uint8_t mode,
               Json::Value *dataJson = nullptr);
  DeviceModbus(std::string id, std::string name, std::string uid,
               uint16_t address, uint16_t version, Json::Value *dataJson);

  std::string GetUid();
  std::string GetCloudId() override;
  bool HasUid();
  uint32_t GetFirmwareVersion();
  uint8_t GetMode();
  bool CopyUid(uint8_t uid[12]);
  void UpdateNode(uint32_t version, uint8_t mode, bool online,
                  uint16_t adcRaw = 0xffff, uint16_t millivolts = 0xffff);
};

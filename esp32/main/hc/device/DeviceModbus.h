#pragma once

#include "Device.h"

#include <stdint.h>
#include <string>

class DeviceModbus : public Device
{
public:
  static constexpr const char *TYPE = "MODBUS_NODE";
  static constexpr const char *GAS_TYPE = "MODBUS_GAS";
  static constexpr const char *TEMPERATURE_HUMIDITY_TYPE =
      "MODBUS_TEMPERATURE_HUMIDITY";
  static constexpr const char *CH3_TYPE = "MODBUS_CH3";
  static constexpr const char *PRESSURE_TYPE = "MODBUS_PRESSURE";

  DeviceModbus(std::string id, std::string name, std::string uid,
               uint16_t address, uint32_t version, uint8_t mode,
               Json::Value *dataJson = nullptr);
  DeviceModbus(std::string id, std::string name, std::string uid,
               uint16_t address, uint32_t version, uint8_t mode,
               uint8_t sensorType, Json::Value *dataJson);
  DeviceModbus(std::string id, std::string name, std::string uid,
               uint16_t address, uint16_t version, Json::Value *dataJson);

  std::string GetUid();
  std::string GetCloudId() override;
  bool HasUid();
  uint32_t GetFirmwareVersion();
  uint8_t GetMode();
  uint8_t GetSensorType();
  bool IsNode() const;
  bool CopyUid(uint8_t uid[12]);
  static const char *GetSensorName(uint8_t sensorType);
  static const char *GetSensorDeviceType(uint8_t sensorType);
  static const char *GetSensorIdSuffix(uint8_t sensorType);
  static std::string GetSensorDeviceId(const std::string &parentId,
                                       uint8_t sensorType);
  void UpdateNode(uint32_t version, uint8_t mode, bool online,
                  uint16_t adcRaw = 0xffff, uint16_t millivolts = 0xffff);

protected:
  DeviceModbus(std::string id, std::string name, std::string uid,
               uint16_t address, uint32_t version, uint8_t mode,
               uint8_t sensorType, const char *deviceType,
               Json::Value *dataJson);
};

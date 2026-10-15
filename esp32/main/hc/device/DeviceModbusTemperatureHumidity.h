#pragma once

#include "DeviceModbus.h"

class DeviceModbusTemperatureHumidity final : public DeviceModbus
{
public:
  DeviceModbusTemperatureHumidity(std::string id, std::string name,
                                  std::string uid, uint16_t address,
                                  uint32_t version, uint8_t sensorType,
                                  Json::Value *dataJson);
};

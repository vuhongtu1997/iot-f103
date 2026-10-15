#pragma once

#include "DeviceModbus.h"

class DeviceModbusCH3 final : public DeviceModbus
{
public:
  DeviceModbusCH3(std::string id, std::string name, std::string uid,
                  uint16_t address, uint32_t version, uint8_t sensorType,
                  Json::Value *dataJson);
};

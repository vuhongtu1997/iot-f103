#pragma once
#include "DeviceModbus.h"

class DeviceModbusPressure : public DeviceModbus
{
public:
  DeviceModbusPressure(std::string id, std::string name, std::string uid,
                       uint16_t address, uint32_t version, uint8_t sensorType,
                       Json::Value *dataJson)
      : DeviceModbus(id, name, uid, address, version, 0, sensorType,
                     PRESSURE_TYPE, dataJson) {}
};

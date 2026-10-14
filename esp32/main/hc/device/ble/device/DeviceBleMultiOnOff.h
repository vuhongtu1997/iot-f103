#pragma once

#include "DeviceBle.h"
#include "module/ModuleOnOff.h"

class DeviceBleMultiOnOff : public DeviceBle
{
private:
  int lastCurtainTime;

public:
  DeviceBleMultiOnOff(string id, string name, string mac, string type,
                      uint16_t addr, uint16_t version,
                      Json::Value *dataJson, uint8_t elementCount);
  int Do(Json::Value &dataValue) override;
};

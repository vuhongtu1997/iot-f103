#pragma once

#include "DeviceBle.h"
#include "module/ModuleCurtain.h"
#include "module/ModuleRawData.h"

class DeviceBleCurtain : public DeviceBle
{
private:
  ModuleCurtain *moduleCurtain;
  ModuleRawData *rawDataModule;

public:
  DeviceBleCurtain(string id, string name, string mac, string type,
                   uint16_t addr, uint16_t version, Json::Value *dataJson);
  int Do(Json::Value &dataValue) override;
};

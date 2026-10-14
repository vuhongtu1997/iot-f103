#pragma once

#include "DeviceBle.h"
#include "module/ModuleCallScene.h"
#include "module/ModuleCct.h"
#include "module/ModuleDim.h"
#include "module/ModuleHsl.h"
#include "module/ModuleLightOnOff.h"
#include "module/ModuleModeRgb.h"

class DeviceBleLightRgb : public DeviceBle
{
private:
  ModuleLightOnOff *moduleOnOff;
  ModuleDim *moduleDim;
  ModuleModeRgb *moduleModeRgb;
  ModuleHsl *moduleHsl;
  ModuleCallScene *moduleCallScene;
  ModuleCct *moduleCct;

public:
  DeviceBleLightRgb(string id, string name, string mac, string type,
                    uint16_t addr, uint16_t version,
                    Json::Value *dataJson);
};

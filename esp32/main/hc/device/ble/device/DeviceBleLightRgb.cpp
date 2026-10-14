#include "DeviceBleLightRgb.h"

DeviceBleLightRgb::DeviceBleLightRgb(string id, string name, string mac,
                                     string type, uint16_t addr,
                                     uint16_t version,
                                     Json::Value *dataJson)
    : DeviceBle(id, name, mac, type, addr, version, dataJson)
{
  moduleOnOff = new ModuleLightOnOff(this, addr, KEY_ATTRIBUTE_ONOFF);
  modules.push_back(moduleOnOff);
  moduleDim = new ModuleDim(this, addr);
  modules.push_back(moduleDim);
  moduleModeRgb = new ModuleModeRgb(this, addr);
  modules.push_back(moduleModeRgb);
  moduleHsl = new ModuleHsl(this, addr);
  modules.push_back(moduleHsl);
  moduleCallScene = new ModuleCallScene(this, addr);
  modules.push_back(moduleCallScene);
  moduleCct = new ModuleCct(this, addr + 1);
  modules.push_back(moduleCct);

  countElement = 2;
  powerSource = POWER_AC;
}

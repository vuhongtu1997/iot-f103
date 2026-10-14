#include "DeviceBleCurtain.h"
#include "Log.h"

DeviceBleCurtain::DeviceBleCurtain(string id, string name, string mac,
                                   string type, uint16_t addr,
                                   uint16_t version, Json::Value *dataJson)
    : DeviceBle(id, name, mac, type, addr, version, dataJson)
{
  countElement = 1;
  moduleCurtain = new ModuleCurtain(this, addr);
  rawDataModule = new ModuleRawData(this, addr);
  modules.push_back(moduleCurtain);
  modules.push_back(rawDataModule);
  powerSource = POWER_AC;
}

int DeviceBleCurtain::Do(Json::Value &dataValue)
{
  if (dataValue.isObject() &&
      dataValue.isMember(KEY_ATTRIBUTE_CURTAINS_TIME))
  {
    LOGI("DeviceBleCurtain dispatch curtainsTime: id=%s type=%s addr=0x%04X "
         "value=%s",
         id.c_str(), type.c_str(), addr,
         dataValue[KEY_ATTRIBUTE_CURTAINS_TIME].toString().c_str());
    return moduleCurtain->Do(dataValue);
  }
  if (dataValue.isObject() && dataValue.isMember("106"))
  {
    LOGI("DeviceBleCurtain dispatch raw data: id=%s type=%s addr=0x%04X",
         id.c_str(), type.c_str(), addr);
    return rawDataModule->Do(dataValue);
  }

  return DeviceBle::Do(dataValue);
}

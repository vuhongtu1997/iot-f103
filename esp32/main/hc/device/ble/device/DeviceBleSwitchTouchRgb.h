#pragma once

#include "DeviceBle.h"
#include "module/ModuleOnOff.h"
#include "module/ModuleRgb.h"
#include "module/ModuleCountDownSwitch.h"
#include "module/ModuleStatusStartup.h"
#include "module/ModuleCallScene.h"
#include "module/ModuleButtonSeftPowerRemote.h"
#include "module/ModuleRawData.h"

using namespace std;

class DeviceBleSwitchTouchRgb : public DeviceBle
{
private:
	ModuleOnOff *moduleOnOff;
	ModuleOnOff *moduleOnOffAll;
	ModuleRgb *moduleRgb;
	vector<ModuleCountDownSwitch *> moduleCountDownSwitches;
	ModuleStatusStartup *moduleStatusStartup;
	ModuleCallScene *moduleCallScene;
	ModuleButtonSeftPowerRemote *moduleButtonSeftPowerRemote;
	ModuleRawData *moduleRawData;
public:
	DeviceBleSwitchTouchRgb(string id, string name, string mac, string type, uint16_t addr, uint16_t version, Json::Value *dataJson, uint8_t countElement);
	int Do(Json::Value &dataValue) override;
};

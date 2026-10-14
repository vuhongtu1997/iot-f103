#include "DeviceBleSwitchTouchRgb.h"
#include "Log.h"
#include <cstdlib>

DeviceBleSwitchTouchRgb::DeviceBleSwitchTouchRgb(string id, string name, string mac, string type, uint16_t addr, uint16_t version, Json::Value *dataJson, uint8_t countElement)
	: DeviceBle(id, name, mac, type, addr, version, dataJson)
{
	this->countElement = countElement;
	moduleButtonSeftPowerRemote = nullptr;
	for (int i = 0; i < countElement; i++)
	{
		uint16_t elementAddr = addr + i;
		moduleOnOff = new ModuleOnOff(this, elementAddr, KEY_ATTRIBUTE_ONOFF, i);
		modules.push_back(moduleOnOff);
		ModuleCountDownSwitch *countDownModule = new ModuleCountDownSwitch(this, elementAddr, i);
		moduleCountDownSwitches.push_back(countDownModule);
		modules.push_back(countDownModule);
	}
	moduleOnOffAll = new ModuleOnOff(this, addr, KEY_ATTRIBUTE_ONOFF);
	modules.push_back(moduleOnOffAll);
	moduleStatusStartup = new ModuleStatusStartup(this, addr);
	modules.push_back(moduleStatusStartup);
	moduleCallScene = new ModuleCallScene(this, addr);
	modules.push_back(moduleCallScene);
	moduleRawData = new ModuleRawData(this, addr);
	modules.push_back(moduleRawData);

	if (type == "HG_BLE_SWITCH_SCENE")
	{
		moduleButtonSeftPowerRemote = new ModuleButtonSeftPowerRemote(this, addr);
		modules.push_back(moduleButtonSeftPowerRemote);
	}

	powerSource = POWER_AC;
}

int DeviceBleSwitchTouchRgb::Do(Json::Value &dataValue)
{
	if (!dataValue.isObject() ||
		!dataValue.isMember(KEY_ATTRIBUTE_COUNTDOWN_TIMER))
		return DeviceBle::Do(dataValue);

	const Json::Value &timers = dataValue[KEY_ATTRIBUTE_COUNTDOWN_TIMER];
	if (!timers.isObject() || timers.empty())
		return CODE_ERROR;

	int requested = 0;
	int succeeded = 0;
	for (auto const &key : timers.getMemberNames())
	{
		if (timers[key].isNull())
			continue;

		char *end = nullptr;
		long button = strtol(key.c_str(), &end, 10);
		if (!end || *end != '\0' || button < 1 ||
			button > static_cast<long>(moduleCountDownSwitches.size()))
		{
			LOGW("Invalid countdown button %s for device %s",
				 key.c_str(), id.c_str());
			return CODE_ERROR;
		}

		requested++;
		if (moduleCountDownSwitches[button - 1]->Do(dataValue) == CODE_OK)
			succeeded++;
	}

	return requested > 0 && requested == succeeded ? CODE_OK : CODE_ERROR;
}

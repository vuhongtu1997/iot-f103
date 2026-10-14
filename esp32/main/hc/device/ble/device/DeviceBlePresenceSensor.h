#pragma once

#include "DeviceBle.h"
#include "module/ModulePresenceSensor.h"
#include "module/ModuleRawData.h"

class DeviceBlePresenceSensor : public DeviceBle
{
private:
	ModulePresenceSensor *modulePresenceSensor;
	ModuleRawData *moduleRawData;

public:
	DeviceBlePresenceSensor(string id, string name, string mac, string type,
							uint16_t addr, uint16_t version,
							Json::Value *dataJson);
};

#include "DeviceBlePresenceSensor.h"

DeviceBlePresenceSensor::DeviceBlePresenceSensor(string id, string name,
												 string mac, string type,
												 uint16_t addr,
												 uint16_t version,
												 Json::Value *dataJson)
	: DeviceBle(id, name, mac, type, addr, version, dataJson)
{
	countElement = 1;

	modulePresenceSensor = new ModulePresenceSensor(this, addr);
	modules.push_back(modulePresenceSensor);

	moduleRawData = new ModuleRawData(this, addr);
	modules.push_back(moduleRawData);

	powerSource = POWER_AC;
}

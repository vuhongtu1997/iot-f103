#include "DeviceBleSmokeSensor.h"

DeviceBleSmokeSensor::DeviceBleSmokeSensor(string id, string name, string mac,
										   string type, uint16_t addr,
										   uint16_t version,
										   Json::Value *dataJson)
	: DeviceBle(id, name, mac, type, addr, version, dataJson)
{
	countElement = 1;

	moduleSmokeSensor = new ModuleSmokeSensor(this, addr);
	modules.push_back(moduleSmokeSensor);

	powerSource = POWER_BATTERY;
}

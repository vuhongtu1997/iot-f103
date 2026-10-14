#include "DeviceBleDoorSensor.h"

DeviceBleDoorSensor::DeviceBleDoorSensor(string id, string name, string mac,
										 string type, uint16_t addr,
										 uint16_t version,
										 Json::Value *dataJson)
	: DeviceBle(id, name, mac, type, addr, version, dataJson)
{
	countElement = 1;

	moduleDoorSensor = new ModuleDoorSensor(this, addr);
	modules.push_back(moduleDoorSensor);

	if (dataJson && dataJson->isObject() && dataJson->isMember("dictDPs"))
	{
		Json::Value initial;
		moduleDoorSensor->InputData((*dataJson)["dictDPs"], initial);
	}

	powerSource = POWER_BATTERY;
}

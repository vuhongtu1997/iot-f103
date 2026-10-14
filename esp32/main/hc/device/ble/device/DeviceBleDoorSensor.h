#pragma once

#include "DeviceBle.h"
#include "module/ModuleDoorSensor.h"

class DeviceBleDoorSensor : public DeviceBle
{
private:
	ModuleDoorSensor *moduleDoorSensor;

public:
	DeviceBleDoorSensor(string id, string name, string mac, string type,
						uint16_t addr, uint16_t version, Json::Value *dataJson);
};

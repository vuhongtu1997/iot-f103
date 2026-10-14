#pragma once

#include "DeviceBle.h"
#include "module/ModuleSmokeSensor.h"

class DeviceBleSmokeSensor : public DeviceBle
{
private:
	ModuleSmokeSensor *moduleSmokeSensor;

public:
	DeviceBleSmokeSensor(string id, string name, string mac, string type,
						 uint16_t addr, uint16_t version, Json::Value *dataJson);
};

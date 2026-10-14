#pragma once

#include "DeviceBle.h"
#include "module/ModuleCct.h"
#include "module/ModuleDim.h"
#include "module/ModuleLightOnOff.h"

using namespace std;

class DeviceBleLightWhite : public DeviceBle
{
private:
	ModuleLightOnOff *moduleOnOff;
	ModuleDim *moduleDim;
	ModuleCct *moduleCct;

public:
	DeviceBleLightWhite(string id, string name, string mac, string type,
						uint16_t addr, uint16_t version,
						Json::Value *dataJson);
	int Do(Json::Value &dataValue) override;
	bool HandleBleStatus(uint16_t opcode, uint8_t *data, int len,
						 uint16_t resolvedAddr) override;
};

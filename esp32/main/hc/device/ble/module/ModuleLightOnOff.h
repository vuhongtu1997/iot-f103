#pragma once

#include "ModuleOnOff.h"

using namespace std;

class ModuleLightOnOff : public ModuleOnOff
{
public:
	ModuleLightOnOff(Device *device, uint16_t addr, string onoffKey = "20");

	int InputData(Json::Value &dataValue, Json::Value &jsonValue) override;
	int InputData(uint8_t *data, int len, Json::Value &jsonValue) override;
	bool CheckData(Json::Value &dataValue, bool &rs) override;
	void BuildTelemetryValue(Json::Value &jsonValue) override;
	int Do(Json::Value &dataValue) override;
};

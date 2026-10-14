#pragma once

#include "Module.h"

class ModuleSmokeSensor : public Module
{
protected:
	uint8_t smoke;
	uint8_t battery;

public:
	ModuleSmokeSensor(Device *device, uint16_t addr);
	~ModuleSmokeSensor();

#ifdef CONFIG_SAVE_ATTRIBUTE
	void InitAttribute(string attribute, double value);
	void SaveAttribute(const string &attribute);
#endif

	int InputData(Json::Value &dataValue, Json::Value &jsonValue);
	int InputData(uint8_t *data, int len, Json::Value &jsonValue);
	bool CheckData(Json::Value &dataValue, bool &rs);
	void BuildTelemetryValue(Json::Value &jsonValue);
};

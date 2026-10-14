#pragma once

#include "Module.h"

class ModulePresenceSensor : public Module
{
protected:
	uint8_t active;
	uint8_t type;
	uint16_t lux;

public:
	ModulePresenceSensor(Device *device, uint16_t addr);
	~ModulePresenceSensor();

#ifdef CONFIG_SAVE_ATTRIBUTE
	void InitAttribute(string attribute, double value);
	void SaveAttribute(const string &attribute);
#endif

	int InputData(Json::Value &dataValue, Json::Value &jsonValue);
	int InputData(uint8_t *data, int len, Json::Value &jsonValue);
	bool CheckData(Json::Value &dataValue, bool &rs);
	void BuildTelemetryValue(Json::Value &jsonValue);
};

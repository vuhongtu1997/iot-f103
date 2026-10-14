#pragma once
#include "Module.h"

using namespace std;

class ModulePirLightSensorStartup : public Module
{
protected:
	uint16_t pir;
	uint16_t lux;
	int idPir;
	int idLux;

public:
	ModulePirLightSensorStartup(Device *device, uint32_t addr);
	~ModulePirLightSensorStartup();

	int InputData(uint8_t *data, int len, Json::Value &jsonValue);

	/**
	 * @brief Check rule input
	 *
	 * @param dataValue json rule data input
	 * @param rs result of checking
	 * @return true if dataValue uses this module paramter
	 * @return false if dataValue don't use this module paramter
	 */
	void BuildTelemetryValue(Json::Value &jsonValue);
};

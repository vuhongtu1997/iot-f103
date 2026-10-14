#pragma once
#include "Module.h"

using namespace std;

class ModuleRgb : public Module
{
protected:
	uint8_t r, g, b, dimOn, dimOff;
	string keyR, keyG, keyB, keyDimOn, keyDimOff;

public:
	ModuleRgb(Device *device, uint16_t addr, uint32_t index = 0);
	~ModuleRgb();

#ifdef CONFIG_SAVE_ATTRIBUTE
	/**
	 * @brief Init parameter value from database after system start
	 *
	 * @param attribute id of attribute
	 * @param value value of attribute
	 */
	void InitAttribute(string attribute, double value);

	/**
	 * @brief Save parameter value to database
	 *
	 */
	void SaveAttribute(string key);
#endif

	int InputData(Json::Value &dataValue, Json::Value &jsonValue);

	/**
	 * @brief Parse raw data to element parameter value
	 *
	 * @param data data from device driver (uart)
	 * @param len length of data
	 * @param jsonValue json value to put parameter after parsing
	 * @return true if data include this element opcode
	 * @return false
	 */
	int InputData(uint8_t *data, int len, Json::Value &jsonValue);

	/**
	 * @brief Check rule input
	 *
	 * @param dataValue json rule data input
	 * @param rs result of checking
	 * @return true if dataValue uses this element paramter
	 * @return false if dataValue don't use this element paramter
	 */
	bool CheckData(Json::Value &dataValue, bool &rs);

	/**
	 * @brief Build telemetry message with this element
	 *
	 * @param jsonValue
	 */
	void BuildTelemetryValue(Json::Value &jsonValue);

	/**
	 * @brief Do an action
	 *
	 * @param dataValue data of action
	 * @return true
	 * @return false
	 */
	int Do(Json::Value &dataValue);
};

#pragma once
#include "Module.h"

using namespace std;

class ModuleHsl : public Module
{
protected:
	uint16_t h, s, l;

public:
	ModuleHsl(Device *device, uint16_t addr);
	~ModuleHsl();

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
	 * @brief Parse raw data to module parameter value
	 *
	 * @param data data from device driver (uart)
	 * @param len length of data
	 * @param jsonValue json value to put parameter after parsing
	 * @return true if data include this module opcode
	 * @return false
	 */
	int InputData(uint8_t *data, int len, Json::Value &jsonValue);

	/**
	 * @brief Check rule input
	 *
	 * @param dataValue json rule data input
	 * @param rs result of checking
	 * @return true if dataValue uses this module paramter
	 * @return false if dataValue don't use this module paramter
	 */
	bool CheckData(Json::Value &dataValue, bool &rs);

	/**
	 * @brief Build telemetry message with this module
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
	int DoJsonArray(Json::Value &dataValue);

	/**
	 * @brief Do an action use message format version 2
	 *
	 * @param dataValue data of action
	 * @return true
	 * @return false
	 */
	int Do(Json::Value &dataValue);
};

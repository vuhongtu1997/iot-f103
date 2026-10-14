#pragma once

#include <string>
#include "json.h"
#include "ErrorCode.h"

using namespace std;

class Device;
class Module
{
protected:
	uint16_t addr;
	uint32_t index;
	Device *device;

public:
	Module(Device *device, uint16_t addr, uint32_t index = 0);
	virtual ~Module();

	bool CheckAddr(uint16_t addr);

	/**
	 * @brief Parse raw data to element parameter value
	 *
	 * @param dataValue json data input
	 * @param jsonValue json value to put parameter after parsing
	 * @return true if data include this element opcode
	 * @return false
	 */
	virtual int InputData(Json::Value &dataValue, Json::Value &jsonValue) { return CODE_ERROR; }

	/**
	 * @brief Parse raw data to element parameter value
	 *
	 * @param data data from device driver (uart)
	 * @param len length of data
	 * @param jsonValue json value to put parameter after parsing
	 * @return true if data include this element opcode
	 * @return false
	 */
	virtual int InputData(uint8_t *data, int len, Json::Value &jsonValue) { return CODE_ERROR; }

	/**
	 * @brief Check rules related with this element
	 *
	 */
	virtual void CheckTrigger(Json::Value &data);

	virtual bool CheckData(Json::Value &dataValue, bool &rs);

	/**
	 * @brief Build telemetry message with this module
	 *
	 * @param jsonValue
	 */
	virtual void BuildTelemetryValue(Json::Value &jsonValue) {}

	/**
	 * @brief Do an action
	 *
	 * @param dataValue data of action
	 * @return true
	 * @return false
	 */
	virtual int Do(Json::Value &dataValue) { return CODE_ERROR; }
	virtual void InitAttribute(string attribute, double value) {}
};

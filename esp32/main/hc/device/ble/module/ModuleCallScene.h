#pragma once

#include "Module.h"

using namespace std;

class ModuleCallScene : public Module
{
protected:
	uint16_t idScene;
	uint8_t id;
	uint16_t value;

public:
	ModuleCallScene(Device *device, uint16_t addr);
	~ModuleCallScene();

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
};

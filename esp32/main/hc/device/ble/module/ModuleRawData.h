#pragma once
#include "Module.h"

using namespace std;

class ModuleRawData : public Module
{
protected:
	string key;
public:
    ModuleRawData(Device *device, uint16_t addr);
    ~ModuleRawData();

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
    int Do(Json::Value &dataValue);
};

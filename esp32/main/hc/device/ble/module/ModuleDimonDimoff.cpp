#include "ModuleDimonDimoff.h"
#include "Log.h"
#include "Util.h"
#include "Device.h"
#include "BleProtocol.h"
#include "Database.h"

ModuleDimonDimoff::ModuleDimonDimoff(Device *device, uint16_t addr, uint32_t index) : Module(device, addr, index)
{
	dimOn = 0;
	dimOff = 0;
	keyDimOn = KEY_ATTRIBUTE_DIM_ON + (index ? to_string(index + 1) : "");
	keyDimOff = KEY_ATTRIBUTE_DIM_OFF + (index ? to_string(index + 1) : "");
}

ModuleDimonDimoff::~ModuleDimonDimoff()
{
}

#ifdef CONFIG_SAVE_ATTRIBUTE
void ModuleDimonDimoff::InitAttribute(string attribute, double value)
{
	if (attribute == keyDimOn)
	{
		dimOn = value;
	}
	else if (attribute == keyDimOff)
	{
		dimOff = value;
	}
}

void ModuleDimonDimoff::SaveAttribute(string key)
{
	if (key == keyDimOn)
		Database::getInstance()->DeviceAttributeAdd(device, keyDimOn, dimOn);
	else if (key == keyDimOff)
		Database::getInstance()->DeviceAttributeAdd(device, keyDimOff, dimOff);
}
#endif

int ModuleDimonDimoff::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
	if (dataValue.isObject() &&
		dataValue.isMember(keyDimOn) && dataValue[keyDimOn].isInt() &&
		dataValue.isMember(keyDimOff) && dataValue[keyDimOff].isInt())
	{
		dimOn = dataValue[keyDimOn].asInt();
		dimOff = dataValue[keyDimOff].asInt();
		BuildTelemetryValue(jsonValue);
		return CODE_OK;
	}
	return CODE_ERROR;
}

int ModuleDimonDimoff::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
	typedef struct __attribute__((packed))
	{
		uint8_t opcode;
		uint16_t vendorId;
		uint16_t header;
		uint8_t btn;
		uint8_t b;
		uint8_t g;
		uint8_t r;
		uint8_t dimOn;
		uint8_t dimOff;
	} data_message_t;
	data_message_t *data_message = (data_message_t *)data;
	if (data_message->opcode == RD_OPCODE_CONFIG_RSP && data_message->header == RD_HEADER_CONFIG_CONTROL_RGB_SWITCH)
	{
		int temp_dimOn, temp_dimOff;
		temp_dimOn = data_message->dimOn;
		temp_dimOff = data_message->dimOff;
		if (temp_dimOff != dimOff)
		{
			dimOff = temp_dimOff;
#ifdef CONFIG_SAVE_ATTRIBUTE
			SaveAttribute(keyDimOff);
#endif
		}
		if (temp_dimOn != dimOn)
		{
			dimOn = temp_dimOn;
#ifdef CONFIG_SAVE_ATTRIBUTE
			SaveAttribute(keyDimOn);
#endif
		}
		BuildTelemetryValue(jsonValue);
		CheckTrigger(jsonValue);
		return CODE_OK;
	}
	return CODE_ERROR;
}

bool ModuleDimonDimoff::CheckData(Json::Value &dataValue, bool &rs)
{
	LOGV("CheckData data: %s", dataValue.toString().c_str());
	if (dataValue.isObject() &&
		dataValue.isMember("op") && dataValue["op"].isString())
	{
		string op = dataValue["op"].asString();
		if (dataValue.isMember(keyDimOn))
		{
			if (dataValue[keyDimOn].isInt())
			{
				int dimOn = dataValue[keyDimOn].asInt();
				rs = Util::CompareNumber(op, this->dimOn, dimOn);
				return true;
			}
			else if (dataValue[keyDimOn].isArray())
			{
				Json::Value listValue = dataValue[keyDimOn];
				if (listValue.size() == 2 && listValue[0].isInt() && listValue[1].isInt())
				{
					int dimOn1 = listValue[0].asInt();
					int dimOn2 = listValue[1].asInt();
					rs = Util::CompareNumber(op, this->dimOn, dimOn1, dimOn2);
					return true;
				}
			}
		}
		else if (dataValue.isMember(keyDimOff))
		{
			if (dataValue[keyDimOff].isInt())
			{
				int dimOff = dataValue[keyDimOff].asInt();
				rs = Util::CompareNumber(op, this->dimOff, dimOff);
				return true;
			}
			else if (dataValue[keyDimOff].isArray())
			{
				Json::Value listValue = dataValue[keyDimOff];
				if (listValue.size() == 2 && listValue[0].isInt() && listValue[1].isInt())
				{
					int dimOff1 = listValue[0].asInt();
					int dimOff2 = listValue[1].asInt();
					rs = Util::CompareNumber(op, this->dimOff, dimOff1, dimOff2);
					return true;
				}
			}
		}
	}
	return false;
}

void ModuleDimonDimoff::BuildTelemetryValue(Json::Value &jsonValue)
{
	jsonValue[keyDimOn] = dimOn;
	jsonValue[keyDimOff] = dimOff;
}

int ModuleDimonDimoff::Do(Json::Value &dataValue)
{
	LOGV("Do data: %s", dataValue.toString().c_str());
	if (dataValue.isObject() &&
		dataValue.isMember(keyDimOn) && dataValue[keyDimOn].isInt() &&
		dataValue.isMember(keyDimOff) && dataValue[keyDimOff].isInt())
	{
		int dimOn = dataValue[keyDimOn].asInt();
		int dimOff = dataValue[keyDimOff].asInt();
		if (BleProtocol::getInstance()->ControlRgbSwitch(addr, index, 0, 0, 0, dimOn, dimOff) == CODE_OK)
		{
			return CODE_OK;
		}
	}
	return CODE_ERROR;
}

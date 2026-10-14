#include "ModuleStatusStartup.h"
#include "Log.h"
#include "Util.h"
#include "Device.h"
#include "BleProtocol.h"
#include "Database.h"

ModuleStatusStartup::ModuleStatusStartup(Device *device, uint32_t addr) : Module(device, addr)
{
	status = 0;
}

ModuleStatusStartup::~ModuleStatusStartup()
{
}

int ModuleStatusStartup::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
	if (dataValue.isObject() && dataValue.isMember(KEY_ATTRIBUTE_STATUS_STARTUP) && dataValue[KEY_ATTRIBUTE_STATUS_STARTUP].isInt())
	{
		status = dataValue[KEY_ATTRIBUTE_STATUS_STARTUP].asInt();
		BuildTelemetryValue(jsonValue);
		return CODE_OK;
	}
	return CODE_ERROR;
}

int ModuleStatusStartup::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
	typedef struct __attribute__((packed))
	{
		uint8_t opcode;
		uint16_t vendorId;
		uint16_t header;
		uint8_t status;
	} data_message_t;
	data_message_t *data_message = (data_message_t *)data;
	if (data_message->opcode == RD_OPCODE_CONFIG_RSP && data_message->header == RD_HEADER_CONFIG_STATUS_STARTUP_SWITCH)
	{
		status = data_message->status;
		BuildTelemetryValue(jsonValue);
		CheckTrigger(jsonValue);
		return CODE_OK;
	}
	return CODE_ERROR;
}

bool ModuleStatusStartup::CheckData(Json::Value &dataValue, bool &rs)
{
	LOGD("CheckData data: %s", dataValue.toString().c_str());

	if (dataValue.isObject() &&
		dataValue.isMember(KEY_ATTRIBUTE_STATUS_STARTUP) &&
		dataValue.isMember("op") && dataValue["op"].isString())
	{
		string op = dataValue["op"].asString();
		if (dataValue[KEY_ATTRIBUTE_STATUS_STARTUP].isInt())
		{
			int value = dataValue[KEY_ATTRIBUTE_STATUS_STARTUP].asInt();
			rs = Util::CompareNumber(op, this->status, value);
			return true;
		}
		else if (dataValue[KEY_ATTRIBUTE_STATUS_STARTUP].isArray())
		{
			Json::Value listValue = dataValue[KEY_ATTRIBUTE_STATUS_STARTUP];
			if (listValue.size() == 2 && listValue[0].isInt() && listValue[1].isInt())
			{
				int value1 = listValue[0].asInt();
				int value2 = listValue[1].asInt();
				rs = Util::CompareNumber(op, this->status, value1, value2);
				return true;
			}
		}
	}
	return false;
}

void ModuleStatusStartup::BuildTelemetryValue(Json::Value &jsonValue)
{
	jsonValue[KEY_ATTRIBUTE_STATUS_STARTUP] = status;
}

int ModuleStatusStartup::Do(Json::Value &dataValue)
{
	if (dataValue.isObject() &&
		dataValue.isMember(KEY_ATTRIBUTE_STATUS_STARTUP) && dataValue[KEY_ATTRIBUTE_STATUS_STARTUP].isInt())
	{
		int status = dataValue[KEY_ATTRIBUTE_STATUS_STARTUP].asInt();
		if (BleProtocol::getInstance()->ConfigStatusStartupSwitch(addr, status) == CODE_OK)
		{
			this->status = status;
			return CODE_OK;
		}
	}
	return CODE_ERROR;
}
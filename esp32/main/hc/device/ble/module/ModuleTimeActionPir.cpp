#include "ModuleTimeActionPir.h"
#include "Log.h"
#include "Util.h"
#include "Device.h"
#include "BleProtocol.h"
#include "Database.h"

ModuleTimeActionPir::ModuleTimeActionPir(Device *device, uint16_t addr) : Module(device, addr)
{
	time = 0;
}

ModuleTimeActionPir::~ModuleTimeActionPir()
{
}

int ModuleTimeActionPir::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
	if (dataValue.isObject() &&
			dataValue.isMember(KEY_ATTRIBUTE_ACTIME) && dataValue[KEY_ATTRIBUTE_ACTIME].isInt())
	{
		time = dataValue[KEY_ATTRIBUTE_ACTIME].asInt();
		BuildTelemetryValue(jsonValue);
		return CODE_OK;
	}
	return CODE_ERROR;
}

int ModuleTimeActionPir::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
	typedef struct __attribute__((packed))
	{
		uint8_t opcode;
		uint16_t vendorId;
		uint16_t header;
		uint16_t time;
	} data_message_t;
	data_message_t *data_message = (data_message_t *)data;
	if (data_message->opcode == RD_OPCODE_CONFIG_RSP &&
		data_message->vendorId == RD_VENDOR_ID &&
		data_message->header == RD_HEADER_CONFIG_SET_TIME_ACTION_PIR_LIGHT_SENSOR)
	{
		time = data_message->time;
		BuildTelemetryValue(jsonValue);
		CheckTrigger(jsonValue);

		return CODE_OK;
	}
	return CODE_ERROR;
}

bool ModuleTimeActionPir::CheckData(Json::Value &dataValue, bool &rs)
{
	LOGV("CheckData data: %s", dataValue.toString().c_str());
	if (dataValue.isObject() &&
			dataValue.isMember(KEY_ATTRIBUTE_ACTIME) &&
			dataValue.isMember("op") && dataValue["op"].isString())
	{
		string op = dataValue["op"].asString();
		if (dataValue[KEY_ATTRIBUTE_ACTIME].isInt())
		{
			int time = dataValue[KEY_ATTRIBUTE_ACTIME].asInt();
			rs = Util::CompareNumber(op, this->time, time);
			return true;
		}
		else if (dataValue[KEY_ATTRIBUTE_ACTIME].isArray())
		{
			Json::Value listValue = dataValue[KEY_ATTRIBUTE_ACTIME];
			if (listValue.size() == 2 && listValue[0].isInt() && listValue[1].isInt())
			{
				int time1 = listValue[0].asInt();
				int time2 = listValue[1].asInt();
				rs = Util::CompareNumber(op, this->time, time1, time2);
				return true;
			}
		}
	}
	return false;
}

void ModuleTimeActionPir::BuildTelemetryValue(Json::Value &jsonValue)
{
	jsonValue[KEY_ATTRIBUTE_ACTIME] = time;
}

int ModuleTimeActionPir::Do(Json::Value &dataValue)
{
	LOGV("Do data: %s", dataValue.toString().c_str());
	if (dataValue.isObject() &&
			dataValue.isMember(KEY_ATTRIBUTE_ACTIME) && dataValue[KEY_ATTRIBUTE_ACTIME].isInt())
	{
		int time = dataValue[KEY_ATTRIBUTE_ACTIME].asInt();
		if (BleProtocol::getInstance()->TimeActionPirLightSensor(addr, time) == CODE_OK)
		{
			this->time = time;
			return CODE_OK;
		}
	}
	return CODE_ERROR;
}

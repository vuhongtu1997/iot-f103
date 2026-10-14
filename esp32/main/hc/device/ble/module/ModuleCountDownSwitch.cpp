#include "ModuleCountDownSwitch.h"
#include "Log.h"
#include "Util.h"
#include "Device.h"
#include "BleProtocol.h"
#include "Database.h"

ModuleCountDownSwitch::ModuleCountDownSwitch(Device *device, uint32_t addr, uint32_t index)
	: Module(device, addr, index)
{
	time = 0;
	status = 0;
}

ModuleCountDownSwitch::~ModuleCountDownSwitch()
{
}

int ModuleCountDownSwitch::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
	if (dataValue.isObject() &&
		dataValue.isMember(KEY_ATTRIBUTE_COUNTDOWN) && dataValue[KEY_ATTRIBUTE_COUNTDOWN].isInt())
	{
		time = dataValue[KEY_ATTRIBUTE_COUNTDOWN].asInt();
		BuildTelemetryValue(jsonValue);
		return CODE_OK;
	}

	const string timerKey = to_string(index + 1);
	if (dataValue.isObject() &&
		dataValue.isMember(KEY_ATTRIBUTE_COUNTDOWN_TIMER) &&
		dataValue[KEY_ATTRIBUTE_COUNTDOWN_TIMER].isObject() &&
		dataValue[KEY_ATTRIBUTE_COUNTDOWN_TIMER].isMember(timerKey))
	{
		const Json::Value &timerValue =
			dataValue[KEY_ATTRIBUTE_COUNTDOWN_TIMER][timerKey];
		if (timerValue.isObject() && timerValue.isMember("timer") &&
			timerValue["timer"].isInt() && timerValue.isMember("status") &&
			timerValue["status"].isInt())
		{
			time = timerValue["timer"].asInt();
			status = timerValue["status"].asInt();
			BuildTelemetryValue(jsonValue);
			return CODE_OK;
		}
	}
	return CODE_ERROR;
}

int ModuleCountDownSwitch::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
	typedef struct __attribute__((packed))
	{
		uint8_t opcode;
		uint16_t vendorId;
		uint16_t header;
		uint8_t status;
		uint16_t time;
	} data_message_t;
	data_message_t *data_message = (data_message_t *)data;
	if (data_message->opcode == RD_OPCODE_CONFIG_RSP &&
		data_message->vendorId == RD_VENDOR_ID &&
		data_message->header == RD_HEADER_CONFIG_SET_TIMER)
	{
		time = data_message->time;
		status = data_message->status;
		BuildTelemetryValue(jsonValue);
		CheckTrigger(jsonValue);
		return CODE_OK;
	}
	return CODE_ERROR;
}

bool ModuleCountDownSwitch::CheckData(Json::Value &dataValue, bool &rs)
{
	LOGV("CheckData data: %s", dataValue.toString().c_str());
	if (dataValue.isObject() &&
		dataValue.isMember(KEY_ATTRIBUTE_DISTANCE) &&
		dataValue.isMember("op") && dataValue["op"].isString())
	{
		string op = dataValue["op"].asString();
		if (dataValue[KEY_ATTRIBUTE_DISTANCE].isInt())
		{
			int value = dataValue[KEY_ATTRIBUTE_DISTANCE].asInt();
			rs = Util::CompareNumber(op, this->time, value);
			return true;
		}
		else if (dataValue[KEY_ATTRIBUTE_DISTANCE].isArray())
		{
			Json::Value listValue = dataValue[KEY_ATTRIBUTE_DISTANCE];
			if (listValue.size() == 2 && listValue[0].isInt() && listValue[1].isInt())
			{
				int value1 = listValue[0].asInt();
				int value2 = listValue[1].asInt();
				rs = Util::CompareNumber(op, this->time, value1, value2);
				return true;
			}
		}
	}
	return false;
}

void ModuleCountDownSwitch::BuildTelemetryValue(Json::Value &jsonValue)
{
	const string timerKey = to_string(index + 1);
	jsonValue[KEY_ATTRIBUTE_COUNTDOWN_TIMER][timerKey]["timer"] = time;
	jsonValue[KEY_ATTRIBUTE_COUNTDOWN_TIMER][timerKey]["status"] = status;

	if (index == 0)
		jsonValue[KEY_ATTRIBUTE_COUNTDOWN] = time;
}

int ModuleCountDownSwitch::Do(Json::Value &dataValue)
{
	LOGV("ModuleCountDownSwitch Do data: %s", dataValue.toString().c_str());
	if (index == 0 && dataValue.isObject() &&
		dataValue.isMember(KEY_ATTRIBUTE_COUNTDOWN) && dataValue[KEY_ATTRIBUTE_COUNTDOWN].isInt())
	{
		int time = dataValue[KEY_ATTRIBUTE_COUNTDOWN].asInt();
		if (BleProtocol::getInstance()->CountDownSwitch(addr, time, 0) == CODE_OK)
		{
			this->time = time;
			this->status = 0;
			return CODE_OK;
		}
	}

	const string timerKey = to_string(index + 1);
	if (dataValue.isObject() &&
		dataValue.isMember(KEY_ATTRIBUTE_COUNTDOWN_TIMER) &&
		dataValue[KEY_ATTRIBUTE_COUNTDOWN_TIMER].isObject() &&
		dataValue[KEY_ATTRIBUTE_COUNTDOWN_TIMER].isMember(timerKey))
	{
		const Json::Value &timerValue =
			dataValue[KEY_ATTRIBUTE_COUNTDOWN_TIMER][timerKey];
		if (!timerValue.isObject() || !timerValue.isMember("timer") ||
			!timerValue["timer"].isInt() || !timerValue.isMember("status") ||
			!timerValue["status"].isInt())
			return CODE_ERROR;

		int timer = timerValue["timer"].asInt();
		int timerStatus = timerValue["status"].asInt();
		if (timer < 0 || timer > UINT16_MAX || timerStatus < 0 ||
			timerStatus > UINT8_MAX)
			return CODE_ERROR;

		if (BleProtocol::getInstance()->CountDownSwitch(
				addr, static_cast<uint16_t>(timer),
				static_cast<uint8_t>(timerStatus)) == CODE_OK)
		{
			time = static_cast<uint16_t>(timer);
			status = static_cast<uint8_t>(timerStatus);
			return CODE_OK;
		}
	}
	return CODE_ERROR;
}

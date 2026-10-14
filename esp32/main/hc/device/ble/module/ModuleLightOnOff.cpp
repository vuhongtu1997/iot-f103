#include "ModuleLightOnOff.h"
#include "BleOpCode.h"
#include "BleProtocol.h"
#include "Device.h"
#include "Log.h"
#include "Util.h"

ModuleLightOnOff::ModuleLightOnOff(Device *device, uint16_t addr, string onoffKey)
	: ModuleOnOff(device, addr)
{
	key = onoffKey;
}

int ModuleLightOnOff::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
	if (dataValue.isObject() && dataValue.isMember(key) &&
		(dataValue[key].isBool() || dataValue[key].isInt()))
	{
		onoff = dataValue[key].isBool() ? dataValue[key].asBool() : dataValue[key].asInt();
		BuildTelemetryValue(jsonValue);
		return CODE_OK;
	}
	return CODE_ERROR;
}

int ModuleLightOnOff::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
	int temp_onoff = 0;
	if (data[0] == 0x82)
	{
		typedef struct __attribute__((packed))
		{
			uint16_t opcode;
			uint8_t state;
			uint8_t onoff;
		} data_message_t;
		data_message_t *data_message = (data_message_t *)data;
		if (data_message->opcode == G_ONOFF_STATUS)
		{
			if (len == 3)
			{
				temp_onoff = data_message->state;
			}
			else if (len >= 5 && data[2] == (G_ONOFF_SET & 0xFF) &&
					 data[3] == ((G_ONOFF_SET >> 8) & 0xFF))
			{
				temp_onoff = (data[4] == 0x01) ? 1 : 0;
			}
			else
			{
				temp_onoff = data_message->onoff;
			}

			if (temp_onoff != onoff)
				onoff = temp_onoff;

			BuildTelemetryValue(jsonValue);
			CheckTrigger(jsonValue);
			return CODE_OK;
		}
	}
	return CODE_ERROR;
}

bool ModuleLightOnOff::CheckData(Json::Value &dataValue, bool &rs)
{
	if (dataValue.isArray() && dataValue[0].asString() == key)
	{
		string op = dataValue[1].asString();
		if (dataValue[2].isBool() || dataValue[2].isInt())
		{
			bool value = dataValue[2].isBool() ? dataValue[2].asBool() : dataValue[2].asInt();
			rs = Util::CompareNumber(op, this->onoff, value);
			return true;
		}
	}
	return false;
}

void ModuleLightOnOff::BuildTelemetryValue(Json::Value &jsonValue)
{
	jsonValue[key] = (onoff != 0) ? 1 : 0;
	device->UpdatePropertyJsonUpdate(jsonValue);
}

int ModuleLightOnOff::Do(Json::Value &dataValue)
{
	LOGI("ModuleLightOnOff::Do, key: %s, data: %s", key.c_str(), dataValue.toString().c_str());
	if (dataValue.isObject() && dataValue.isMember(key) &&
		(dataValue[key].isBool() || dataValue[key].isInt()))
	{
		int temp_onoff = dataValue[key].isBool() ? dataValue[key].asBool() : dataValue[key].asInt();
		int rs = BleProtocol::getInstance()->SetOnOffLight(addr, temp_onoff, TRANSITION_DEFAULT, true);
		if (rs == CODE_OK)
		{
			onoff = temp_onoff;
			return CODE_OK;
		}
	}
	return CODE_ERROR;
}

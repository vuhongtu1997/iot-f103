#include "ModuleCct.h"
#include "Log.h"
#include "Util.h"
#include "Device.h"
#include "BleProtocol.h"
#include "BleOpCode.h"
#include "Database.h"

namespace
{
int CctRawToValue(uint16_t rawCct)
{
	if (rawCct <= 800)
		return 0;
	if (rawCct >= 20000)
		return 100;
	return static_cast<int>(((static_cast<uint32_t>(rawCct) - 800) * 100 +
							 9600) /
							19200);
}

uint16_t CctValueToRaw(int cct)
{
	if (cct < 0)
		cct = 0;
	else if (cct > 100)
		cct = 100;
	return static_cast<uint16_t>(800 + (cct * 19200 + 50) / 100);
}
}

ModuleCct::ModuleCct(Device *device, uint16_t addr, string cctKey) : Module(device, addr)
{
	cct = 0;
	key = cctKey;
}

ModuleCct::~ModuleCct()
{
}

#ifdef CONFIG_SAVE_ATTRIBUTE
void ModuleCct::InitAttribute(string attribute, double value)
{
	if (attribute == key)
		cct = value;
}

void ModuleCct::SaveAttribute()
{
	Database::getInstance()->DeviceAttributeAdd(device, key, cct);
}
#endif

int ModuleCct::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
	if (dataValue.isObject() && dataValue.isMember(key) && dataValue[key].isInt())
	{
		cct = dataValue[key].asInt();
		BuildTelemetryValue(jsonValue);
		return CODE_OK;
	}
	return CODE_ERROR;
}

int ModuleCct::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
	if (len < 4)
	{
		return CODE_ERROR;
	}

	typedef struct __attribute__((packed))
	{
		uint16_t opcode;
		uint16_t cct_first;
		uint16_t magic;
		uint16_t cct;
	} data_message_t;
	data_message_t *data_message = (data_message_t *)data;
	if (data_message->opcode == LIGHT_CTL_TEMP_STATUS)
	{
		int temp_cct;
		if (len >= 7 &&
			data[2] == (LIGHT_CTL_TEMP_SET & 0xFF) &&
			data[3] == ((LIGHT_CTL_TEMP_SET >> 8) & 0xFF))
		{
			uint16_t raw_cct = data[5] | (data[6] << 8);
			temp_cct = CctRawToValue(raw_cct);
		}
		else if (len <= 6)
		{
			temp_cct = CctRawToValue(data_message->cct_first);
		}
		else
		{
			temp_cct = CctRawToValue(data_message->cct);
		}
		if (temp_cct != cct)
		{
			cct = temp_cct;
#ifdef CONFIG_SAVE_ATTRIBUTE
			SaveAttribute();
#endif
		}
		BuildTelemetryValue(jsonValue);
		CheckTrigger(jsonValue);
		return CODE_OK;
	}
	return CODE_ERROR;
}

bool ModuleCct::CheckData(Json::Value &dataValue, bool &rs)
{
	LOGV("CheckData data: %s", dataValue.toString().c_str());
	if (dataValue.isObject() &&
		dataValue.isMember(key) &&
		dataValue.isMember("op") && dataValue["op"].isString())
	{
		string op = dataValue["op"].asString();
		if (dataValue[key].isInt())
		{
			int cct = dataValue[key].asInt();
			rs = Util::CompareNumber(op, this->cct, cct);
			return true;
		}
		else if (dataValue[key].isArray())
		{
			Json::Value listValue = dataValue[key];
			if (listValue.size() == 2 && listValue[0].isInt() && listValue[1].isInt())
			{
				int cct1 = listValue[0].asInt();
				int cct2 = listValue[1].asInt();
				rs = Util::CompareNumber(op, this->cct, cct1, cct2);
				return true;
			}
		}
	}
	return false;
}

void ModuleCct::BuildTelemetryValue(Json::Value &jsonValue)
{
	jsonValue[key] = cct;
	device->UpdatePropertyJsonUpdate(jsonValue);
}

int ModuleCct::Do(Json::Value &dataValue)
{
	LOGV("Do data: %s", dataValue.toString().c_str());
	if (dataValue.isObject() &&
		dataValue.isMember(key) && dataValue[key].isInt())
	{
		int cct = dataValue[key].asInt();
		if (BleProtocol::getInstance()->SetCctLight(addr, CctValueToRaw(cct), TRANSITION_DEFAULT, true) == CODE_OK)
		{
			return CODE_OK;
		}
	}
	return CODE_ERROR;
}

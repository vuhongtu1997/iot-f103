#include "ModuleHsl.h"
#include "Log.h"
#include "Util.h"
#include "Device.h"
#include "BleProtocol.h"
#include "BleOpCode.h"
#include "Database.h"

ModuleHsl::ModuleHsl(Device *device, uint16_t addr) : Module(device, addr)
{
	h = 0;
	s = 0;
	l = 0;
}

ModuleHsl::~ModuleHsl()
{
}

#ifdef CONFIG_SAVE_ATTRIBUTE
void ModuleHsl::InitAttribute(string attribute, double value)
{
	if (attribute == KEY_ATTRIBUTE_HUE)
	{
		h = value;
	}
	else if (attribute == KEY_ATTRIBUTE_SATURATION)
	{
		s = value;
	}
	else if (attribute == KEY_ATTRIBUTE_LUMINANCE)
	{
		l = value;
	}
}

void ModuleHsl::SaveAttribute(string key)
{
	if (key == KEY_ATTRIBUTE_HUE)
	{
		Database::getInstance()->DeviceAttributeAdd(device, KEY_ATTRIBUTE_HUE, h);
	}
	else if (key == KEY_ATTRIBUTE_SATURATION)
	{
		Database::getInstance()->DeviceAttributeAdd(device, KEY_ATTRIBUTE_SATURATION, s);
	}
	else if (key == KEY_ATTRIBUTE_LUMINANCE)
	{
		Database::getInstance()->DeviceAttributeAdd(device, KEY_ATTRIBUTE_LUMINANCE, l);
	}
}
#endif

int ModuleHsl::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
	if (dataValue.isObject() &&
		dataValue.isMember(KEY_ATTRIBUTE_HUE) && dataValue[KEY_ATTRIBUTE_HUE].isInt() &&
		dataValue.isMember(KEY_ATTRIBUTE_SATURATION) && dataValue[KEY_ATTRIBUTE_SATURATION].isInt() &&
		dataValue.isMember(KEY_ATTRIBUTE_LUMINANCE) && dataValue[KEY_ATTRIBUTE_LUMINANCE].isInt())
	{
		h = dataValue[KEY_ATTRIBUTE_HUE].asInt();
		s = dataValue[KEY_ATTRIBUTE_SATURATION].asInt();
		l = dataValue[KEY_ATTRIBUTE_LUMINANCE].asInt();
		BuildTelemetryValue(jsonValue);
		return CODE_OK;
	}
	return CODE_ERROR;
}

int ModuleHsl::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
	typedef struct __attribute__((packed))
	{
		uint16_t opcode;
		uint16_t l;
		uint16_t h;
		uint16_t s;
	} data_message_t;
	// Status may echo the Set opcode (82 76) right after the status opcode.
	int offset = 0;
	if (len >= 10 && data[2] == (LIGHT_HSL_SET & 0xFF) &&
		data[3] == ((LIGHT_HSL_SET >> 8) & 0xFF))
		offset = 2;
	if (len < offset + 8)
		return CODE_ERROR;
	data_message_t message;
	message.opcode = data[0] | (data[1] << 8);
	message.l = data[offset + 2] | (data[offset + 3] << 8);
	message.h = data[offset + 4] | (data[offset + 5] << 8);
	message.s = data[offset + 6] | (data[offset + 7] << 8);
	data_message_t *data_message = &message;
	if (data_message->opcode == BLE_MESH_OPCODE_HSL)
	{
		if (data_message->l != l)
		{
			l = data_message->l;
		}

		if (data_message->s != s)
		{
			s = data_message->s;
		}

		if (data_message->h != h)
		{
			h = data_message->h;
		}

		if (h > 0 && s > 0 && l > 0)
		{
#ifdef CONFIG_SAVE_ATTRIBUTE
			SaveAttribute(KEY_ATTRIBUTE_HUE);
			SaveAttribute(KEY_ATTRIBUTE_SATURATION);
			SaveAttribute(KEY_ATTRIBUTE_LUMINANCE);
#endif
			BuildTelemetryValue(jsonValue);
			CheckTrigger(jsonValue);
			return CODE_OK;
		}
	}
	return CODE_ERROR;
}

bool ModuleHsl::CheckData(Json::Value &dataValue, bool &rs)
{
	LOGV("CheckData data: %s", dataValue.toString().c_str());
	if (dataValue.isObject() &&
		dataValue.isMember("op") && dataValue["op"].isString())
	{
		string op = dataValue["op"].asString();
		if (dataValue.isMember(KEY_ATTRIBUTE_HUE))
		{
			if (dataValue[KEY_ATTRIBUTE_HUE].isInt())
			{
				int h = dataValue[KEY_ATTRIBUTE_HUE].asInt();
				rs = Util::CompareNumber(op, this->h, h);
				return true;
			}
			else if (dataValue[KEY_ATTRIBUTE_HUE].isArray())
			{
				Json::Value listValue = dataValue[KEY_ATTRIBUTE_HUE];
				if (listValue.size() == 2 && listValue[0].isInt() && listValue[1].isInt())
				{
					int h1 = listValue[0].asInt();
					int h2 = listValue[1].asInt();
					rs = Util::CompareNumber(op, this->h, h1, h2);
					return true;
				}
			}
		}
		else if (dataValue.isMember(KEY_ATTRIBUTE_SATURATION))
		{
			if (dataValue[KEY_ATTRIBUTE_SATURATION].isInt())
			{
				int s = dataValue[KEY_ATTRIBUTE_SATURATION].asInt();
				rs = Util::CompareNumber(op, this->s, s);
				return true;
			}
			else if (dataValue[KEY_ATTRIBUTE_SATURATION].isArray())
			{
				Json::Value listValue = dataValue[KEY_ATTRIBUTE_SATURATION];
				if (listValue.size() == 2 && listValue[0].isInt() && listValue[1].isInt())
				{
					int s1 = listValue[0].asInt();
					int s2 = listValue[1].asInt();
					rs = Util::CompareNumber(op, this->s, s1, s2);
					return true;
				}
			}
		}
		else if (dataValue.isMember(KEY_ATTRIBUTE_LUMINANCE))
		{
			if (dataValue[KEY_ATTRIBUTE_LUMINANCE].isInt())
			{
				int l = dataValue[KEY_ATTRIBUTE_LUMINANCE].asInt();
				rs = Util::CompareNumber(op, this->l, l);
				return true;
			}
			else if (dataValue[KEY_ATTRIBUTE_LUMINANCE].isArray())
			{
				Json::Value listValue = dataValue[KEY_ATTRIBUTE_LUMINANCE];
				if (listValue.size() == 2 && listValue[0].isInt() && listValue[1].isInt())
				{
					int l1 = listValue[0].asInt();
					int l2 = listValue[1].asInt();
					rs = Util::CompareNumber(op, this->l, l1, l2);
					return true;
				}
			}
		}
	}
	return false;
}

void ModuleHsl::BuildTelemetryValue(Json::Value &jsonValue)
{
	if (h > 0 && s > 0 && l > 0)
	{
		jsonValue[KEY_ATTRIBUTE_HUE] = h;
		jsonValue[KEY_ATTRIBUTE_SATURATION] = s;
		jsonValue[KEY_ATTRIBUTE_LUMINANCE] = l;
		device->UpdatePropertyJsonUpdate(jsonValue);
	}
}

int ModuleHsl::Do(Json::Value &dataValue)
{
	LOGV("Do data: %s", dataValue.toString().c_str());
	if (dataValue.isObject() &&
		dataValue.isMember(KEY_ATTRIBUTE_HUE) && dataValue[KEY_ATTRIBUTE_HUE].isInt() &&
		dataValue.isMember(KEY_ATTRIBUTE_SATURATION) && dataValue[KEY_ATTRIBUTE_SATURATION].isInt() &&
		dataValue.isMember(KEY_ATTRIBUTE_LUMINANCE) && dataValue[KEY_ATTRIBUTE_LUMINANCE].isInt())
	{
		int h = dataValue[KEY_ATTRIBUTE_HUE].asInt();
		int s = dataValue[KEY_ATTRIBUTE_SATURATION].asInt();
		int l = dataValue[KEY_ATTRIBUTE_LUMINANCE].asInt();
		if (BleProtocol::getInstance()->SetHSLLight(addr, h, s, l, TRANSITION_DEFAULT, true) == CODE_OK)
		{
			return CODE_OK;
		}
	}
	return CODE_ERROR;
}

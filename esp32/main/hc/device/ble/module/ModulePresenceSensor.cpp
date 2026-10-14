#include "ModulePresenceSensor.h"

#include "Database.h"
#include "Device.h"
#include "Log.h"
#include "Util.h"

ModulePresenceSensor::ModulePresenceSensor(Device *device, uint16_t addr)
	: Module(device, addr)
{
	active = 0;
	type = 0;
	lux = 0;
}

ModulePresenceSensor::~ModulePresenceSensor()
{
}

#ifdef CONFIG_SAVE_ATTRIBUTE
void ModulePresenceSensor::InitAttribute(string attribute, double value)
{
	if (attribute == KEY_ATTRIBUTE_PRESENCE_ACTIVE)
		active = (uint8_t)value;
	else if (attribute == KEY_ATTRIBUTE_PRESENCE_TYPE)
		type = (uint8_t)value;
	else if (attribute == KEY_ATTRIBUTE_PRESENCE_LUX)
		lux = (uint16_t)value;
}

void ModulePresenceSensor::SaveAttribute(const string &attribute)
{
	if (attribute == KEY_ATTRIBUTE_PRESENCE_ACTIVE)
		Database::getInstance()->DeviceAttributeAdd(device, attribute, active);
	else if (attribute == KEY_ATTRIBUTE_PRESENCE_TYPE)
		Database::getInstance()->DeviceAttributeAdd(device, attribute, type);
	else if (attribute == KEY_ATTRIBUTE_PRESENCE_LUX)
		Database::getInstance()->DeviceAttributeAdd(device, attribute, lux);
}
#endif

int ModulePresenceSensor::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
	if (!dataValue.isObject())
		return CODE_ERROR;

	bool changed = false;
	if (dataValue.isMember(KEY_ATTRIBUTE_PRESENCE_ACTIVE) &&
		dataValue[KEY_ATTRIBUTE_PRESENCE_ACTIVE].isInt())
	{
		active = (uint8_t)(dataValue[KEY_ATTRIBUTE_PRESENCE_ACTIVE].asInt() + 1);
		changed = true;
	}
	if (dataValue.isMember(KEY_ATTRIBUTE_PRESENCE_TYPE) &&
		dataValue[KEY_ATTRIBUTE_PRESENCE_TYPE].isInt())
	{
		type = (uint8_t)(dataValue[KEY_ATTRIBUTE_PRESENCE_TYPE].asInt() + 1);
		changed = true;
	}
	if (dataValue.isMember(KEY_ATTRIBUTE_PRESENCE_LUX) &&
		dataValue[KEY_ATTRIBUTE_PRESENCE_LUX].isInt())
	{
		lux = (uint16_t)dataValue[KEY_ATTRIBUTE_PRESENCE_LUX].asInt();
		changed = true;
	}

	if (!changed)
		return CODE_ERROR;

	BuildTelemetryValue(jsonValue);
	return jsonValue.empty() ? CODE_ERROR : CODE_OK;
}

int ModulePresenceSensor::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
	if (len < 9)
		return CODE_ERROR;

	bool matchOpcode = (data[0] == 0x11 && data[1] == 0xE1) ||
					   (data[0] == 0xE1 && data[1] == 0x11);
	if (!matchOpcode || data[2] != 0x02 || data[3] != 0x02 || data[4] != 0xFF)
		return CODE_ERROR;

	uint8_t tempActive = data[5];
	uint8_t tempType = data[6];
	uint16_t tempLux = (uint16_t)data[7] | ((uint16_t)data[8] << 8);

	if (active != tempActive)
	{
		active = tempActive;
#ifdef CONFIG_SAVE_ATTRIBUTE
		SaveAttribute(KEY_ATTRIBUTE_PRESENCE_ACTIVE);
#endif
	}
	if (type != tempType)
	{
		type = tempType;
#ifdef CONFIG_SAVE_ATTRIBUTE
		SaveAttribute(KEY_ATTRIBUTE_PRESENCE_TYPE);
#endif
	}
	if (lux != tempLux)
	{
		lux = tempLux;
#ifdef CONFIG_SAVE_ATTRIBUTE
		SaveAttribute(KEY_ATTRIBUTE_PRESENCE_LUX);
#endif
	}

	BuildTelemetryValue(jsonValue);
	if (jsonValue.empty())
	{
		LOGI("Presence sensor 0x%04X frame ignored because all DPs are zero", addr);
		return CODE_ERROR;
	}

	LOGI("Presence sensor 0x%04X telemetry: %s", addr, jsonValue.toString().c_str());
	CheckTrigger(jsonValue);
	return CODE_OK;
}

bool ModulePresenceSensor::CheckData(Json::Value &dataValue, bool &rs)
{
	if (dataValue.isArray() && dataValue.size() >= 3 &&
		dataValue[0].isString() && dataValue[1].isString())
	{
		string key = dataValue[0].asString();
		string op = dataValue[1].asString();
		if (dataValue[2].isInt())
		{
			if (key == KEY_ATTRIBUTE_PRESENCE_ACTIVE)
			{
				rs = Util::CompareNumber(op, active > 0 ? active - 1 : 0,
										 dataValue[2].asInt());
				return true;
			}
			if (key == KEY_ATTRIBUTE_PRESENCE_TYPE)
			{
				rs = Util::CompareNumber(op, type > 0 ? type - 1 : 0,
										 dataValue[2].asInt());
				return true;
			}
			if (key == KEY_ATTRIBUTE_PRESENCE_LUX)
			{
				rs = Util::CompareNumber(op, lux, dataValue[2].asInt());
				return true;
			}
		}
	}
	return false;
}

void ModulePresenceSensor::BuildTelemetryValue(Json::Value &jsonValue)
{
	if (active > 0)
		jsonValue[KEY_ATTRIBUTE_PRESENCE_ACTIVE] = active - 1;
	if (type > 0)
		jsonValue[KEY_ATTRIBUTE_PRESENCE_TYPE] = type - 1;
	if (lux > 0)
		jsonValue[KEY_ATTRIBUTE_PRESENCE_LUX] = lux;
	device->UpdatePropertyJsonUpdate(jsonValue);
}

#include "ModuleDoorSensor.h"

#include "Database.h"
#include "Device.h"
#include "Log.h"
#include "Util.h"

ModuleDoorSensor::ModuleDoorSensor(Device *device, uint16_t addr)
	: Module(device, addr)
{
	detect = 0;
	alarm = false;
	battery = 0;
}

ModuleDoorSensor::~ModuleDoorSensor()
{
}

#ifdef CONFIG_SAVE_ATTRIBUTE
void ModuleDoorSensor::InitAttribute(string attribute, double value)
{
	if (attribute == KEY_ATTRIBUTE_DOOR_DETECT)
		detect = (value != 0);
	else if (attribute == KEY_ATTRIBUTE_DOOR_ALARM)
		alarm = (value != 0);
	else if (attribute == KEY_ATTRIBUTE_DOOR_BATTERY)
		battery = (uint8_t)value;
}

void ModuleDoorSensor::SaveAttribute(const string &attribute)
{
	if (attribute == KEY_ATTRIBUTE_DOOR_DETECT)
		Database::getInstance()->DeviceAttributeAdd(device, attribute, detect ? 1 : 0);
	else if (attribute == KEY_ATTRIBUTE_DOOR_ALARM)
		Database::getInstance()->DeviceAttributeAdd(device, attribute, alarm ? 1 : 0);
	else if (attribute == KEY_ATTRIBUTE_DOOR_BATTERY)
		Database::getInstance()->DeviceAttributeAdd(device, attribute, battery);
}
#endif

int ModuleDoorSensor::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
	if (!dataValue.isObject())
		return CODE_ERROR;

	bool changed = false;
	if (dataValue.isMember(KEY_ATTRIBUTE_DOOR_DETECT))
	{
		if (dataValue[KEY_ATTRIBUTE_DOOR_DETECT].isBool())
			detect = dataValue[KEY_ATTRIBUTE_DOOR_DETECT].asBool();
		else if (dataValue[KEY_ATTRIBUTE_DOOR_DETECT].isInt())
			detect = dataValue[KEY_ATTRIBUTE_DOOR_DETECT].asInt() != 0;
		else
			return CODE_ERROR;
		changed = true;
	}

	if (dataValue.isMember(KEY_ATTRIBUTE_DOOR_ALARM))
	{
		if (dataValue[KEY_ATTRIBUTE_DOOR_ALARM].isBool())
			alarm = dataValue[KEY_ATTRIBUTE_DOOR_ALARM].asBool();
		else if (dataValue[KEY_ATTRIBUTE_DOOR_ALARM].isInt())
			alarm = dataValue[KEY_ATTRIBUTE_DOOR_ALARM].asInt() != 0;
		else
			return CODE_ERROR;
		changed = true;
	}

	if (dataValue.isMember(KEY_ATTRIBUTE_DOOR_BATTERY) &&
		dataValue[KEY_ATTRIBUTE_DOOR_BATTERY].isInt())
	{
		int value = dataValue[KEY_ATTRIBUTE_DOOR_BATTERY].asInt();
		if (value < 0)
			value = 0;
		if (value > 100)
			value = 100;
		battery = (uint8_t)value;
		changed = true;
	}

	if (!changed)
		return CODE_ERROR;

	BuildTelemetryValue(jsonValue);
	return CODE_OK;
}

int ModuleDoorSensor::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
	if (len < 4 || data[0] != 0x52)
		return CODE_ERROR;

	bool matched = false;
	if (data[1] == 0x09 && len >= 4)
	{
		if (data[2] == 0x00)
		{
			detect = data[3] == 0x01;
			jsonValue[KEY_ATTRIBUTE_DOOR_DETECT] = detect ? 1 : 0;
			matched = true;
#ifdef CONFIG_SAVE_ATTRIBUTE
			SaveAttribute(KEY_ATTRIBUTE_DOOR_DETECT);
#endif
		}
		else if (data[2] == 0x04)
		{
			alarm = data[3] == 0x01;
			jsonValue[KEY_ATTRIBUTE_DOOR_ALARM] = alarm ? 1 : 0;
			matched = true;
#ifdef CONFIG_SAVE_ATTRIBUTE
			SaveAttribute(KEY_ATTRIBUTE_DOOR_ALARM);
#endif
		}
	}
	else if (data[1] == 0x01 && len >= 5 && data[2] == 0x00)
	{
		battery = data[4] > 100 ? 100 : data[4];
		jsonValue[KEY_ATTRIBUTE_DOOR_BATTERY] = battery;
		matched = true;
#ifdef CONFIG_SAVE_ATTRIBUTE
		SaveAttribute(KEY_ATTRIBUTE_DOOR_BATTERY);
#endif
	}

	if (!matched)
		return CODE_ERROR;

	device->UpdatePropertyJsonUpdate(jsonValue);
	LOGI("Door sensor 0x%04X telemetry: %s", addr, jsonValue.toString().c_str());
	CheckTrigger(jsonValue);
	return CODE_OK;
}

bool ModuleDoorSensor::CheckData(Json::Value &dataValue, bool &rs)
{
	if (dataValue.isArray() && dataValue.size() >= 3 &&
		dataValue[0].isString() && dataValue[1].isString())
	{
		string key = dataValue[0].asString();
		string op = dataValue[1].asString();
		int current = 0;
		bool knownKey = true;

		if (key == KEY_ATTRIBUTE_DOOR_DETECT)
			current = detect ? 1 : 0;
		else if (key == KEY_ATTRIBUTE_DOOR_ALARM)
			current = alarm ? 1 : 0;
		else if (key == KEY_ATTRIBUTE_DOOR_BATTERY)
			current = battery;
		else
			knownKey = false;

		if (knownKey)
		{
			if (dataValue[2].isBool())
			{
				rs = Util::CompareNumber(op, current, dataValue[2].asBool() ? 1 : 0);
				return true;
			}
			if (dataValue[2].isInt())
			{
				rs = Util::CompareNumber(op, current, dataValue[2].asInt());
				return true;
			}
		}
	}
	return false;
}

void ModuleDoorSensor::BuildTelemetryValue(Json::Value &jsonValue)
{
	jsonValue[KEY_ATTRIBUTE_DOOR_DETECT] = detect ? 1 : 0;
	jsonValue[KEY_ATTRIBUTE_DOOR_ALARM] = alarm ? 1 : 0;
	jsonValue[KEY_ATTRIBUTE_DOOR_BATTERY] = battery;
	device->UpdatePropertyJsonUpdate(jsonValue);
}

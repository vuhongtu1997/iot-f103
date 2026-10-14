#include "ModuleSmokeSensor.h"

#include "Database.h"
#include "Device.h"
#include "Log.h"
#include "Util.h"

ModuleSmokeSensor::ModuleSmokeSensor(Device *device, uint16_t addr)
	: Module(device, addr)
{
	smoke = false;
	battery = 0;
}

ModuleSmokeSensor::~ModuleSmokeSensor()
{
}

#ifdef CONFIG_SAVE_ATTRIBUTE
void ModuleSmokeSensor::InitAttribute(string attribute, double value)
{
	if (attribute == KEY_ATTRIBUTE_SMOKE)
		smoke = (value != 0);
	else if (attribute == KEY_ATTRIBUTE_DOOR_BATTERY)
		battery = static_cast<uint8_t>(value);
}

void ModuleSmokeSensor::SaveAttribute(const string &attribute)
{
	if (attribute == KEY_ATTRIBUTE_SMOKE)
		Database::getInstance()->DeviceAttributeAdd(device, attribute,
												   smoke ? 1 : 0);
	else if (attribute == KEY_ATTRIBUTE_DOOR_BATTERY)
		Database::getInstance()->DeviceAttributeAdd(device, attribute, battery);
}
#endif

int ModuleSmokeSensor::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
	if (!dataValue.isObject())
		return CODE_ERROR;

	bool changed = false;
	if (dataValue.isMember(KEY_ATTRIBUTE_SMOKE) &&
		dataValue[KEY_ATTRIBUTE_SMOKE].isBool())
	{
		smoke = dataValue[KEY_ATTRIBUTE_SMOKE].asBool();
		changed = true;
	}
	else if (dataValue.isMember(KEY_ATTRIBUTE_SMOKE) &&
			 dataValue[KEY_ATTRIBUTE_SMOKE].isInt())
	{
		smoke = (dataValue[KEY_ATTRIBUTE_SMOKE].asInt() != 0);
		changed = true;
	}

	if (dataValue.isMember(KEY_ATTRIBUTE_DOOR_BATTERY) &&
		dataValue[KEY_ATTRIBUTE_DOOR_BATTERY].isInt())
	{
		battery = dataValue[KEY_ATTRIBUTE_DOOR_BATTERY].asInt() > 0 ? 100 : 0;
		changed = true;
	}

	if (!changed)
		return CODE_ERROR;

	BuildTelemetryValue(jsonValue);
	return CODE_OK;
}

int ModuleSmokeSensor::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
	if (len < 5)
		return CODE_ERROR;

	// Smoke sensor status frame observed from BLE UART:
	// 52 08 01 <state> ...
	// In this codebase raw opcode bytes are forwarded directly to modules,
	// so match the wire format instead of relying on endian-converted values.
	if (data[0] != 0x52 || data[1] != 0x08 || data[2] != 0x01)
		return CODE_ERROR;

	uint8_t tempSmoke = (data[3] == 0x01);
	uint8_t tempBattery = data[4] ? 100 : 0;
	if (smoke != (tempSmoke != 0))
	{
		smoke = (tempSmoke != 0);
#ifdef CONFIG_SAVE_ATTRIBUTE
		SaveAttribute(KEY_ATTRIBUTE_SMOKE);
#endif
		LOGI("Smoke sensor 0x%04X updated: %s", addr, smoke ? "alarm" : "normal");
	}
	if (battery != tempBattery)
	{
		battery = tempBattery;
#ifdef CONFIG_SAVE_ATTRIBUTE
		SaveAttribute(KEY_ATTRIBUTE_DOOR_BATTERY);
#endif
		LOGI("Smoke sensor 0x%04X battery=%u", addr, battery);
	}

	Database::getInstance()->DeviceHistoryAdd(
		device->GetCloudId(), 1, smoke ? "1" : "0");
	Database::getInstance()->DeviceHistoryAdd(
		device->GetCloudId(), 3, to_string(battery));

	BuildTelemetryValue(jsonValue);
	LOGI("Smoke sensor telemetry: %s", jsonValue.toString().c_str());
	CheckTrigger(jsonValue);
	return CODE_OK;
}

bool ModuleSmokeSensor::CheckData(Json::Value &dataValue, bool &rs)
{
	if (dataValue.isArray() && dataValue.size() >= 3 &&
		dataValue[0].isString() && dataValue[0].asString() == KEY_ATTRIBUTE_SMOKE &&
		dataValue[1].isString())
	{
		string op = dataValue[1].asString();
		if (dataValue[2].isBool())
		{
			rs = Util::CompareNumber(op, smoke ? 1 : 0, dataValue[2].asBool() ? 1 : 0);
			return true;
		}
		if (dataValue[2].isInt())
		{
			rs = Util::CompareNumber(op, smoke ? 1 : 0, dataValue[2].asInt());
			return true;
		}
	}
	if (dataValue.isArray() && dataValue.size() >= 3 &&
		dataValue[0].isString() &&
		dataValue[0].asString() == KEY_ATTRIBUTE_DOOR_BATTERY &&
		dataValue[1].isString() && dataValue[2].isInt())
	{
		rs = Util::CompareNumber(dataValue[1].asString(), battery,
								 dataValue[2].asInt());
		return true;
	}
	return false;
}

void ModuleSmokeSensor::BuildTelemetryValue(Json::Value &jsonValue)
{
	jsonValue[KEY_ATTRIBUTE_SMOKE] = smoke;
	jsonValue[KEY_ATTRIBUTE_DOOR_BATTERY] = battery;
	device->UpdatePropertyJsonUpdate(jsonValue);
}

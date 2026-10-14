#include "DeviceBleLightWhite.h"
#include "BleProtocol.h"
#include "BleOpCode.h"
#include "Log.h"
#include <cctype>

namespace
{
bool ParseDimLevel(const Json::Value &value, uint8_t &level)
{
	int parsed = -1;
	if (value.isInt())
	{
		parsed = value.asInt();
	}
	else if (value.isString())
	{
		const string text = value.asString();
		if (text.empty())
			return false;

		parsed = 0;
		for (char ch : text)
		{
			if (!std::isdigit(static_cast<unsigned char>(ch)))
				return false;
			parsed = parsed * 10 + (ch - '0');
			if (parsed > 100)
				return false;
		}
	}

	if (parsed < 0 || parsed > 100)
		return false;
	level = static_cast<uint8_t>(parsed);
	return true;
}

bool ParseLevelValue(const Json::Value &value, int &level)
{
	if (!value.isInt())
		return false;

	level = value.asInt();
	return level >= 0 && level <= 100;
}

bool ParseCctValue(const Json::Value &value, int &cct)
{
	if (!value.isInt())
		return false;

	cct = value.asInt();
	return cct >= 0 && cct <= 100;
}

uint16_t CctValueToRaw(int cct)
{
	return static_cast<uint16_t>(800 + (cct * 19200 + 50) / 100);
}

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
}

DeviceBleLightWhite::DeviceBleLightWhite(string id, string name, string mac,
										 string type, uint16_t addr,
										 uint16_t version,
										 Json::Value *dataJson)
	: DeviceBle(id, name, mac, type, addr, version, dataJson)
{
	countElement = 1;

	moduleOnOff = new ModuleLightOnOff(this, addr, "onoff");
	modules.push_back(moduleOnOff);

	moduleDim = new ModuleDim(this, addr, "dim", "onoff");
	modules.push_back(moduleDim);

	moduleCct = new ModuleCct(this, addr, "cct");
	modules.push_back(moduleCct);

	powerSource = POWER_AC;
}

int DeviceBleLightWhite::Do(Json::Value &dataValue)
{
	if (!dataValue.isObject())
	{
		return DeviceBle::Do(dataValue);
	}

	if (dataValue.isMember("dim") || dataValue.isMember("cct"))
	{
		int dim = moduleDim->GetDim();
		int cct = moduleCct->GetCct();

		if (dataValue.isMember("dim") &&
			!ParseLevelValue(dataValue["dim"], dim))
		{
			LOGW("Invalid CCT light dim for device %s", id.c_str());
			return CODE_ERROR;
		}

		if (dataValue.isMember("cct") &&
			!ParseCctValue(dataValue["cct"], cct))
		{
			LOGW("Invalid CCT light temperature for device %s", id.c_str());
			return CODE_ERROR;
		}

		if (BleProtocol::getInstance()->SetCctDimLight(
				addr, CctValueToRaw(cct), dim * 65535 / 100,
				TRANSITION_DEFAULT, true) == CODE_OK)
		{
			DeviceBle::InputData(dataValue);
			return CODE_OK;
		}
		return CODE_ERROR;
	}

	if (!dataValue.isMember(KEY_ATTRIBUTE_SETUP_DIMS))
	{
		return DeviceBle::Do(dataValue);
	}

	if (type != "HG_BLE_SWITCH_DIMMING")
		return CODE_ERROR;

	const Json::Value &setupDims = dataValue[KEY_ATTRIBUTE_SETUP_DIMS];
	if (!setupDims.isArray() || setupDims.size() != 5)
	{
		LOGW("Invalid setup_dims for device %s: expected 5 levels", id.c_str());
		return CODE_ERROR;
	}

	uint8_t levels[5];
	for (Json::ArrayIndex index = 0; index < setupDims.size(); index++)
	{
		if (!ParseDimLevel(setupDims[index], levels[index]))
		{
			LOGW("Invalid setup_dims[%u] for device %s",
				 static_cast<unsigned>(index), id.c_str());
			return CODE_ERROR;
		}
	}

	return BleProtocol::getInstance()->SetupDimLevels(addr, levels);
}

bool DeviceBleLightWhite::HandleBleStatus(uint16_t opcode, uint8_t *data,
										  int len, uint16_t resolvedAddr)
{
	if (type != "HG_BLE_SWITCH_DIMMING" || opcode != LIGHT_CTL_STATUS)
		return DeviceBle::HandleBleStatus(opcode, data, len, resolvedAddr);

	if (len < 9)
	{
		LOGW("Switch dimming status parse failed for devAddr=0x%04X len=%d",
			 resolvedAddr, len);
		return true;
	}

	uint16_t statusOpcode = data[0] | (data[1] << 8);
	if (statusOpcode != LIGHT_CTL_STATUS)
		return DeviceBle::HandleBleStatus(opcode, data, len, resolvedAddr);

	Json::Value dataValues = Json::objectValue;
	uint8_t onoff = data[4] ? 1 : 0;
	uint16_t lightness = data[5] | (data[6] << 8);
	uint16_t colorTemperature = data[7] | (data[8] << 8);

	dataValues["onoff"] = onoff;
	dataValues["dim"] =
		static_cast<int>((static_cast<uint32_t>(lightness) * 100 + 32767) /
						 65535);
	if (colorTemperature >= 800 && colorTemperature <= 20000)
		dataValues["cct"] = CctRawToValue(colorTemperature);

	LOGI("Switch dimming status parsed: devAddr=0x%04X data=%s",
		 resolvedAddr, dataValues.toStyledString().c_str());
	SetPropertyJsonUpdate(dataValues);
	return true;
}

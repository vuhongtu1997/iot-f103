#include "ModuleDim.h"
#include "Log.h"
#include "Util.h"
#include "Device.h"
#include "BleProtocol.h"
#include "BleOpCode.h"
#include "Database.h"

ModuleDim::ModuleDim(Device *device, uint16_t addr, string dimKey, string onoffKey) : Module(device, addr)
{
	dim = 0;
	key = dimKey;
	this->onoffKey = onoffKey;
}

ModuleDim::~ModuleDim()
{
}

static bool UsesBoolOnOffTelemetry(const string &onoffKey)
{
	return onoffKey == "20";
}

static bool UsesMilliLevel(const string &dimKey)
{
	return dimKey == "22";
}

static int RawDimToValue(uint16_t rawDim, const string &dimKey)
{
	const int scale = UsesMilliLevel(dimKey) ? 1000 : 100;
	return static_cast<int>((static_cast<uint32_t>(rawDim) * scale + 32767) /
							65535);
}

static uint16_t DimValueToRaw(int dim, const string &dimKey)
{
	const int scale = UsesMilliLevel(dimKey) ? 1000 : 100;
	if (dim < 0)
		dim = 0;
	else if (dim > scale)
		dim = scale;
	return static_cast<uint16_t>(static_cast<uint32_t>(dim) * 65535 / scale);
}

#ifdef CONFIG_SAVE_ATTRIBUTE
void ModuleDim::InitAttribute(string attribute, double value)
{
	if (attribute == key)
		dim = value;
}

void ModuleDim::SaveAttribute()
{
	Database::getInstance()->DeviceAttributeAdd(device, key, dim);
}
#endif

int ModuleDim::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
	if (dataValue.isObject() &&
		dataValue.isMember(key) && dataValue[key].isInt())
	{
		dim = dataValue[key].asInt();
		BuildTelemetryValue(jsonValue);
		return CODE_OK;
	}
	return CODE_ERROR;
}

int ModuleDim::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
	if (len < 4)
	{
		return CODE_ERROR;
	}

	typedef struct __attribute__((packed))
	{
		uint16_t opcode;
		uint16_t dim_first;
		uint16_t dim;
	} data_message_t;
	data_message_t *data_message = (data_message_t *)data;
	if (data_message->opcode == LIGHTNESS_STATUS)
	{
		int temp_dim;
		if (len >= 7 &&
			data[2] == (LIGHTNESS_SET & 0xFF) &&
			data[3] == ((LIGHTNESS_SET >> 8) & 0xFF))
		{
			uint16_t raw_dim = data[5] | (data[6] << 8);
			temp_dim = RawDimToValue(raw_dim, key);
		}
		else if (len <= 5)
		{
			temp_dim = RawDimToValue(data_message->dim_first, key);
		}
		else
		{
			temp_dim = RawDimToValue(data_message->dim, key);
		}
		if (temp_dim != dim)
		{
			dim = temp_dim;
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

bool ModuleDim::CheckData(Json::Value &dataValue, bool &rs)
{
	LOGV("CheckData data: %s", dataValue.toString().c_str());
	if (dataValue.isObject() &&
		dataValue.isMember(key) &&
		dataValue.isMember("op") && dataValue["op"].isString())
	{
		string op = dataValue["op"].asString();
		if (dataValue[key].isInt())
		{
			int dim = dataValue[key].asInt();
			rs = Util::CompareNumber(op, this->dim, dim);
			return true;
		}
		else if (dataValue[key].isArray())
		{
			Json::Value listValue = dataValue[key];
			if (listValue.size() == 2 && listValue[0].isInt() && listValue[1].isInt())
			{
				int dim1 = listValue[0].asInt();
				int dim2 = listValue[1].asInt();
				rs = Util::CompareNumber(op, this->dim, dim1, dim2);
				return true;
			}
		}
	}
	return false;
}

void ModuleDim::BuildTelemetryValue(Json::Value &jsonValue)
{
	jsonValue[key] = dim;
	if (dim > 0 && !onoffKey.empty())
	{
		if (UsesBoolOnOffTelemetry(onoffKey))
		{
			jsonValue[onoffKey] = true;
		}
		else
		{
			jsonValue[onoffKey] = 1;
		}
	}

	device->UpdatePropertyJsonUpdate(jsonValue);
}

int ModuleDim::Do(Json::Value &dataValue)
{
	LOGV("Do data: %s", dataValue.toString().c_str());
	if (dataValue.isObject() &&
		dataValue.isMember(key) && dataValue[key].isInt())
	{
		int dim = dataValue[key].asInt();
		if (BleProtocol::getInstance()->SetDimmingLight(addr, DimValueToRaw(dim, key), TRANSITION_DEFAULT, true) == CODE_OK)
		{
			return CODE_OK;
		}
	}
	return CODE_ERROR;
}

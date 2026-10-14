#include "ModuleOnOff.h"
#include "Log.h"
#include "Util.h"
#include "Device.h"
#include "BleProtocol.h"
#include "BleOpCode.h"
#include "Database.h"

ModuleOnOff::ModuleOnOff(Device *device, uint16_t addr, string onoffKey, uint32_t index) : Module(device, addr, index)
{
	onoff = 0;
	if (onoffKey == KEY_ATTRIBUTE_ONOFF)
	{
		// New-core switch schema: first button is `bt`, followed by bt2,
		// bt3... (there is deliberately no bt1).
		key = onoffKey + to_string(index);
	}
	else if (onoffKey.empty())
	{
		// Preserve numeric/internal keys used by legacy curtain and multi-on/off
		// devices until those protocols are migrated independently.
		key = to_string(index + 1);
	}
	else
		key = onoffKey;
}

ModuleOnOff::~ModuleOnOff()
{
}

#ifdef CONFIG_SAVE_ATTRIBUTE
void ModuleOnOff::InitAttribute(string attribute, double value)
{
	if (attribute == key)
		onoff = value;
}

void ModuleOnOff::SaveAttribute()
{
	Database::getInstance()->DeviceAttributeAdd(device, key, onoff);
}
#endif

int ModuleOnOff::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
	LOGE("ModuleOnOff::InputData, key: %s, dataValue: %s", key.c_str(), dataValue.toString().c_str());
	// if (dataValue.isObject() &&
	// 	dataValue.isMember(key) && dataValue[key].isInt())
	// {
	// 	onoff = dataValue[key].asInt();
	// 	BuildTelemetryValue(jsonValue);
	// 	return CODE_OK;
	// }
	return CODE_ERROR;
}

int ModuleOnOff::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
	if (!data || len < 3)
		return CODE_ERROR;
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
			// LOGI("ModuleOnOff::InputData parsing G_ONOFF_STATUS, len=%d", len);
			// Log raw data for verification
			char raw_hex[64] = {0};
			int log_len = (len < 10 ? len : 10);
			for (int hh = 0; hh < log_len; hh++)
				sprintf(raw_hex + hh * 3, "%02X ", data[hh]);
			LOGI("Raw data[0..%d]: %s", log_len - 1, raw_hex);

			if (len == 3)
			{
				temp_onoff = data_message->state;
				LOGI("G_ONOFF_STATUS received len=3 state=%d", data_message->state);
			}
			else if (len >= 5)
			{
				// Extended status frames may include padding after the state.
				// The numeric state remains at data[4].
				// Curtain switches use 0/1/2 instead of a boolean state.
				temp_onoff = data[4];
				// LOGI("G_ONOFF_STATUS received len=%d, extracted temp_onoff=%d from data[4]=%02X", len, temp_onoff, data[4]);
			}
			else
			{
				return CODE_ERROR;
			}
			// else
			// {
			// 	temp_onoff = data_message->onoff;
			// 	LOGI("G_ONOFF_STATUS received len=%d onoff=%d", len, data_message->onoff);
			// }
			if (temp_onoff != onoff)
			{
				onoff = temp_onoff;
#ifdef CONFIG_SAVE_ATTRIBUTE
				SaveAttribute();
#endif
				LOGI("G_ONOFF_STATUS updated module onoff=%d", onoff);
			}
			else
			{
				// LOGD("G_ONOFF_STATUS no change onoff=%d", onoff);
			}
			BuildTelemetryValue(jsonValue);

			CheckTrigger(jsonValue);
			return CODE_OK;
		}
	}
	if (data[0] == RD_OPCODE_CONFIG_RSP)
	{
		typedef struct __attribute__((packed))
		{
			uint8_t opcodeVendor;
			uint16_t vendorId;
			uint16_t header;
			uint8_t data[100];
		} data_message_t;
		data_message_t *data_message = (data_message_t *)data;
		if (data_message->vendorId == RD_VENDOR_ID)
		{
			if (data_message->header == RD_HEADER_CONFIG_CONTROL_RELAY_SWITCH_4)
			{
				temp_onoff = data_message->data[index + 1];
				if (temp_onoff != onoff)
				{
					onoff = temp_onoff;
#ifdef CONFIG_SAVE_ATTRIBUTE
					SaveAttribute();
#endif
				}
				BuildTelemetryValue(jsonValue);
				LOGI("jsonValue after BuildTelemetryValue: %s", jsonValue.toString().c_str());
				CheckTrigger(jsonValue);
				return CODE_OK;
			}
		}
	}
	return CODE_ERROR;
}

bool ModuleOnOff::CheckData(Json::Value &dataValue, bool &rs)
{
	LOGV("CheckData data: %s", dataValue.toString().c_str());
	if (dataValue.isArray() && dataValue[0].asString() == key)
	{
		string op = dataValue[1].asString();
		LOGD("op: %s", op.c_str());
		if (dataValue[2].isBool())
		{
			bool onoff = dataValue[2].asBool();
			rs = Util::CompareNumber(op, this->onoff, onoff);
			return true;
		}
	}
	return false;
}

void ModuleOnOff::BuildTelemetryValue(Json::Value &jsonValue)
{
	jsonValue[key] = onoff;
	device->UpdatePropertyJsonUpdate(jsonValue);
}

int ModuleOnOff::Do(Json::Value &dataValue)
{
	LOGI("ModuleOnOff::Do, key: %s, data: %s", key.c_str(), dataValue.toString().c_str());
	if (dataValue.isObject() &&
		dataValue.isMember(key))
	{
		const Json::Value &value = dataValue[key];
		LOGI("ModuleOnOff::Do found key %s, isBool: %d, isInt: %d",
			 key.c_str(), value.isBool(), value.isInt());
		if (value.isBool() || value.isInt())
		{
			int onoff = value.isBool() ? static_cast<int>(value.asBool())
									   : value.asInt();
			if (onoff < 0 || onoff > 2)
				return CODE_ERROR;

			LOGI("ModuleOnOff::Do invoking BleProtocol::SetOnOffLight for addr: 0x%04X, onoff: %d", addr, onoff);
			int rs = BleProtocol::getInstance()->SetOnOffLight(addr, onoff, TRANSITION_DEFAULT, true);
			LOGI("BleProtocol::SetOnOffLight returned: %d", rs);
			if (rs == CODE_OK)
			{
				return CODE_OK;
			}
		}
	}
	return CODE_ERROR;
}

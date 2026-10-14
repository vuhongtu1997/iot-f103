#include "ModuleSensiPir.h"
#include "Log.h"
#include "Util.h"
#include "Device.h"
#include "BleProtocol.h"
#include "Database.h"

ModuleSensiPir::ModuleSensiPir(Device *device, uint32_t addr) : Module(device, addr)
{
    sensi = 0;
}

ModuleSensiPir::~ModuleSensiPir()
{
}

int ModuleSensiPir::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
    if (dataValue.isObject() && dataValue.isMember(KEY_ATTRIBUTE_SENSI) && dataValue[KEY_ATTRIBUTE_SENSI].isInt())
    {
        sensi = dataValue[KEY_ATTRIBUTE_SENSI].asInt();
        BuildTelemetryValue(jsonValue);
        return CODE_OK;
    }
    return CODE_ERROR;
}

int ModuleSensiPir::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
    typedef struct __attribute__((packed))
    {
        uint8_t opcode;
        uint16_t vendorId;
        uint16_t header;
        uint8_t sensi;
    } data_message_t;
    data_message_t *data_message = (data_message_t *)data;
    if (data_message->opcode == RD_OPCODE_CONFIG_RSP &&
        data_message->vendorId == RD_VENDOR_ID &&
        data_message->header == RD_HEADER_CONFIG_SET_SENSI_PIR_LIGHT_SENSOR)
    {
        sensi = data_message->sensi;
        BuildTelemetryValue(jsonValue);
        CheckTrigger(jsonValue);
        return CODE_OK;
    }
    return CODE_ERROR;
}

bool ModuleSensiPir::CheckData(Json::Value &dataValue, bool &rs)
{
    LOGD("CheckData data: %s", dataValue.toString().c_str());
	if (dataValue.isObject() &&
		dataValue.isMember(KEY_ATTRIBUTE_SENSI) &&
		dataValue.isMember("op") && dataValue["op"].isString())
	{
		string op = dataValue["op"].asString();
		if (dataValue[KEY_ATTRIBUTE_SENSI].isInt())
		{
			int value = dataValue[KEY_ATTRIBUTE_SENSI].asInt();
			rs = Util::CompareNumber(op, this->sensi, value);
			return true;
		}
		else if (dataValue[KEY_ATTRIBUTE_SENSI].isArray())
		{
			Json::Value listValue = dataValue[KEY_ATTRIBUTE_SENSI];
			if (listValue.size() == 2 && listValue[0].isInt() && listValue[1].isInt())
			{
				int value1 = listValue[0].asInt();
				int value2 = listValue[1].asInt();
				rs = Util::CompareNumber(op, this->sensi, value1, value2);
				return true;
			}
		}
	}
	return false;
}

void ModuleSensiPir::BuildTelemetryValue(Json::Value &jsonValue)
{
    jsonValue[KEY_ATTRIBUTE_SENSI] = sensi;
}

int ModuleSensiPir::Do(Json::Value &dataValue)
{
    LOGV("ModuleSensiPir Do data: %s", dataValue.toString().c_str());
    if (dataValue.isObject() &&
        dataValue.isMember(KEY_ATTRIBUTE_SENSI) && dataValue[KEY_ATTRIBUTE_SENSI].isInt())
    {
        int sensi = dataValue[KEY_ATTRIBUTE_SENSI].asInt();
        if (BleProtocol::getInstance()->SetSensiPirLightSensor(addr, sensi) == CODE_OK)
        {
            this->sensi = sensi;
            return CODE_OK;
        }
    }
    return CODE_ERROR;
}

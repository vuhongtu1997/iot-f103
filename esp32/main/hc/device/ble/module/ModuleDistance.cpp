#include "ModuleDistance.h"
#include "Log.h"
#include "Util.h"
#include "Device.h"
#include "BleProtocol.h"
#include "Database.h"

ModuleDistance::ModuleDistance(Device *device, uint32_t addr) : Module(device, addr)
{
    distance = 0;
}

ModuleDistance::~ModuleDistance()
{
}

#ifdef CONFIG_SAVE_ATTRIBUTE
void ModuleDistance::InitAttribute(string attribute, double value)
{
    if (attribute == KEY_ATTRIBUTE_DISTANCE)
        distance = value;
}

void ModuleDistance::SaveAttribute()
{
    Database::getInstance()->DeviceAttributeAdd(device, KEY_ATTRIBUTE_DISTANCE, distance);
}
#endif

int ModuleDistance::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
    if (dataValue.isObject() &&
        dataValue.isMember(KEY_ATTRIBUTE_DISTANCE) && dataValue[KEY_ATTRIBUTE_DISTANCE].isInt())
    {
        distance = dataValue[KEY_ATTRIBUTE_DISTANCE].asInt();
        BuildTelemetryValue(jsonValue);
        return CODE_OK;
    }
    return CODE_ERROR;
}

int ModuleDistance::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
    typedef struct __attribute__((packed))
    {
        uint8_t opcode;
        uint16_t vendorId;
        uint16_t header;
        uint8_t distance;
    } data_message_t;
    data_message_t *data_message = (data_message_t *)data;
    if (data_message->opcode == RD_OPCODE_CONFIG_RSP)
    {
        if (data_message->header == RD_HEADER_CONFIG_SET_DISTANCE_RADA_SENSOR)
        {
            if (distance != data_message->distance)
            {
                distance = data_message->distance;
#ifdef CONFIG_SAVE_ATTRIBUTE
                SaveAttribute();
#endif
            }
            BuildTelemetryValue(jsonValue);
            CheckTrigger(jsonValue);
            return CODE_OK;
        }
    }
    return CODE_ERROR;
}

bool ModuleDistance::CheckData(Json::Value &dataValue, bool &rs)
{
    LOGV("CheckData data: %s", dataValue.toString().c_str());
    if (dataValue.isObject() &&
        dataValue.isMember(KEY_ATTRIBUTE_DISTANCE) &&
        dataValue.isMember("op") && dataValue["op"].isString())
    {
        string op = dataValue["op"].asString();
        if (dataValue[KEY_ATTRIBUTE_DISTANCE].isInt())
        {
            int value = dataValue[KEY_ATTRIBUTE_DISTANCE].asInt();
            rs = Util::CompareNumber(op, this->distance, value);
            return true;
        }
        else if (dataValue[KEY_ATTRIBUTE_DISTANCE].isArray())
        {
            Json::Value listValue = dataValue[KEY_ATTRIBUTE_DISTANCE];
            if (listValue.size() == 2 && listValue[0].isInt() && listValue[1].isInt())
            {
                int value1 = listValue[0].asInt();
                int value2 = listValue[1].asInt();
                rs = Util::CompareNumber(op, this->distance, value1, value2);
                return true;
            }
        }
    }
    return false;
}

void ModuleDistance::BuildTelemetryValue(Json::Value &jsonValue)
{
    jsonValue[KEY_ATTRIBUTE_DISTANCE] = distance;
}

int ModuleDistance::Do(Json::Value &dataValue)
{
    if (dataValue.isObject() &&
        dataValue.isMember(KEY_ATTRIBUTE_DISTANCE) && dataValue[KEY_ATTRIBUTE_DISTANCE].isInt())
    {
        int value = dataValue[KEY_ATTRIBUTE_DISTANCE].asInt();
        if (BleProtocol::getInstance()->SetDistanceSensor(addr, value) == CODE_OK)
        {
            return CODE_OK;
        }
    }
    return CODE_ERROR;
}

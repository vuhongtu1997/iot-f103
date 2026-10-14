#include "ModuleButtonAll.h"
#include "Log.h"
#include "Util.h"
#include "Device.h"
#include "BleProtocol.h"
#include "Database.h"

ModuleButtonAll::ModuleButtonAll(Device *device, uint16_t addr, uint32_t index) : Module(device, addr, index)
{
    this->index = index;
    bt = new uint8_t[index];
    key = new std::string[index];
    for (uint8_t i = 0; i < index; i++)
    {
        bt[i] = 0;
        key[i] = KEY_ATTRIBUTE_BUTTON + (i ? std::to_string(i + 1) : "");
    }
}

ModuleButtonAll::~ModuleButtonAll()
{
    delete[] bt;
    delete[] key;
}

#ifdef CONFIG_SAVE_ATTRIBUTE
void ModuleButtonAll::InitAttribute(string attribute, double value)
{
    for (uint8_t i = 0; i < index; i++)
    {
        if (attribute == key[i])
            bt[i] = value;
    }
}

void ModuleButtonAll::SaveAttribute()
{
    for (uint8_t i = 0; i < index; i++)
    {
        Database->getInstance()->DeviceAttributeAdd(device, key[i], bt[i]);
    }
}
#endif

int ModuleButtonAll::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
    typedef struct __attribute__((packed))
    {
        uint8_t opcodeVendor;
        uint16_t vendorId;
        uint16_t header;
        uint8_t numBt;
        uint8_t data[100];
    } data_message_t;
    data_message_t *data_message = (data_message_t *)data;
    if (data_message->opcodeVendor == RD_OPCODE_CONFIG_RSP)
    {
        if ((data_message->header == RD_HEADER_CONFIG_STATUS_ALL_RELAY_SWITCH) ||
            (data_message->header == RD_HEADER_CONFIG_CONTROL_RELAY_SWITCH_4))
        {
            if (data_message->numBt == index)
            {
                for (uint8_t i = 0; i < index; i++)
                {
                    bt[i] = data_message->data[i];
                }
                BuildTelemetryValue(jsonValue);
                CheckTrigger(jsonValue);
                return CODE_OK;
            }
        }
    }
    return CODE_ERROR;
}

bool ModuleButtonAll::CheckData(Json::Value &dataValue, bool &rs)
{
    LOGV("CheckData data: %s", dataValue.toString().c_str());
    for (uint8_t i = 0; i < index; i++)
    {
        if (dataValue.isObject() &&
            dataValue.isMember(key[i]) &&
            dataValue.isMember("op") && dataValue["op"].isString())
        {
            string op = dataValue["op"].asString();
            if (dataValue[key[i]].isInt())
            {
                int bt = dataValue[key[i]].asInt();
                rs = Util::CompareNumber(op, this->bt[i], bt);
                return true;
            }
            else if (dataValue[key[i]].isArray())
            {
                Json::Value listValue = dataValue[key[i]];
                if (listValue.size() == 2 && listValue[0].isInt() && listValue[1].isInt())
                {
                    int bt1 = listValue[0].asInt();
                    int bt2 = listValue[1].asInt();
                    rs = Util::CompareNumber(op, this->bt[i], bt1, bt2);
                    return true;
                }
            }
        }
    }
    return false;
}

void ModuleButtonAll::BuildTelemetryValue(Json::Value &jsonValue)
{
    for (uint8_t i = 0; i < index; i++)
    {
        jsonValue[key[i]] = bt[i];
    }
    device->UpdatePropertyJsonUpdate(jsonValue);
}

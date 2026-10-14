#include "ModuleRawData.h"
#include "Log.h"
#include "Util.h"
#include "Device.h"
#include "BleProtocol.h"
#include "BleOpCode.h"
#include <cctype>
#include <vector>

namespace
{
int HexDigitToValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

std::vector<uint8_t> HexStringToBytes(const string &value)
{
    string hex;
    hex.reserve(value.size());

    for (size_t i = 0; i < value.size(); i++)
    {
        unsigned char c = static_cast<unsigned char>(value[i]);
        if (isspace(c) || value[i] == ':' || value[i] == '-' || value[i] == ',')
            continue;
        if (value[i] == '0' && i + 1 < value.size() && (value[i + 1] == 'x' || value[i + 1] == 'X'))
        {
            i++;
            continue;
        }
        hex.push_back(value[i]);
    }

    if (hex.empty() || hex.size() % 2 != 0)
        return {};

    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2)
    {
        int high = HexDigitToValue(hex[i]);
        int low = HexDigitToValue(hex[i + 1]);
        if (high < 0 || low < 0)
            return {};
        bytes.push_back(static_cast<uint8_t>((high << 4) | low));
    }
    return bytes;
}
}

ModuleRawData::ModuleRawData(Device *device, uint16_t addr) : Module(device, addr)
{
    key = "106";
}

ModuleRawData::~ModuleRawData()
{
}

int ModuleRawData::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
    // Implementation for InputData with Json::Value
    return 0;
}

int ModuleRawData::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
    // Implementation for InputData with raw data
    return 0;
}

int ModuleRawData::Do(Json::Value &dataValue)
{
    // Implementation for Do function
    if (dataValue.isObject() && dataValue.isMember(key))
    {
        if (dataValue[key].isString())
        {
            string value = dataValue[key].asString();
            auto data = HexStringToBytes(value);
            if (data.empty())
            {
                LOGE("ModuleRawData::Do invalid hex string: %s", value.c_str());
                return CODE_ERROR;
            }
            return BleProtocol::getInstance()->SendRawData(data.data(), static_cast<int>(data.size()));
        }
    }
    return CODE_ERROR;
}

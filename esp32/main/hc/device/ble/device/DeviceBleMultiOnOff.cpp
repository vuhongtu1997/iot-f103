#include "DeviceBleMultiOnOff.h"
#include "BleProtocol.h"
#include "Log.h"
#include <algorithm>
#include <cctype>
#include <vector>

namespace
{
int HexDigitToValue(char value)
{
  if (value >= '0' && value <= '9')
    return value - '0';
  if (value >= 'a' && value <= 'f')
    return value - 'a' + 10;
  if (value >= 'A' && value <= 'F')
    return value - 'A' + 10;
  return -1;
}

bool ParseHexFrame(const string &value, std::vector<uint8_t> &frame)
{
  if (value.size() != 38)
    return false;

  frame.clear();
  frame.reserve(19);
  for (size_t index = 0; index < value.size(); index += 2)
  {
    int high = HexDigitToValue(value[index]);
    int low = HexDigitToValue(value[index + 1]);
    if (high < 0 || low < 0)
      return false;
    frame.push_back(static_cast<uint8_t>((high << 4) | low));
  }
  return true;
}

bool ParsePositiveSeconds(const Json::Value &value, uint16_t &seconds)
{
  int parsed = 0;
  if (value.isInt())
  {
    parsed = value.asInt();
  }
  else if (value.isString())
  {
    const string text = value.asString();
    if (text.empty())
      return false;
    for (char ch : text)
    {
      if (!std::isdigit(static_cast<unsigned char>(ch)))
        return false;
      parsed = parsed * 10 + (ch - '0');
      if (parsed > UINT16_MAX)
        return false;
    }
  }
  else
  {
    return false;
  }

  if (parsed <= 0 || parsed > UINT16_MAX)
    return false;
  seconds = static_cast<uint16_t>(parsed);
  return true;
}
}

DeviceBleMultiOnOff::DeviceBleMultiOnOff(string id, string name, string mac,
                                         string type, uint16_t addr,
                                         uint16_t version,
                                         Json::Value *dataJson,
                                         uint8_t elementCount)
    : DeviceBle(id, name, mac, type, addr, version, dataJson)
{
  lastCurtainTime = -1;
  countElement = elementCount;
  for (uint8_t index = 0; index < elementCount; index++)
  {
    uint16_t elementAddr = addr + index;
    modules.push_back(new ModuleOnOff(this, elementAddr, "", index));
  }
  powerSource = POWER_AC;
}

int DeviceBleMultiOnOff::Do(Json::Value &dataValue)
{
  if (type != "HG_BLE_CURTAIN_DOUBLE" || !dataValue.isObject() ||
      !dataValue.isMember("106"))
  {
    return DeviceBle::Do(dataValue);
  }

  const Json::Value &value = dataValue["106"];
  uint16_t seconds = 0;
  std::vector<uint8_t> frame;

  if (value.isString() && ParseHexFrame(value.asString(), frame))
  {
    static const uint8_t command[] = {
        0xE0, 0x11, 0x02, 0x00, 0x00, 0xE0, 0x00,
    };
    uint16_t frameAddr =
        (static_cast<uint16_t>(frame[8]) << 8) | frame[9];

    if (frame[0] != 0xE8 || frame[1] != 0xFF || frameAddr != addr ||
        !std::equal(command, command + sizeof(command), frame.begin() + 10))
    {
      LOGE("Invalid curtain-time frame for addr=0x%04X", addr);
      return CODE_ERROR;
    }
    seconds = static_cast<uint16_t>(frame[17]) |
              (static_cast<uint16_t>(frame[18]) << 8);
  }
  else if (!ParsePositiveSeconds(value, seconds))
  {
    LOGE("Invalid curtain time: %s", value.toString().c_str());
    return CODE_ERROR;
  }

  if (seconds == 0)
    return CODE_ERROR;
  if (lastCurtainTime == seconds)
  {
    LOGI("Curtain time already set to %u seconds, skip duplicate", seconds);
    return CODE_OK;
  }

  int rs = BleProtocol::getInstance()->SetCurtainTime(addr, seconds);
  if (rs == CODE_OK)
    lastCurtainTime = seconds;
  return rs;
}

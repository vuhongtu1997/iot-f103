#include "ModuleCurtain.h"
#include "BleOpCode.h"
#include "BleProtocol.h"
#include "Device.h"
#include "Log.h"
#include "Util.h"
#include <cctype>

namespace
{
constexpr uint16_t NEW_CURTAIN_STATUS = 0x0A52;

bool IsLegacySingleCurtain(Device *device)
{
  if (!device)
    return false;
  if (device->GetType() == "HG_BLE_CURTAIN_OLD")
    return true;

  const Json::Value data = device->GetData();
  string pid = data.get("pid", "").asString();
  if (pid.empty() && data.isMember("devices") && data["devices"].isString())
  {
    vector<string> tokens = Util::splitString(data["devices"].asString(), '|');
    if (tokens.size() > 1)
      pid = tokens[1];
  }
  return pid == "BLEHGAA0105";
}

bool ParseUint8(const Json::Value &value, uint8_t &parsed)
{
  int number = 0;
  if (value.isInt())
  {
    number = value.asInt();
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
      number = number * 10 + (ch - '0');
      if (number > UINT8_MAX)
        return false;
    }
  }
  else
  {
    return false;
  }

  if (number < 0 || number > UINT8_MAX)
    return false;
  parsed = static_cast<uint8_t>(number);
  return true;
}
}

ModuleCurtain::ModuleCurtain(Device *device, uint16_t addr) : Module(device, addr)
{
  status = 0;
  position = 0;
  hasPosition = false;
  travelTime = -1;
  curtainType = -1;
  direction = -1;
}

ModuleCurtain::~ModuleCurtain() {}

int ModuleCurtain::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
  if (!dataValue.isObject())
    return CODE_ERROR;

  bool updated = false;
  if (dataValue.isMember("1") && dataValue["1"].isInt())
  {
    status = dataValue["1"].asInt();
    updated = true;
  }
  if (dataValue.isMember("2") && dataValue["2"].isInt())
  {
    position = dataValue["2"].asInt();
    hasPosition = true;
    updated = true;
  }

  if (!updated)
    return CODE_ERROR;

  BuildTelemetryValue(jsonValue);
  return CODE_OK;
}

int ModuleCurtain::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
  if (!data || len < 3)
    return CODE_ERROR;

  uint16_t opcode = data[0] | (data[1] << 8);
  bool updated = false;

  if (opcode == G_ONOFF_STATUS)
  {
    int nextStatus = status;
    if (len == 3)
      nextStatus = data[2];
    else if (len >= 5)
      nextStatus = data[4];
    else
      return CODE_ERROR;

    LOGI("ModuleCurtain G_ONOFF_STATUS len=%d status=%d", len, nextStatus);
    updated = nextStatus != status;
    status = nextStatus;
  }
  else if (opcode == NEW_CURTAIN_STATUS && len >= 5)
  {
    // New-curtain status: param[1] = open/close, param[2] = position.
    int nextStatus = data[3];
    int nextPosition = data[4];

    LOGI("ModuleCurtain 52 0A status len=%d status=%d position=%d", len,
         nextStatus, nextPosition);
    updated = nextStatus != status || !hasPosition || nextPosition != position;
    status = nextStatus;
    position = nextPosition;
    hasPosition = true;
  }
  else if (data[0] == 0xE0 && data[1] == 0x11 && len >= 6 &&
           data[2] == 0x02 && data[4] == 0x00)
  {
    if (data[3] == 0x17)
    {
      travelTime = data[5];
      LOGI("ModuleCurtain travel time response=%d", travelTime);
    }
    else if (data[3] == 0xE0)
    {
      curtainType = data[5];
      LOGI("ModuleCurtain type response=%d", curtainType);
    }
    else if (data[3] == 0xE2 && data[5] <= 1)
    {
      direction = data[5];
      LOGI("ModuleCurtain direction response=%s",
           direction == 0 ? "forward" : "backward");
    }
    else
    {
      return CODE_ERROR;
    }
    updated = true;
  }
  else
  {
    return CODE_ERROR;
  }

  if (updated)
  {
    LOGI("ModuleCurtain updated status=%d position=%d hasPosition=%d", status,
         position, hasPosition);
  }
  else
  {
    LOGD("ModuleCurtain no change status=%d position=%d hasPosition=%d", status,
         position, hasPosition);
  }

  BuildTelemetryValue(jsonValue);
  CheckTrigger(jsonValue);
  return CODE_OK;
}

bool ModuleCurtain::CheckData(Json::Value &dataValue, bool &rs)
{
  if (!dataValue.isArray() || dataValue.size() < 3)
    return false;

  string key = dataValue[0].asString();
  if (key != "1" && key != "2")
    return false;

  string op = dataValue[1].asString();
  if (!dataValue[2].isInt())
    return false;

  int current = key == "1" ? status : position;
  rs = Util::CompareNumber(op, current, dataValue[2].asInt());
  return true;
}

void ModuleCurtain::BuildTelemetryValue(Json::Value &jsonValue)
{
  jsonValue["1"] = status;
  if (hasPosition)
    jsonValue["2"] = position;
  if (direction >= 0)
    jsonValue["120"] = direction == 0 ? "forward" : "backward";
  if (curtainType >= 0)
    jsonValue["121"] = to_string(curtainType);
  if (travelTime >= 0)
    jsonValue["122"] = to_string(travelTime);
  device->UpdatePropertyJsonUpdate(jsonValue);
}

int ModuleCurtain::Do(Json::Value &dataValue)
{
  if (!dataValue.isObject())
    return CODE_ERROR;

  int nextStatus = 0;
  int nextPosition = 0;
  bool hasCommand = false;
  bool hasConfig = false;
  int result = CODE_OK;

  if (dataValue.isMember("1") && dataValue["1"].isInt())
  {
    nextStatus = dataValue["1"].asInt();
    if (nextStatus < 0 || nextStatus > 2)
      return CODE_ERROR;
    hasCommand = true;
  }

  if (dataValue.isMember("2") && dataValue["2"].isInt())
  {
    nextPosition = dataValue["2"].asInt();
    if (nextPosition < 0 || nextPosition > 100)
      return CODE_ERROR;
    hasCommand = true;
  }

  if (dataValue.isMember("120"))
  {
    if (!dataValue["120"].isString())
      return CODE_ERROR;

    string value = dataValue["120"].asString();
    uint8_t nextDirection = 0;
    if (value == "forward")
      nextDirection = 0;
    else if (value == "backward")
      nextDirection = 1;
    else
      return CODE_ERROR;

    result = BleProtocol::getInstance()->SetCurtainDirection(addr, nextDirection);
    if (result != CODE_OK)
      return result;
    direction = nextDirection;
    hasConfig = true;
  }

  if (dataValue.isMember("121"))
  {
    uint8_t nextType = 0;
    if (!ParseUint8(dataValue["121"], nextType))
      return CODE_ERROR;

    result = BleProtocol::getInstance()->SetCurtainType(addr, nextType);
    if (result != CODE_OK)
      return result;
    curtainType = nextType;
    hasConfig = true;
  }

  if (dataValue.isMember("122"))
  {
    uint8_t nextTravelTime = 0;
    if (!ParseUint8(dataValue["122"], nextTravelTime) || nextTravelTime == 0)
      return CODE_ERROR;

    result =
        BleProtocol::getInstance()->SetCurtainTravelTime(addr, nextTravelTime);
    if (result != CODE_OK)
      return result;
    travelTime = nextTravelTime;
    hasConfig = true;
  }

  if (dataValue.isMember(KEY_ATTRIBUTE_CURTAINS_TIME))
  {
    uint8_t nextTravelTime = 0;
    if (!ParseUint8(dataValue[KEY_ATTRIBUTE_CURTAINS_TIME], nextTravelTime) ||
        nextTravelTime == 0)
    {
      LOGW("ModuleCurtain invalid curtainsTime: %s",
           dataValue[KEY_ATTRIBUTE_CURTAINS_TIME].toString().c_str());
      return CODE_ERROR;
    }

    LOGI("ModuleCurtain sending curtainsTime: addr=0x%04X time=%u", addr,
         nextTravelTime);
    result =
        BleProtocol::getInstance()->SetCurtainTravelTime(addr, nextTravelTime);
    if (result != CODE_OK)
      return result;
    travelTime = nextTravelTime;
    hasConfig = true;
  }

  if (!hasCommand && !hasConfig)
    return CODE_ERROR;

  if (hasCommand)
  {
    LOGI("ModuleCurtain::Do addr=0x%04X status=%d position=%d", addr,
         nextStatus, nextPosition);
    if (IsLegacySingleCurtain(device))
      result = BleProtocol::getInstance()->ControlLegacyCurtain(
          addr, static_cast<uint8_t>(nextStatus));
    else
      result = BleProtocol::getInstance()->ControlCurtain(
          addr, static_cast<uint8_t>(nextStatus),
          static_cast<uint8_t>(nextPosition));
  }
  return result;
}

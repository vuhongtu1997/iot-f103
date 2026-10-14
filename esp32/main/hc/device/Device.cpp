#include "Device.h"
#include "Gateway.h"
#include "Log.h"
#include "Util.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <string.h>
#include <unistd.h>

Device::Device(string id, string name, string mac, string type, uint16_t addr,
               uint16_t version, Json::Value *dataJson)
    : Object(id, addr, name)
{
  this->mac = mac;
  this->type = type;
  if (dataJson)
  {
    this->dataJson = *dataJson;
  }
  else
  {
    this->dataJson = Json::objectValue;
  }
  this->version = version;
  powerSource = POWER_UNKNOWN;
  this->isFavorite = false;
  propertyJsonUpdate = Json::objectValue;

  lastOnlineState = false;
  lastTimeActive = time(NULL);
  lastTimeCheckActive = 0;
}

Device::~Device() {}

string Device::GetMac() { return mac; }

Json::Value Device::GetData() { return dataJson; }

void Device::SetData(Json::Value &dataValue) { dataJson = dataValue; }

string Device::GetCloudId()
{
  string cloudId;
  if (dataJson.isObject() && dataJson.isMember("device_id") &&
      dataJson["device_id"].isString())
  {
    cloudId = dataJson["device_id"].asString();
  }
  else if (dataJson.isObject() && dataJson.isMember("id") &&
           dataJson["id"].isString())
  {
    cloudId = dataJson["id"].asString();
  }
  else if (!GetMac().empty())
  {
    cloudId = GetMac();
  }
  else
  {
    cloudId = GetId();
  }

  std::transform(cloudId.begin(), cloudId.end(), cloudId.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return cloudId;
}

int Device::GetPageIndex()
{
  if (dataJson.isObject() && dataJson.isMember("pageIndex") &&
      dataJson["pageIndex"].isInt())
  {
    return dataJson["pageIndex"].asInt();
  }
  return 1;
}

string Device::GetDeviceKey() { return ""; }

string Device::GetType() { return type; }

uint16_t Device::GetVersion() { return version; }

string Device::GetVersionStr()
{
  return to_string((version >> 8) & 0xFF) + "." + to_string(version & 0xFF);
}

int Device::GetRSSI() { return rssi; }

void Device::SetRSSI(int rssi) { this->rssi = rssi; }

bool Device::IsSensitive() { return sensitive; }

void Device::SetSensitive(bool isSensitive) { this->sensitive = isSensitive; }

protocol_e Device::GetProtocol() { return protocol; }

bool Device::IsOnline() { return lastOnlineState; }

bool Device::IsNeedCheckOnline() { return powerSource == POWER_AC; }

#if 1 // Rule feature disabled
void Device::RegisterTrigger(RuleInputDevice *ruleInputDevice)
{
  LOGD("RegisterTrigger");
  deviceRuleInputList.push_back(ruleInputDevice);
}

void Device::UnregisterTrigger(RuleInputDevice *ruleInputDevice)
{
  LOGD("UnregisterTrigger");
  deviceRuleInputList.erase(remove(deviceRuleInputList.begin(), deviceRuleInputList.end(), ruleInputDevice), deviceRuleInputList.end());
}
#endif

int Device::BuildAttributeValue(Json::Value &pushDataValue)
{
  Json::Value deviceData;
  deviceData["gateway"] = "Farm Gateway RAL";
  deviceData["name"] = name;
  deviceData["mac"] = mac;
  // deviceData["type"] = (int)type;
  pushDataValue[id] = deviceData;
  return CODE_OK;
}

void Device::DeviceInputData(uint8_t *data, int len, uint16_t addr)
{
  InputData(data, len, addr);
}

void Device::UpdateLastTimeActive() { lastTimeActive = time(NULL); }

void Device::CheckTrigger()
{
  LOGV("CheckTrigger");
#if 1 // Rule feature disabled
  bool rs;
  for (auto &ruleInputDevice : deviceRuleInputList)
  {
    rs = false;
    if (CheckData(*ruleInputDevice->GetData(), rs))
      ruleInputDevice->Trigger(rs);
  }
#endif
}

int Device::DoJsonArray(Json::Value &dataValue)
{
  if (dataValue.isArray())
  {
    for (Json::ArrayIndex i = 0; i < dataValue.size(); i++)
    {
      Do(dataValue[i]);
    }
  }
  else
  {
    Do(dataValue);
  }
  return CODE_OK;
}

void Device::SetPropertyJsonUpdate(Json::Value property)
{
  if (this->propertyJsonUpdate != property)
  {
    PushTelemetry(property);
    this->propertyJsonUpdate = property;
  }
}

void Device::UpdatePropertyJsonUpdate(Json::Value &propertyUpdate)
{
  if (propertyUpdate.isObject() && this->propertyJsonUpdate.isObject())
  {
    for (auto const &key : propertyUpdate.getMemberNames())
    {
      if (this->propertyJsonUpdate.isMember(key))
      {
        if (propertyUpdate[key].type() ==
            this->propertyJsonUpdate[key].type())
        {
          this->propertyJsonUpdate[key] = propertyUpdate[key];
        }
      }
    }
  }
}

Json::Value Device::GetPropertyJsonUpdate() { return this->propertyJsonUpdate; }

int Device::PushAttribute()
{
  Json::Value jsonValue;
  int build = BuildAttributeValue(jsonValue);
  if (build == 0)
  {
    // return Gateway::getInstance()->DeviceAttribute(this, jsonValue);
  }
  return CODE_ERROR;
}

int Device::PushAttribute(Json::Value &jsonValue)
{
  // return Gateway::getInstance()->DeviceAttribute(this, jsonValue);
  return CODE_ERROR;
}

int Device::PushTelemetry()
{
  Json::Value jsonValue;
  Json::Value devValue;
  Json::Value telemetryValue;
  int build = BuildTelemetryValue(telemetryValue);
  if (build == 0)
  {
    return PushTelemetry(telemetryValue);
  }
  return CODE_ERROR;
}

int Device::PushTelemetry(Json::Value &telemetryValue)
{
  if (!telemetryValue.isNull())
  {
    Json::Value reported;
    reported["sender"] = 11;
    reported["type"] = 9;

    Json::Value dev;
    dev["state"] = 2; // Online
    dev["dictDPs"] = telemetryValue;

    reported[GetCloudId()] = dev;

    int rs = Gateway::getInstance()->AddDeviceToControlResponse(this, reported);
    if (rs != CODE_OK)
      rs = Gateway::getInstance()->PushDeviceUpdateCloud(this, telemetryValue);
    return rs;
  }
  return CODE_ERROR;
}

int Device::PushOnlineState(bool isOnline)
{
  Json::Value reported;
  reported["sender"] = 11;
  reported["type"] = 9;

  Json::Value dev;
  dev["state"] = isOnline ? 2 : 3;
  reported[GetCloudId()] = dev;

  int rs = Gateway::getInstance()->AddDeviceToControlResponse(this, reported);
  if (rs != CODE_OK)
    rs = Gateway::getInstance()->PushDeviceOnlineState(this, isOnline);
  return rs;
}

static map<uint32_t, const char *> typeToNameList;
static map<const char *, const char *> typeToModelList;
static map<uint32_t, uint32_t> bleTypeToGroupIdList;
static map<uint32_t, const char *> bleGroupIntToGroupString;
static map<uint16_t, const char *> bleAttributeIdToAttributeString;

void Device::InitDeviceModelList()
{
  bleTypeToGroupIdList[TYPE_GROUP_SWITCH] = BLE_SWITCH_TOUCH_GROUP;
  bleTypeToGroupIdList[BLE_DOWNLIGHT_SMT] = BLE_DOWNLIGHT_SMT_GROUP;
  bleTypeToGroupIdList[BLE_DOWNLIGHT_RGBCW] = BLE_DOWNLIGHT_RGBCW_GROUP;

  bleGroupIntToGroupString[BLE_SWITCH_TOUCH_GROUP] = "GROUP_SWITCH";
  bleGroupIntToGroupString[BLE_DOWNLIGHT_SMT_GROUP] = "D_CCT_BLE";
  bleGroupIntToGroupString[BLE_DOWNLIGHT_RGBCW_GROUP] = "D_CCT-RGB_BLE";

#ifdef CONFIG_ENABLE_BLE

  RegisterDeviceModel(HG_BLE_SWITCH_1, HG_BLE_SWITCH);
  RegisterDeviceModel(HG_BLE_SWITCH_2, HG_BLE_SWITCH);
  RegisterDeviceModel(HG_BLE_SWITCH_3, HG_BLE_SWITCH);
  RegisterDeviceModel(HG_BLE_SWITCH_4, HG_BLE_SWITCH);

#endif
}

void Device::RegisterDeviceModel(const char *type, const char *model)
{
  // typeToNameList[type] = name;
  typeToModelList[type] = model;
}

uint32_t Device::BleTypeToGroupId(uint32_t deviceType)
{
  return bleTypeToGroupIdList[deviceType];
}

uint32_t Device::BleGroupIntByString(const char *groupStr)
{
  for (const auto &[_groupInt, _groupStr] : bleGroupIntToGroupString)
  {
    if (strcmp(_groupStr, groupStr) == 0)
      return _groupInt;
  }
  return 0;
}

const char *Device::ConvertDeviceTypeToName(uint32_t type)
{
  // TODO: check null when use
  return typeToNameList[type];
}

const char *Device::ConvertDeviceTypeToModel(uint32_t type)
{
  // const char *model = typeToModelList[type];
  // if (model)
  //   return model;
  // else
  return "";
}

uint32_t Device::ConvertModelToDeviceType(const char *model)
{
  // for (const auto &[_type, _model] : typeToModelList) {
  //   if (strcmp(model, _model) == 0)
  //     return _type;
  // }
  return DEVICE_UNKNOWN_TYPE;
}

bool Device::GetIsFavorite() { return this->isFavorite; }

bool Device::SetIsFavorite(bool isFavorite)
{
  this->isFavorite = isFavorite;
  return this->isFavorite;
}

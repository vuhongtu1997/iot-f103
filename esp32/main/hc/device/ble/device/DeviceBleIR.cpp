#include "DeviceBleIR.h"
#include "BleProtocol.h"
#include "Database.h"
#include "Define.h"
#include "DeviceManager.h"
#include "Gateway.h"
#include "Log.h"
#include "Util.h"

#include <cctype>
#include <vector>

static bool IsHexString(const std::string &value)
{
  if (value.empty() || (value.size() % 2) != 0)
    return false;
  for (char ch : value)
  {
    if (!std::isxdigit(static_cast<unsigned char>(ch)))
      return false;
  }
  return true;
}

DeviceBleIR::DeviceBleIR(string id, string name, string mac, string type,
                         uint16_t addr, uint16_t version, Json::Value *dataJson)
    : DeviceBle(id, name, mac, type, addr, version, dataJson)
{
  countElement = 1;
  powerSource = POWER_AC;
}

std::string DeviceBleIR::GetPid() const
{
  if (dataJson.isObject())
  {
    if (dataJson.isMember("pid") && dataJson["pid"].isString())
      return dataJson["pid"].asString();

    if (dataJson.isMember("devices") && dataJson["devices"].isString())
    {
      auto tokens = Util::splitString(dataJson["devices"].asString(), '|');
      if (tokens.size() >= 2)
        return tokens[1];
    }
  }
  return "";
}

bool DeviceBleIR::IsVoiceFamily() const
{
  std::string pid = GetPid();
  return pid == HG_BLE_IR || pid == HG_BLE_IR_V2 || pid == HG_BLE_IR_BLE ||
         pid.empty();
}

bool DeviceBleIR::IsAcFamily() const
{
  std::string pid = GetPid();
  return pid == HG_BLE_IR_AC || pid == HG_BLE_IR_AC_V2 ||
         pid == HG_BLE_IR_AC_V2_LAO;
}

bool DeviceBleIR::IsTvFanRemoteFamily() const
{
  std::string pid = GetPid();
  return pid == HG_BLE_IR_TV || pid == HG_BLE_IR_FAN ||
         pid == HG_BLE_IR_REMOTE || pid == HG_BLE_IR_TV_V2 ||
         pid == HG_BLE_IR_FAN_V2 || pid == HG_BLE_IR_REMOTE_V2 ||
         pid == HG_BLE_IR_TV_V2_LAO || pid == HG_BLE_IR_FAN_V2_LAO ||
         pid == HG_BLE_IR_REMOTE_V2_LAO;
}

bool DeviceBleIR::IsIrV2Family() const
{
  std::string pid = GetPid();
  return pid == HG_BLE_IR_V2 || pid == HG_BLE_IR_TV_V2 ||
         pid == HG_BLE_IR_FAN_V2 || pid == HG_BLE_IR_AC_V2 ||
         pid == HG_BLE_IR_REMOTE_V2 || pid == HG_BLE_IR_BLE ||
         pid == HG_BLE_IR_TV_V2_LAO || pid == HG_BLE_IR_FAN_V2_LAO ||
         pid == HG_BLE_IR_AC_V2_LAO || pid == HG_BLE_IR_REMOTE_V2_LAO;
}

void DeviceBleIR::MergeDictDps(Json::Value &target, const Json::Value &source)
{
  if (!target.isObject())
    target = Json::objectValue;
  if (!source.isObject())
    return;

  for (auto const &key : source.getMemberNames())
  {
    target[key] = source[key];
  }
}

int DeviceBleIR::GetIntDp(const Json::Value &dictDPs, const char *dpId,
                          int fallback) const
{
  if (!dictDPs.isObject() || !dictDPs.isMember(dpId))
    return fallback;

  const Json::Value &value = dictDPs[dpId];
  if (value.isBool())
    return value.asBool() ? 1 : 0;
  if (value.isInt())
    return value.asInt();
  if (value.isUInt())
    return static_cast<int>(value.asUInt());
  if (value.isString())
  {
    const std::string raw = value.asString();
    if (raw.empty())
      return fallback;
    return atoi(raw.c_str());
  }
  return fallback;
}

int DeviceBleIR::IrV3DepartmentFromCode(int code) const
{
  if (code >= 1 && code <= 19)
    return 1;
  if (code >= 20 && code <= 59)
    return 2;
  if (code >= 60 && code <= 79)
    return 3;
  if (code == 100 || code == 101)
    return 0;
  if (code >= 110 && code <= 255)
    return 4;
  return -1;
}

bool DeviceBleIR::TryHandleIrV3Learning(Json::Value &dictDPs)
{
  if (!IsIrV2Family() || !dictDPs.isObject())
    return false;

  for (auto const &codeKey : dictDPs.getMemberNames())
  {
    Json::Value &codeNode = dictDPs[codeKey];
    if (!codeNode.isObject())
      continue;

    if (codeNode.isMember("index") && codeNode["index"].isInt() &&
        codeNode["index"].asInt() < 0 && codeNode.isMember("id") &&
        codeNode["id"].isInt())
    {
      int id = codeNode["id"].asInt();
      int department = IrV3DepartmentFromCode(id);
      if (department < 0)
      {
        LOGW("Invalid IR V3 learning id=%d for device %s", id, this->id.c_str());
        continue;
      }

      pendingIrV3CodeIndex = -1;
      pendingIrV3Id = id;
      LOGI("IR V3 learn custom command: device=%s codeKey=%s id=%d",
           this->id.c_str(), codeKey.c_str(), id);
      BleProtocol::getInstance()->LearnIRV3Code(
          addr, static_cast<uint8_t>(department), static_cast<uint16_t>(id));
      return true;
    }

    if (!codeNode.isMember("values") || !codeNode["values"].isObject())
      continue;

    Json::Value &values = codeNode["values"];
    for (auto const &indexKey : values.getMemberNames())
    {
      Json::Value &valueNode = values[indexKey];
      if (!valueNode.isObject() || !valueNode.isMember("index") ||
          !valueNode["index"].isInt() || valueNode["index"].asInt() >= 0 ||
          !valueNode.isMember("id") || !valueNode["id"].isInt())
        continue;

      int id = valueNode["id"].asInt();
      int department = IrV3DepartmentFromCode(id);
      if (department < 0)
      {
        LOGW("Invalid IR V3 learning child id=%d for device %s", id,
             this->id.c_str());
        continue;
      }

      pendingIrV3CodeIndex = atoi(indexKey.c_str());
      pendingIrV3Id = id;
      LOGI("IR V3 learn child command: device=%s codeKey=%s codeIndex=%d id=%d",
           this->id.c_str(), codeKey.c_str(), pendingIrV3CodeIndex, id);
      BleProtocol::getInstance()->LearnIRV3Code(
          addr, static_cast<uint8_t>(department), static_cast<uint16_t>(id));
      return true;
    }
  }

  return false;
}

bool DeviceBleIR::UpdateIrV3LearningResponse(Json::Value &dictDPs, int code,
                                             int index, int state)
{
  if (!dictDPs.isObject())
    dictDPs = Json::objectValue;

  std::string codeKey = to_string(code);
  if (!dictDPs.isMember(codeKey) || !dictDPs[codeKey].isObject())
    dictDPs[codeKey] = Json::objectValue;

  if (pendingIrV3CodeIndex >= 0)
  {
    std::string indexKey = to_string(pendingIrV3CodeIndex);
    if (!dictDPs[codeKey].isMember("values") ||
        !dictDPs[codeKey]["values"].isObject())
      dictDPs[codeKey]["values"] = Json::objectValue;
    if (!dictDPs[codeKey]["values"].isMember(indexKey) ||
        !dictDPs[codeKey]["values"][indexKey].isObject())
      dictDPs[codeKey]["values"][indexKey] = Json::objectValue;
    dictDPs[codeKey]["values"][indexKey]["index"] = index;
    dictDPs[codeKey]["values"][indexKey]["state"] = state;
    if (pendingIrV3Id > 0)
      dictDPs[codeKey]["values"][indexKey]["id"] = pendingIrV3Id;
  }
  else
  {
    dictDPs[codeKey]["index"] = index;
    dictDPs[codeKey]["state"] = state;
    if (pendingIrV3Id > 0)
      dictDPs[codeKey]["id"] = pendingIrV3Id;
  }

  pendingIrV3CodeIndex = -1000;
  pendingIrV3Id = 0;
  return true;
}

bool DeviceBleIR::PublishPendingIrV3LearningResponse(const Json::Value &dictDPs)
{
  if (pendingIrV3ResponsePageIndex <= 0 || dictDPs.isNull() ||
      !Gateway::getInstance() || Gateway::getInstance()->GetId().empty())
  {
    pendingIrV3ResponsePageIndex = -1;
    pendingIrV3ResponseSenderId.clear();
    return false;
  }

  LOGI("IR V3 learning response publish: device=%s data=%s",
       GetId().c_str(), dictDPs.toString().c_str());
  pendingIrV3ResponsePageIndex = -1;
  pendingIrV3ResponseSenderId.clear();
  Json::Value data = dictDPs;
  return Gateway::getInstance()->PushDeviceUpdateCloud(this, data) == CODE_OK;
}

void DeviceBleIR::SetPendingIrV3ResponseContext(int pageIndex,
                                                const std::string &senderId)
{
  pendingIrV3ResponsePageIndex = pageIndex;
  pendingIrV3ResponseSenderId = senderId;
}

static std::string BuildIrRawLearnResponse(uint16_t addr, uint8_t *data,
                                           int len)
{
  if (!data || len < 4)
    return "";

  const uint16_t frameLen = static_cast<uint16_t>(len + 6);
  const uint16_t gwAddr = Gateway::getInstance()->GetBleAddr();

  std::vector<uint8_t> response;
  response.reserve(static_cast<size_t>(len) + 8);
  response.push_back(static_cast<uint8_t>(frameLen & 0xFF));
  response.push_back(static_cast<uint8_t>(frameLen >> 8));
  response.push_back(0x91);
  response.push_back(0x81);
  response.push_back(static_cast<uint8_t>(addr >> 8));
  response.push_back(static_cast<uint8_t>(addr & 0xFF));
  response.push_back(static_cast<uint8_t>(gwAddr >> 8));
  response.push_back(static_cast<uint8_t>(gwAddr & 0xFF));
  for (int i = 0; i < len; ++i)
    response.push_back(data[i]);

  return Util::ConvertU32ToHexString(response.data(),
                                     static_cast<int>(response.size()));
}

int DeviceBleIR::BuildTelemetryValue(Json::Value &pushDataValue)
{
  if (dataJson.isObject() && dataJson.isMember("dictDPs") &&
      dataJson["dictDPs"].isObject())
  {
    for (auto const &key : dataJson["dictDPs"].getMemberNames())
    {
      pushDataValue[key] = dataJson["dictDPs"][key];
    }
  }
  pushDataValue["stt"] = IsOnline();
  return CODE_OK;
}

int DeviceBleIR::Do(Json::Value &dataValue)
{
  if (!dataValue.isObject())
    return CODE_FORMAT_ERROR;

  if (!dataJson.isObject())
    dataJson = Json::objectValue;
  if (!dataJson.isMember("dictDPs") || !dataJson["dictDPs"].isObject())
    dataJson["dictDPs"] = Json::objectValue;

  bool irV3Learning = TryHandleIrV3Learning(dataValue);

  Json::Value merged = dataJson["dictDPs"];
  MergeDictDps(merged, dataValue);
  dataJson["dictDPs"] = merged;
  if (irV3Learning)
  {
    Database::getInstance()->DeviceUpdateData(this);
    return CODE_OK;
  }

  bool sendCmdDirectlyFromApp = false;
  if (dataValue.isMember(DPID_IR_COMMAND) &&
      dataValue[DPID_IR_COMMAND].isString())
  {
    const std::string command = dataValue[DPID_IR_COMMAND].asString();
    if (IsHexString(command))
    {
      if (BleProtocol::getInstance()->ControlIRCmd(command) == CODE_OK)
      {
        sendCmdDirectlyFromApp = true;
      }
    }
    else if (!command.empty() && command != "0")
    {
      LOGW("Ignore invalid IR command hex for device %s: %s", id.c_str(),
           command.c_str());
    }
  }

  const int brandId = GetIntDp(merged, DPID_IR_BRAND_ID, 0);
  const int remoteId = GetIntDp(merged, DPID_IR_REMOTE_ID, 0);
  const int commandType = GetIntDp(merged, DPID_IR_COMMAND_TYPE, 0);

  int temp = 0;
  if (merged.isMember(DPID_IR_TEMP))
    temp = GetIntDp(merged, DPID_IR_TEMP, 0);
  else if (merged.isMember(DPID_IR_ONOFF))
    temp = GetIntDp(merged, DPID_IR_ONOFF, 0);

  const int mode = GetIntDp(merged, DPID_IR_MODE, 0);
  const int fan = GetIntDp(merged, DPID_IR_FAN, 0);
  const int swing = GetIntDp(merged, DPID_IR_SWING, 0);

  if (!sendCmdDirectlyFromApp &&
      (IsAcFamily() || IsTvFanRemoteFamily()) && commandType > 0 &&
      brandId > 0 && remoteId > 0)
  {
    if (BleProtocol::getInstance()->ControlIR(addr, commandType, brandId,
                                              remoteId, temp, mode, fan,
                                              swing) != CODE_OK)
    {
      return CODE_ERROR;
    }
  }

  return CODE_OK;
}

bool DeviceBleIR::CheckData(Json::Value &dataValue, bool &rs)
{
  if (!dataValue.isArray() || dataValue.size() < 3)
    return false;

  const std::string ruleKey = dataValue[0].asString();
  const std::string op = dataValue[1].asString();
  int expectVal = 0;
  if (dataValue[2].isInt())
    expectVal = dataValue[2].asInt();
  else if (dataValue[2].isBool())
    expectVal = dataValue[2].asBool() ? 1 : 0;

  Json::Value dictDPs = dataJson.isObject() && dataJson.isMember("dictDPs") &&
                                 dataJson["dictDPs"].isObject()
                             ? dataJson["dictDPs"]
                             : Json::Value(Json::objectValue);
  const int currentVal = GetIntDp(dictDPs, ruleKey.c_str(), 0);
  rs = Util::CompareNumber(op, currentVal, expectVal);
  return true;
}

void DeviceBleIR::CheckRuleTrigger(const Json::Value &data)
{
  if (!data.isObject())
    return;

  for (auto &ruleInputDevice : deviceRuleInputList)
  {
    Json::Value *ruleValue = ruleInputDevice->GetData();
    if (!ruleValue || !ruleValue->isArray() || ruleValue->size() < 3)
      continue;

    const std::string ruleKey = (*ruleValue)[0].asString();
    if (!data.isMember(ruleKey))
      continue;

    const std::string op = (*ruleValue)[1].asString();
    int expectVal = 0;
    if ((*ruleValue)[2].isInt())
      expectVal = (*ruleValue)[2].asInt();
    else if ((*ruleValue)[2].isBool())
      expectVal = (*ruleValue)[2].asBool() ? 1 : 0;

    const int currentVal = GetIntDp(data, ruleKey.c_str(), -1);
    const bool matched = Util::CompareNumber(op, currentVal, expectVal);
    LOGD("IR rule trigger: device=%s key=%s current=%d expect=%d op=%s matched=%s",
         id.c_str(), ruleKey.c_str(), currentVal, expectVal, op.c_str(),
         matched ? "true" : "false");
    if (matched)
      ruleInputDevice->Trigger(true);
  }
}

void DeviceBleIR::InputData(uint8_t *data, int len, uint16_t inputAddr)
{
  (void)inputAddr;

  if (!data || len < 8)
    return;

  const uint16_t header = data[3] | (data[4] << 8);
  const uint16_t vendorHeader = data[3] | (data[4] << 8);
  const bool isIrVendorPayload =
      data[0] == 0xE5 && data[1] == 0x11 && data[2] == 0x02;
  if (header != GW_RESPONSE_IR && !isIrVendorPayload && vendorHeader != 0x0355 &&
      vendorHeader != 0x0455 && vendorHeader != 0x0655)
    return;

  Json::Value report = dataJson.isObject() && dataJson.isMember("dictDPs") &&
                               dataJson["dictDPs"].isObject()
                           ? dataJson["dictDPs"]
                           : Json::Value(Json::objectValue);
  Json::Value telemetryReport;
  bool pushedDirectResponse = false;
  bool ruleTriggerPending = false;
  Json::Value ruleTriggerData;

  const uint8_t irCmd =
      isIrVendorPayload && len > 3 ? data[3] : static_cast<uint8_t>(0);

  if (data[0] == 0xE5 && data[1] == 0x11 && data[2] == 0x02 &&
      data[3] == 0x55 && data[4] == 0x03 && len >= 16)
  {
    const int code = (data[7] | (data[8] << 8)) + 1000;
    const int index = data[9] | (data[10] << 8);
    const int state = data[15] + 2;
    LOGI("IR V3 learn response: device=%s code=%d index=%d state=%d "
         "pendingIndex=%d",
         id.c_str(), code, index, state, pendingIrV3CodeIndex);
    const std::string codeKey = to_string(code);
    if (pendingIrV3CodeIndex >= 0)
    {
      const std::string indexKey = to_string(pendingIrV3CodeIndex);
      telemetryReport[codeKey]["values"][indexKey]["index"] = index;
      telemetryReport[codeKey]["values"][indexKey]["state"] = state;
    }
    else
    {
      telemetryReport[codeKey]["index"] = index;
      telemetryReport[codeKey]["state"] = state;
    }
    UpdateIrV3LearningResponse(report, code, index, state);
  }
  else if (irCmd == 0x0A && len >= 10)
  {
    const int temp = data[7];
    const int mode = data[8];
    const int fan = data[9] >> 4;
    const int swing = data[9] & 0x0F;

    report[DPID_IR_MODE] = mode;
    report[DPID_IR_TEMP] = temp;
    report[DPID_IR_FAN] = fan;
    report[DPID_IR_SWING] = swing;
  }
  else if (irCmd == 0x09 && len >= 6)
  {
    const int voiceId = data[4] | (data[5] << 8);
    LOGI("IR voice command: device=%s voiceId=%d", id.c_str(), voiceId);
    report[DPID_IR_BRAND_ID] = voiceId;
    ruleTriggerData[DPID_IR_BRAND_ID] = voiceId;
    ruleTriggerPending = true;
  }
  else if (isIrVendorPayload && irCmd == 0x0B)
  {
    const std::string respCmd = BuildIrRawLearnResponse(addr, data, len);
    if (!respCmd.empty())
    {
      LOGI("IR raw learn response: device=%s command=%s", id.c_str(),
           respCmd.c_str());
      report[DPID_IR_COMMAND] = respCmd;

    }
  }

  if (ruleTriggerPending)
  {
    bool dispatchedVoiceRule = false;
    DeviceManager::getInstance()->ForEach([&](Device *device)
                                          {
      if (!device || device->GetType() != "HG_BLE_IR" ||
          device->GetAddr() != GetAddr())
      {
        return;
      }

      DeviceBleIR *irDevice = dynamic_cast<DeviceBleIR *>(device);
      if (!irDevice || !irDevice->IsVoiceFamily())
      {
        return;
      }

      dispatchedVoiceRule = true;
      const std::string targetId = irDevice->GetId();
      LOGI("IR voice trigger dispatch: source=%s target=%s addr=0x%04X",
           id.c_str(), targetId.c_str(), GetAddr());
      irDevice->CheckRuleTrigger(ruleTriggerData); });

    if (!dispatchedVoiceRule)
    {
      CheckRuleTrigger(ruleTriggerData);
    }
    report[DPID_IR_BRAND_ID] = 0;
  }

  dataJson["dictDPs"] = report;
  Database::getInstance()->DeviceUpdateData(this);
  UpdateLastTimeActive();
  if (!lastOnlineState)
  {
    lastOnlineState = true;
    Gateway::getInstance()->PushDeviceOnlineState(this, true);
  }
  if (!pushedDirectResponse &&
      PublishPendingIrV3LearningResponse(telemetryReport.isNull() ? report
                                                                  : telemetryReport))
  {
    pushedDirectResponse = true;
  }
  if (!pushedDirectResponse)
    PushTelemetry(telemetryReport.isNull() ? report : telemetryReport);
}

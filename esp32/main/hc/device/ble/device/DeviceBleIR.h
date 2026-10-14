#pragma once

#include "DeviceBle.h"

class DeviceBleIR : public DeviceBle
{
private:
  std::string GetPid() const;
  bool IsVoiceFamily() const;
  bool IsAcFamily() const;
  bool IsTvFanRemoteFamily() const;
  bool IsIrV2Family() const;
  void MergeDictDps(Json::Value &target, const Json::Value &source);
  int GetIntDp(const Json::Value &dictDPs, const char *dpId,
               int fallback = 0) const;
  int IrV3DepartmentFromCode(int code) const;
  bool TryHandleIrV3Learning(Json::Value &dictDPs);
  bool UpdateIrV3LearningResponse(Json::Value &dictDPs, int code, int index,
                                  int state);
  bool PublishPendingIrV3LearningResponse(const Json::Value &dictDPs);
  void CheckRuleTrigger(const Json::Value &data);

  int pendingIrV3CodeIndex = -1000;
  int pendingIrV3Id = 0;
  int pendingIrV3ResponsePageIndex = -1;
  std::string pendingIrV3ResponseSenderId;

public:
  DeviceBleIR(string id, string name, string mac, string type, uint16_t addr,
              uint16_t version, Json::Value *dataJson);

  int BuildTelemetryValue(Json::Value &pushDataValue) override;
  int Do(Json::Value &dataValue) override;
  void InputData(uint8_t *data, int len, uint16_t addr = 0) override;
  bool CheckData(Json::Value &dataValue, bool &rs) override;
  void SetPendingIrV3ResponseContext(int pageIndex, const std::string &senderId);
};

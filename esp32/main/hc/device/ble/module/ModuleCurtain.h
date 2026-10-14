#pragma once

#include "Module.h"

class ModuleCurtain : public Module
{
private:
  int status;
  int position;
  bool hasPosition;
  int travelTime;
  int curtainType;
  int direction;

public:
  ModuleCurtain(Device *device, uint16_t addr);
  ~ModuleCurtain();

  int InputData(Json::Value &dataValue, Json::Value &jsonValue);
  int InputData(uint8_t *data, int len, Json::Value &jsonValue);
  bool CheckData(Json::Value &dataValue, bool &rs);
  void BuildTelemetryValue(Json::Value &jsonValue);
  int Do(Json::Value &dataValue);
};

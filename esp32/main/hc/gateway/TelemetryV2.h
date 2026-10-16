#pragma once

#include "json.h"
#include "ModbusBus.h"
#include <ctime>
#include <string>

// One batch per polling cycle: never replay measurements from an earlier cycle.
class TelemetryV2
{
  Json::Value env{Json::objectValue};
  Json::Value gas{Json::arrayValue};
  time_t oldest = 0;
  time_t newest = 0;
  bool invalid = false;

public:
  void Add(const ModbusNodeInfo &node, const uint16_t *registers, time_t sampledAt);
  bool Serialize(const std::string &gatewayUid, const std::string &token,
                 time_t now, std::string &payload) const;
};

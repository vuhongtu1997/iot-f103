#pragma once

#include <stdint.h>
#include <vector>
#include <string>
#include "protocol.h"

struct ModbusNodeInfo
{
  uint8_t uid[12];
  uint8_t address;
  uint8_t mode;
  uint8_t sensorType;
  uint32_t version;
  std::string sensorUid; // Cloud identity, persisted by the master, never sent to the node.
};

bool ModbusBusInit();
bool ModbusDiscoverNodes();
int ModbusAssignAddress(uint8_t address, ModbusNodeInfo *assigned,
                        const std::string &sensorUid = "");
bool ModbusSetSensorUid(uint8_t address, const std::string &sensorUid);
void ModbusGetNodes(std::vector<ModbusNodeInfo> &nodes);
bool ModbusReadNode(const ModbusNodeInfo &node, uint16_t registers[DIGITAL_REGISTER_COUNT]);

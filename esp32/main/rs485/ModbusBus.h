#pragma once

#include <stdint.h>
#include <vector>

struct ModbusNodeInfo
{
  uint8_t uid[12];
  uint8_t address;
  uint8_t mode;
  uint8_t sensorType;
  uint32_t version;
};

bool ModbusBusInit();
bool ModbusDiscoverNodes();
int ModbusAssignAddress(uint8_t address, ModbusNodeInfo *assigned);
void ModbusGetNodes(std::vector<ModbusNodeInfo> &nodes);
bool ModbusReadNode(const ModbusNodeInfo &node, uint16_t registers[12]);

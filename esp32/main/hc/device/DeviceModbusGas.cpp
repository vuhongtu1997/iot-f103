#include "DeviceModbusGas.h"

#include <utility>

DeviceModbusGas::DeviceModbusGas(std::string id, std::string name,
                                 std::string uid, uint16_t address,
                                 uint32_t version, uint8_t sensorType,
                                 Json::Value *dataJson)
    : DeviceModbus(std::move(id), std::move(name), std::move(uid), address,
                   version, 0, sensorType, GAS_TYPE, dataJson)
{
}

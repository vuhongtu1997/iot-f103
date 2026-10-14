#include "ModulePirLightSensorStartup.h"
#include "Log.h"
#include "Util.h"
#include "Device.h"
#include "BleProtocol.h"
#include "Database.h"

ModulePirLightSensorStartup::ModulePirLightSensorStartup(Device *device, uint32_t addr) : Module(device, addr)
{
    pir = 0;
    lux = 0;
}

ModulePirLightSensorStartup::~ModulePirLightSensorStartup()
{
}

int ModulePirLightSensorStartup::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
    typedef struct __attribute__((packed))
    {
        uint8_t opcode;
        uint16_t vendorId;
        uint16_t header;
        uint8_t pir;
        uint16_t lux;
    } data_message_t;
    data_message_t *data_message = (data_message_t *)data;
    if (data_message->opcode == RD_OPCODE_CONFIG_RSP)
    {
        if (data_message->header == RD_HEADER_RSP_PIR_LIGHT_SENSOR_STARTUP)
        {
            pir = data_message->pir;
            lux = data_message->lux;
            BuildTelemetryValue(jsonValue);
            uint16_t sceneId = data[5] | (data[6] << 8);
            return CODE_OK;
        }
    }
    return CODE_ERROR;
}

void ModulePirLightSensorStartup::BuildTelemetryValue(Json::Value &jsonValue)
{
    jsonValue[KEY_ATTRIBUTE_PIR] = pir;
    jsonValue[KEY_ATTRIBUTE_LUX] = lux;
}

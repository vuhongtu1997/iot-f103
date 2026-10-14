#include "ModulePirSensor.h"
#include "BleProtocol.h"
#include "Database.h"
#include "Device.h"
#include "Gateway.h"
#include "Log.h"
#include "Util.h"

// #if 1 // Scene feature disabled
// #include "Scene.h"
// #endif

ModulePirSensor::ModulePirSensor(Device *device, uint16_t addr)
    : Module(device, addr) {
  pir = 0;
}

ModulePirSensor::~ModulePirSensor() {}

#ifdef CONFIG_SAVE_ATTRIBUTE
void ModulePirSensor::InitAttribute(string attribute, double value) {
  if (attribute == KEY_ATTRIBUTE_PIR)
    pir = value;
}

void ModulePirSensor::SaveAttribute() {
  Database::getInstance()->DeviceAttributeAdd(device, KEY_ATTRIBUTE_PIR, pir);
}
#endif

int ModulePirSensor::InputData(Json::Value &dataValue, Json::Value &jsonValue) {
  if (dataValue.isObject() && dataValue.isMember(KEY_ATTRIBUTE_PIR) &&
      dataValue[KEY_ATTRIBUTE_PIR].isInt()) {
    pir = dataValue[KEY_ATTRIBUTE_PIR].asInt();
    BuildTelemetryValue(jsonValue);
    return CODE_OK;
  }
  return CODE_ERROR;
}

int ModulePirSensor::InputData(uint8_t *data, int len, Json::Value &jsonValue) {
  typedef struct __attribute__((packed)) {
    uint8_t opcode;
    uint16_t header;
    uint16_t pir;
    uint16_t scene;
  } data_message_t;
  data_message_t *data_message = (data_message_t *)data;
  if (data_message->opcode == RD_OPCODE_SENSOR_RSP &&
      data_message->header == HG_HEADER_0005) {
    if (pir != data_message->pir) {
      pir = data_message->pir;
#ifdef CONFIG_SAVE_ATTRIBUTE
      SaveAttribute();
#endif
    }
    BuildTelemetryValue(jsonValue);
    CheckTrigger(jsonValue);

#if 1 // Scene feature disabled
		if (data_message->scene > 0)
		{
			Scene *scene = SceneManager::getInstance()->GetSceneFromAddr(data_message->scene);
			if (scene)
			{
				for (int i = 0; i < scene->deviceList.size(); i++)
				{
					DeviceBle *dev = (DeviceBle *)scene->deviceList[i]->device;
					if (dev)
					{
						if (scene->deviceList[i]->data.isArray())
						{
							for (Json::ArrayIndex j = 0; j < scene->deviceList[i]->data.size(); j++)
							{
								if (scene->deviceList[i]->data[j].isObject())
								{
									dev->InputData(scene->deviceList[i]->data[j]);
								}
							}
						}
						else if (scene->deviceList[i]->data.isObject())
						{
							dev->InputData(scene->deviceList[i]->data);
						}
					}
					else
					{
						LOGW("DeviceBle error");
					}
				}
			}
			else
				LOGW("Scene not found");
		}
#endif
    return CODE_OK;
  }
  return CODE_ERROR;
}

bool ModulePirSensor::CheckData(Json::Value &dataValue, bool &rs) {
  LOGV("CheckData data: %s", dataValue.toString().c_str());
  if (dataValue.isObject() && dataValue.isMember(KEY_ATTRIBUTE_PIR) &&
      dataValue.isMember("op") && dataValue["op"].isString()) {
    string op = dataValue["op"].asString();
    if (dataValue[KEY_ATTRIBUTE_PIR].isInt()) {
      int pir = dataValue[KEY_ATTRIBUTE_PIR].asInt();
      rs = Util::CompareNumber(op, this->pir, pir);
      return true;
    } else if (dataValue[KEY_ATTRIBUTE_PIR].isArray()) {
      Json::Value listValue = dataValue[KEY_ATTRIBUTE_PIR];
      if (listValue.size() == 2 && listValue[0].isInt() &&
          listValue[1].isInt()) {
        int pir1 = listValue[0].asInt();
        int pir2 = listValue[1].asInt();
        rs = Util::CompareNumber(op, this->pir, pir1, pir2);
        return true;
      }
    }
  }
  return false;
}

void ModulePirSensor::BuildTelemetryValue(Json::Value &jsonValue) {
  jsonValue[KEY_ATTRIBUTE_PIR] = pir;
}

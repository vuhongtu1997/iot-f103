#include "ModulePirLight.h"
#include "BleProtocol.h"
#include "Database.h"
#include "Device.h"
#include "Gateway.h"
#include "Log.h"
#include "Util.h"

#if 1 // Rule/Scene features disabled
#include "Rule.h"
#include "RuleInputDevice.h"
#include "Scene.h"

#endif

ModulePirLight::ModulePirLight(Device *device, uint16_t addr)
    : Module(device, addr)
{
  pir = 0;
  lux = 0;
}

ModulePirLight::~ModulePirLight() {}

#ifdef CONFIG_SAVE_ATTRIBUTE
void ModulePirLight::InitAttribute(string attribute, double value)
{
  if (attribute == KEY_ATTRIBUTE_PIR)
    pir = value;
  else if (attribute == KEY_ATTRIBUTE_LUX)
    lux = value;
}

void ModulePirLight::SaveAttribute(string key)
{
  if (key == KEY_ATTRIBUTE_PIR)
    Database::getInstance()->DeviceAttributeAdd(device, KEY_ATTRIBUTE_PIR, pir);
  else if (key == KEY_ATTRIBUTE_LUX)
    Database::getInstance()->DeviceAttributeAdd(device, KEY_ATTRIBUTE_LUX, lux);
}
#endif

int ModulePirLight::InputData(Json::Value &dataValue, Json::Value &jsonValue)
{
  if (dataValue.isObject() && dataValue.isMember(KEY_ATTRIBUTE_PIR) &&
      dataValue[KEY_ATTRIBUTE_PIR].isInt() &&
      dataValue.isMember(KEY_ATTRIBUTE_LUX) &&
      dataValue[KEY_ATTRIBUTE_LUX].isInt())
  {
    pir = dataValue[KEY_ATTRIBUTE_PIR].asInt();
    lux = dataValue[KEY_ATTRIBUTE_LUX].asInt();
    BuildTelemetryValue(jsonValue);
    return CODE_OK;
  }
  return CODE_ERROR;
}

int ModulePirLight::InputData(uint8_t *data, int len, Json::Value &jsonValue)
{
  typedef struct __attribute__((packed))
  {
    uint8_t opcode;
    uint16_t header;
    uint16_t pir;
    uint16_t scene;
    uint16_t lux;
  } data_message_t;
  data_message_t *data_message = (data_message_t *)data;
  if (data_message->opcode == RD_OPCODE_SENSOR_RSP &&
      data_message->header == HG_HEADER_0005)
  {
    int temp_pir = data_message->pir;
    int temp_lux = data_message->lux;
    if (len == 7)
    {
      if (pir != temp_pir)
      {
        pir = temp_pir;
#ifdef CONFIG_SAVE_ATTRIBUTE
        SaveAttribute(KEY_ATTRIBUTE_PIR);
#endif
      }
    }
    else if (len > 7)
    {
      if (temp_pir != pir)
      {
        pir = temp_pir;
#ifdef CONFIG_SAVE_ATTRIBUTE
        SaveAttribute(KEY_ATTRIBUTE_PIR);
#endif
      }
      if (temp_lux != lux)
      {
        lux = temp_lux;
#ifdef CONFIG_SAVE_ATTRIBUTE
        SaveAttribute(KEY_ATTRIBUTE_LUX);
#endif
      }
      BuildTelemetryValue(jsonValue);

#if 1 // Rule feature disabled
      for (auto &ruleInputDevice : device->deviceRuleInputList)
      {
        Json::Value &ruleValue = *(ruleInputDevice->GetData());

        if (jsonValue.isObject() && ruleValue.isObject())
        {
          for (auto const &key : jsonValue.getMemberNames())
          {
            if (ruleValue.isMember(key))
            {
              bool rs = false;
              if (device->CheckData(*ruleInputDevice->GetData(), rs))
                ruleInputDevice->UpdateStatus(rs);
            }
          }
        }
        else
        {
          LOGW("Is not object");
        }
      }

      CheckTrigger(jsonValue);
#endif

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
  }
  return CODE_ERROR;
}

bool ModulePirLight::CheckData(Json::Value &dataValue, bool &rs)
{
  LOGV("CheckData data: %s", dataValue.toString().c_str());
  if (dataValue.isObject() && dataValue.isMember("op") &&
      dataValue["op"].isString())
  {
    string op = dataValue["op"].asString();
    if (dataValue.isMember(KEY_ATTRIBUTE_PIR))
    {
      if (dataValue[KEY_ATTRIBUTE_PIR].isInt())
      {
        int pir = dataValue[KEY_ATTRIBUTE_PIR].asInt();
        rs = Util::CompareNumber(op, this->pir, pir);
        return true;
      }
      else if (dataValue[KEY_ATTRIBUTE_PIR].isArray())
      {
        Json::Value listValue = dataValue[KEY_ATTRIBUTE_PIR];
        if (listValue.size() == 2 && listValue[0].isInt() &&
            listValue[1].isInt())
        {
          int pir1 = listValue[0].asInt();
          int pir2 = listValue[1].asInt();
          rs = Util::CompareNumber(op, this->pir, pir1, pir2);
          return true;
        }
      }
    }
    else if (dataValue.isMember(KEY_ATTRIBUTE_LUX))
    {
      if (dataValue[KEY_ATTRIBUTE_LUX].isInt())
      {
        int lux = dataValue[KEY_ATTRIBUTE_LUX].asInt();
        rs = Util::CompareNumber(op, this->lux, lux);
        return true;
      }
      else if (dataValue[KEY_ATTRIBUTE_LUX].isArray())
      {
        Json::Value listValue = dataValue[KEY_ATTRIBUTE_LUX];
        if (listValue.size() == 2 && listValue[0].isInt() &&
            listValue[1].isInt())
        {
          int lux1 = listValue[0].asInt();
          int lux2 = listValue[1].asInt();
          rs = Util::CompareNumber(op, this->lux, lux1, lux2);
          return true;
        }
      }
    }
  }
  return false;
}

void ModulePirLight::BuildTelemetryValue(Json::Value &jsonValue)
{
  jsonValue[KEY_ATTRIBUTE_PIR] = pir;
  jsonValue[KEY_ATTRIBUTE_LUX] = lux;
}

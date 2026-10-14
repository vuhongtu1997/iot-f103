#include "ModuleCallScene.h"
#include "BleOpCode.h"
#include "BleProtocol.h"
#include "Device.h"
#include "Gateway.h"
#include "Log.h"
#include "Util.h"

#if 1 // Scene feature disabled
#include "Scene.h"
#endif

ModuleCallScene::ModuleCallScene(Device *device, uint16_t addr)
    : Module(device, addr) {
  idScene = 0;
  id = 0;
  value = 0;
}

ModuleCallScene::~ModuleCallScene() {}

int ModuleCallScene::InputData(uint8_t *data, int len, Json::Value &jsonValue) {
  typedef struct __attribute__((packed)) {
    uint16_t opcode;
    uint16_t id;
    uint16_t idScene;
  } data_message_t;
  data_message_t *data_message = (data_message_t *)data;
  if (data_message->opcode == BLE_MESH_OPCODE_RGB) {
    if (len == 7 || len == 9)
      idScene = data_message->idScene;
    else
      idScene = data_message->id;
    if (idScene > 0) {
#if 1 // Scene feature disabled
			Scene *scene = SceneManager::getInstance()->GetSceneFromAddr(idScene);
			if (scene)
			{
				for (int i = 0; i < scene->deviceList.size(); i++)
				{
					if (scene->deviceList[i]->device->GetAddr() == addr)
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
						break;
					}
					else
					{
						// LOGW("Device not found");
					}
				}
			}
			else
				LOGW("Scene not found");
#endif
    }
    return CODE_OK;
  }
  return CODE_ERROR;
}

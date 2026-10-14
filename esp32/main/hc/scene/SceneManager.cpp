#include "SceneManager.h"
#include "Log.h"
#include "DeviceManager.h"
#include "BleProtocol.h"
#include "Group.h"
#include <cstdlib>

namespace
{
bool IsSingleCurtainSceneDp(Device *device, int dpId)
{
	if (!device)
		return false;

	string type = device->GetType();
	return (type == "HG_BLE_CURTAIN_NEW" ||
			type == "HG_BLE_ROLLING_DOOR_NEW") &&
		   (dpId == 1 || dpId == 2);
}

bool IsLightSceneDp(Device *device, int dpId)
{
	if (!device)
		return false;

	string type = device->GetType();
	if (type == "HG_BLE_LIGHT_WHITE" || type == "HG_BLE_SWITCH_DIMMING")
		return dpId == 20;
	if (type == "HG_BLE_LIGHT_RGB")
		return dpId == 20 || dpId == 21 || dpId == 24;
	return false;
}

uint16_t GetSceneActionElement(Device *device, int dpId)
{
	if (IsSingleCurtainSceneDp(device, dpId) || IsLightSceneDp(device, dpId))
		return device->GetAddr();
	return Group::GetElementFromDp(device, dpId);
}

bool TryGetInt(const Json::Value &value, int &result)
{
	if (value.isInt() || value.isUInt())
	{
		result = value.asInt();
		return true;
	}
	if (value.isBool())
	{
		result = value.asBool() ? 1 : 0;
		return true;
	}
	if (value.isString())
	{
		const string text = value.asString();
		char *end = nullptr;
		long parsed = strtol(text.c_str(), &end, 10);
		if (end != text.c_str() && end && *end == '\0')
		{
			result = static_cast<int>(parsed);
			return true;
		}
	}
	return false;
}

int GetDpInt(const Json::Value &dps, const char *key, int fallback)
{
	int value = fallback;
	return dps.isObject() && dps.isMember(key) && TryGetInt(dps[key], value)
			   ? value
			   : fallback;
}
}

SceneManager *SceneManager::getInstance()
{
	static SceneManager *sceneManager = NULL;
	if (!sceneManager)
	{
		sceneManager = new SceneManager();
	}
	return sceneManager;
}

SceneManager::SceneManager()
{
}

void SceneManager::ForEach(std::function<void(Scene *)> func)
{
	sceneListMtx.lock();
	for (auto &[id, scene] : sceneList)
	{
		func(scene);
	}
	sceneListMtx.unlock();
}

Scene *SceneManager::AddScene(Json::Value &sceneValue)
{
	LOGD("SceneManager::AddScene");
	for (auto &member : sceneValue.getMemberNames())
	{
		if (sceneValue[member].isObject())
		{
			string id = member;
			uint16_t addr = static_cast<uint16_t>(strtoul(id.c_str(), NULL, 16));
			Json::Value &sceneData = sceneValue[member];
			string name = sceneData.get("name", id).asString();
			bool hasDeleteOperation = false;
			Scene *scene = GetSceneFromId(id);
			if (!scene)
			{
				scene = new Scene(id, addr, name);
				AddScene(scene);
			}

			if (sceneData.isMember("conditions") && sceneData["conditions"].isArray())
			{
				Json::Value &conditions = sceneData["conditions"];
				for (auto &condition : conditions)
				{
					if (condition.isMember("entityID") && condition["entityID"].isString() &&
						condition.isMember("entityType") && condition["entityType"].isInt() &&
						condition.isMember("expr") && condition["expr"].isArray())
					{
						string deviceId = condition["entityID"].asString();
						int state = condition.get("state", -2).asInt();
						hasDeleteOperation = hasDeleteOperation || state == -3;
						Json::Value expr = condition["expr"];
						int dpId = 0;
						int opInt = 0;
						int valueInt = 0;

						if (expr.size() >= 3)
						{
							if (expr[0].isString())
							{
								string dp = expr[0].asString();
								if (dp.rfind("$dp", 0) == 0)
									dpId = atoi(dp.c_str() + 3);
							}
							if (expr[1].isString())
							{
								string op = expr[1].asString();
								if (op == ">")
									opInt = 2;
								else if (op == "==")
									opInt = 1;
							}
							if (expr[2].isBool())
								valueInt = expr[2].asBool() ? 1 : 0;
							else if (expr[2].isInt())
								valueInt = expr[2].asInt();
						}

						Device *device =
							DeviceManager::getInstance()->GetDeviceFromId(deviceId);
						int result = CODE_ERROR;
						if (device && dpId > 0 && dpId <= device->GetNumElement())
						{
							uint16_t elementAddr =
								Group::GetElementFromDp(device, dpId);
							result = BleProtocol::getInstance()->SetConditionDeviceScene(
								state, elementAddr, addr, device->GetType(),
								valueInt, opInt);
						}
						else
						{
							LOGW("Invalid scene condition target: device=%s dp=%d",
								 deviceId.c_str(), dpId);
						}
						condition["state"] = result == CODE_OK ? 0 : -1;
					}
				}
			}

			if (sceneData.isMember("actions") && sceneData["actions"].isArray())
			{
				Json::Value &actions = sceneData["actions"];
				for (auto &action : actions)
				{
					if (!action.isObject() ||
						!action.isMember("entityID") ||
						!action["entityID"].isString() ||
						!action.isMember("executorProperty") ||
						!action["executorProperty"].isObject())
					{
						continue;
					}

					string deviceId = action["entityID"].asString();
					int state = -2;
					TryGetInt(action.get("state", -2), state);
					hasDeleteOperation = hasDeleteOperation || state == -3;
					Device *device =
						DeviceManager::getInstance()->GetDeviceFromId(deviceId);
					int result = CODE_ERROR;
					if (device)
					{
						const bool isIrAction =
							device->GetType() == "HG_BLE_IR" &&
							action.get("actionExecutor", "").asString() == "irHGBLE";
						if (isIrAction)
						{
							Json::Value deviceData = device->GetData();
							Json::Value irDps =
								deviceData.isObject() && deviceData.isMember("dictDPs") &&
										deviceData["dictDPs"].isObject()
									? deviceData["dictDPs"]
									: Json::Value(Json::objectValue);
							for (const auto &dpKey :
								 action["executorProperty"].getMemberNames())
							{
								irDps[dpKey] = action["executorProperty"][dpKey];
							}

							const int commandType = GetDpInt(irDps, DPID_IR_COMMAND_TYPE, 0);
							const int brandId = GetDpInt(irDps, DPID_IR_BRAND_ID, 0);
							const int remoteId = GetDpInt(irDps, DPID_IR_REMOTE_ID, 0);
							const int temp = irDps.isMember(DPID_IR_TEMP)
											 ? GetDpInt(irDps, DPID_IR_TEMP, 0)
											 : GetDpInt(irDps, DPID_IR_ONOFF, 0);
							result = BleProtocol::getInstance()->SetIrDeviceActionScene(
								state, device->GetAddr(), addr, commandType, brandId,
								remoteId, temp, GetDpInt(irDps, DPID_IR_MODE, 0),
								GetDpInt(irDps, DPID_IR_FAN, 1),
								GetDpInt(irDps, DPID_IR_SWING, 1));
							action["state"] = result == CODE_OK ? 0 : -1;
							continue;
						}

						for (const auto &dpKey :
							 action["executorProperty"].getMemberNames())
						{
							int dpId = atoi(dpKey.c_str());
							const Json::Value &dpValue =
								action["executorProperty"][dpKey];
							int value = 0;
							if (!TryGetInt(dpValue, value))
							{
								LOGW("Invalid scene action value: device=%s dp=%s",
									 deviceId.c_str(), dpKey.c_str());
								result = CODE_FORMAT_ERROR;
								break;
							}
							if (dpId <= 0 ||
								(dpId > device->GetNumElement() &&
								 !IsSingleCurtainSceneDp(device, dpId) &&
								 !IsLightSceneDp(device, dpId)))
							{
								LOGW("Invalid scene action target: device=%s dp=%d",
									 deviceId.c_str(), dpId);
								result = CODE_ERROR;
							}
							else
							{
								uint16_t elementAddr =
									GetSceneActionElement(device, dpId);
								result =
									BleProtocol::getInstance()->SetDeviceActionScene(
										state, elementAddr, addr, device->GetType(),
										value, dpId);
							}
							if (result != CODE_OK)
								break;
						}
					}
					action["state"] = result == CODE_OK ? 0 : -1;
				}
			}
			if (!hasDeleteOperation && sceneData.isMember("state") &&
				sceneData["state"].isBool())
			{
				int result = BleProtocol::getInstance()->SetLocalSceneState(
					addr, sceneData["state"].asBool());
				if (result != CODE_OK)
				{
					LOGW("Cannot update state for local scene %s", id.c_str());
				}
			}
			scene->SetData(sceneData);
			return scene;
		}
	}
	return NULL;
}

Scene *SceneManager::GetSceneFromId(string id)
{
	sceneListMtx.lock();
	if (sceneList.find(id) != sceneList.end())
	{
		sceneListMtx.unlock();
		return sceneList[id];
	}
	sceneListMtx.unlock();
	return NULL;
}

Scene *SceneManager::GetSceneFromAddr(uint16_t addr)
{
	sceneListMtx.lock();
	for (const auto &[id, scene] : sceneList)
	{
		if (scene->GetAddr() == addr)
		{
			sceneListMtx.unlock();
			return scene;
		}
	}
	sceneListMtx.unlock();
	return NULL;
}

uint16_t SceneManager::GetNextSceneAddr()
{
	uint16_t sceneAddr = 1;
	sceneListMtx.lock();
	for (const auto &[id, scene] : sceneList)
	{
		if (scene->GetAddr() >= sceneAddr && scene->GetAddr() < 4096)
		{
			sceneAddr = scene->GetAddr() + 1;
		}
	}
	sceneListMtx.unlock();
	return sceneAddr;
}

void SceneManager::AddScene(Scene *scene)
{
	if (scene)
	{
		sceneListMtx.lock();
		sceneList[scene->GetId()] = scene;
		sceneListMtx.unlock();
	}
}

void SceneManager::DelScene(Scene *scene)
{
	sceneListMtx.lock();
	sceneList.erase(scene->GetId());
	sceneListMtx.unlock();
	delete scene;
}

void SceneManager::DelAllScene()
{
	sceneListMtx.lock();
	for (auto &[id, scene] : sceneList)
		delete scene;
	sceneList.clear();
	sceneListMtx.unlock();
}

void SceneManager::PrintScene()
{
	sceneListMtx.lock();
	for (auto &[id, sce] : sceneList)
	{
		LOGD("scene: %s", id.c_str());
		for (auto &dev : sce->deviceList)
		{
			LOGD("\tdev:%s: %d", dev->device->GetId().c_str(), dev->device->GetAddr());
		}
	}
	sceneListMtx.unlock();
}

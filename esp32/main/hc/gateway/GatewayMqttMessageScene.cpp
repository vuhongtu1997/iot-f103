#include "Gateway.h"
#include "Log.h"
#include "Database.h"
#include "BleProtocol.h"
#include <unordered_set>
#include <algorithm>

#ifdef CONFIG_ENABLE_BLE
// #include "DeviceBleSeftPowerRemote.h"
#endif

namespace
{
	void BroadcastSceneActionStatus(Scene *scene)
	{
		if (!scene)
			return;

		Json::Value sceneData = scene->GetData();
		if (!sceneData.isObject() || !sceneData.isMember("actions") ||
			!sceneData["actions"].isArray())
		{
			return;
		}

		for (auto &action : sceneData["actions"])
		{
			if (!action.isObject() ||
				!action.isMember("entityID") || !action["entityID"].isString() ||
				!action.isMember("executorProperty") ||
				!action["executorProperty"].isObject())
			{
				continue;
			}

			string deviceId = action["entityID"].asString();
			Device *device = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
			if (!device)
				continue;

			LOGD("Broadcast device on/off status after scene action");
			BleProtocol::getInstance()->GetDevicesOnOffBroadcast();
			return;
		}
	}
}

void Gateway::InitMqttMessageScene()
{
	// OnDeviceRpcCallbackRegister(TYPE_CTR_SCENE, bind(&Gateway::OnControlScene, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister(TYPE_ADD_SCENE, bind(&Gateway::OnCreateScene, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("addDevToScene", bind(&Gateway::OnAddDevToScene, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("delDevFromScene", bind(&Gateway::OnDelDevToScene, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("editScene", bind(&Gateway::OnEditScene, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("delScene", bind(&Gateway::OnDeleteScene, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("updateSceneName", bind(&Gateway::OnUpdateSceneName, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("createSceneController", bind(&Gateway::OnCreateSceneController, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("delSceneController", bind(&Gateway::OnDelSceneController, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("getSceneList", bind(&Gateway::OnGetSceneList, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("getDevListInScene", bind(&Gateway::OnGetDevListInScene, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("addFavoriteScene", bind(&Gateway::OnAddFavoriteScene, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("delFavoriteScene", bind(&Gateway::OnDelFavoriteScene, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("getFavoriteScene", bind(&Gateway::OnGetFavoriteScene, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("scenes", bind(&Gateway::OnScenesSync, this, placeholders::_1, placeholders::_2, placeholders::_3));
}

int Gateway::ThreadRspCallScene(Scene *scene, int timeout)
{
	if (!scene)
		return CODE_ERROR;
	if (timeout > 0)
	{
		Json::Value rspValuePush = Json::objectValue;
		for (int i = 0; i < timeout; i++)
		{
			sleep(1);
			Json::Value rspValue = Json::objectValue;
			rspValue["method"] = "deviceUpdate";
			map<string, Json::Value> tempListRsp;
			scene->listRspMtx.lock();
			if (!scene->listRsp.empty())
			{
				tempListRsp.swap(scene->listRsp);
			}
			scene->listRspMtx.unlock();

			if (tempListRsp.empty())
			{
				continue;
			}
			for (auto &[id, data] : tempListRsp)
			{
				rspValue["params"][id] = data;
			}
			rspValue["params"]["sceneId"] = scene->GetId();

			if (rspValue["params"] != rspValuePush["params"])
			{
				rspValuePush = rspValue;
				// CloudProtocol::DeviceRpcRequest(rspValuePush.toString());
			}
		}

		listControlObjMutex.lock();
		if (listControlObj.find(scene->GetId()) != listControlObj.end())
		{
			listControlObj.erase(scene->GetId());
		}
		listControlObjMutex.unlock();
	}
	return CODE_OK;
}

int Gateway::OnControlScene(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGD("OnControlScene");
	string sceneId = "";
	for (auto &key : reqValue.getMemberNames())
	{
		Json::Value scene = reqValue[key];
		if (scene.isObject())
		{
			sceneId = key;
			int id = (int)strtol(sceneId.c_str(), NULL, 16);
			Scene *scene = SceneManager::getInstance()->GetSceneFromAddr(id);
			if (scene)
			{
				int rs = scene->Do(false);
				SLEEP_MS(500);
				BroadcastSceneActionStatus(scene);
			}
			else
			{
				LOGW("Scene %s dose not exsit", sceneId.c_str());
			}
			break;
		}
	}
	return CODE_OK;
}

int Gateway::OnCreateScene(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	if (reqValue.isMember("id") && reqValue["id"].isString() &&
		reqValue.isMember("name") && reqValue["name"].isString() &&
		reqValue.isMember("devices") && reqValue["devices"].isArray())
	{
		Json::Value dataRsp;
		dataRsp["method"] = "createSceneRsp";
		dataRsp["params"] = reqValue;
		dataRsp["params"]["code"] = CODE_OK;
		CloudProtocol::Publish(topicRsp, dataRsp.toString());

		// int msgId = CloudProtocol::GetMsgId();
		// CloudProtocol::SetMsgId(++msgId);
		// topicRsp = "v1/devices/me/rpc/request/" + to_string(CloudProtocol::GetMsgId());

		Json::Value successList = Json::arrayValue;
		Json::Value failedList = Json::arrayValue;
		string sceneId = reqValue["id"].asString();
		string sceneName = reqValue["name"].asString();
		Json::Value deviceList = reqValue["devices"];
		uint16_t roomAdd = 0;

		Scene *scene = SceneManager::getInstance()->GetSceneFromId(sceneId);
		if (!scene)
		{
			scene = new Scene(sceneId, SceneManager::getInstance()->GetNextSceneAddr(), sceneName);
			if (scene)
			{
				SceneManager::getInstance()->AddScene(scene);
				Database::getInstance()->SceneAdd(scene);
			}
		}

		if (scene)
		{
			if (reqValue.isMember("room") && reqValue["room"].isString())
			{
				string roomId = reqValue["room"].asString();
				Room *room = RoomManager::getInstance()->GetRoomFromId(roomId);
				if (room)
				{
					respValue["params"]["room"] = room->GetId();
					roomAdd = room->GetAddr();
					room->AddScene(scene);
					Database::getInstance()->SceneUpdateRoom(scene, roomId);
				}
			}

			for (auto &deviceValue : deviceList)
			{
				if (deviceValue.isObject() &&
					deviceValue.isMember("id") && deviceValue["id"].isString() &&
					deviceValue.isMember("data") && deviceValue["data"].isObject())
				{
					Json::Value deviceProperties = deviceValue["data"];
					string deviceId = deviceValue["id"].asString();
					Device *device = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
					if (device)
					{
						if (device->AddToScene(scene->GetAddr(), deviceProperties, roomAdd) == CODE_OK)
						{
							scene->AddDevice(device, deviceProperties);
							Database::getInstance()->DeviceInSceneAdd(scene, device, deviceProperties.toString());
							Json::Value sceneRs = Json::objectValue;
							sceneRs["id"] = device->GetId();
							sceneRs["data"] = deviceProperties;
							successList.append(sceneRs);
						}
						else
						{
							device->RemoveFromScene(scene->GetAddr(), deviceProperties, roomAdd);
							failedList.append(device->GetId());
						}
					}
					SLEEP_MS(100);
				}
			}
			respValue["params"]["code"] = CODE_OK;
			respValue["params"]["id"] = scene->GetId();
			respValue["params"]["name"] = scene->GetName();
			respValue["params"]["success"] = successList;
			respValue["params"]["failed"] = failedList;

			SceneManager::getInstance()->PrintScene();
		}
		else
		{
			respValue["params"]["code"] = CODE_MEMORY_ERROR;
		}
	}
	else
	{
		respValue["params"]["code"] = CODE_FORMAT_ERROR;
	}
	respValue["method"] = "createSceneRspComplete";
	return CODE_OK;
}

int Gateway::OnAddDevToScene(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	if (reqValue.isMember("id") && reqValue["id"].isString() &&
		reqValue.isMember("name") && reqValue["name"].isString() &&
		reqValue.isMember("devices") && reqValue["devices"].isArray())
	{
		Json::Value dataRsp;
		dataRsp["method"] = "addDevToSceneRsp";
		dataRsp["params"]["code"] = CODE_OK;
		CloudProtocol::Publish(topicRsp, dataRsp.toString());

		// int msgId = CloudProtocol::GetMsgId();
		// CloudProtocol::SetMsgId(++msgId);
		// topicRsp = "v1/devices/me/rpc/request/" + to_string(CloudProtocol::GetMsgId());

		Json::Value successList = Json::arrayValue;
		Json::Value failedList = Json::arrayValue;
		Json::Value deviceList = reqValue["devices"];
		string sceneId = reqValue["id"].asString();
		string sceneName = reqValue["name"].asString();
		uint16_t roomAddr = 0;

		Scene *scene = SceneManager::getInstance()->GetSceneFromId(sceneId);
		if (!scene)
		{
			scene = new Scene(sceneId, SceneManager::getInstance()->GetNextSceneAddr(), sceneName);
			if (scene)
			{
				SceneManager::getInstance()->AddScene(scene);
				Database::getInstance()->SceneAdd(scene);
			}
		}
		if (scene)
		{
			if (reqValue.isMember("roomId") && reqValue["roomId"].isString())
			{
				string roomId = reqValue["roomId"].asString();
				Room *room = RoomManager::getInstance()->GetRoomFromId(roomId);
				if (room)
				{
					respValue["params"]["room"] = room->GetId();
					roomAddr = room->GetAddr();
					room->AddScene(scene);
					Database::getInstance()->SceneUpdateRoom(scene, roomId);
				}
			}
			for (auto &deviceValue : deviceList)
			{
				if (deviceValue.isObject() &&
					deviceValue.isMember("id") && deviceValue["id"].isString() &&
					deviceValue.isMember("data") && deviceValue["data"].isObject())
				{
					Json::Value deviceProperties = deviceValue["data"];
					string deviceId = deviceValue["id"].asString();
					Device *device = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
					if (device)
					{
						if (device->AddToScene(scene->GetAddr(), deviceProperties, roomAddr) == CODE_OK)
						{
							scene->AddDevice(device, deviceProperties);
							Database::getInstance()->DeviceInSceneAdd(scene, device, deviceProperties.toString());
							Json::Value sceneRs = Json::objectValue;
							sceneRs["id"] = device->GetId();
							sceneRs["data"] = deviceProperties;
							successList.append(sceneRs);
						}
						else
						{
							device->RemoveFromScene(scene->GetAddr(), deviceProperties, roomAddr);
							failedList.append(device->GetId());
						}
					}
				}
				SLEEP_MS(100);
			}
			respValue["params"]["code"] = CODE_OK;
			respValue["params"]["id"] = scene->GetId();
			respValue["params"]["name"] = scene->GetName();
			respValue["params"]["success"] = successList;
			respValue["params"]["failed"] = failedList;

			SceneManager::getInstance()->PrintScene();
		}
		else
		{
			respValue["params"]["code"] = CODE_NOT_FOUND_SCENE;
		}
	}
	else
	{
		respValue["params"]["code"] = CODE_FORMAT_ERROR;
	}
	respValue["method"] = "addDevToSceneRspComplete";
	return CODE_OK;
}

int Gateway::OnDelDevToScene(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	if (reqValue.isMember("id") && reqValue["id"].isString() &&
		reqValue.isMember("devices") && reqValue["devices"].isArray())
	{
		Json::Value dataRsp;
		dataRsp["method"] = "delDevFromSceneRsp";
		dataRsp["params"]["code"] = CODE_OK;
		CloudProtocol::Publish(topicRsp, dataRsp.toString());

		// int msgId = CloudProtocol::GetMsgId();
		// CloudProtocol::SetMsgId(++msgId);
		// topicRsp = "v1/devices/me/rpc/request/" + to_string(CloudProtocol::GetMsgId());

		Json::Value successList = Json::arrayValue;
		Json::Value failedList = Json::arrayValue;
		Json::Value deviceList = reqValue["devices"];
		string sceneId = reqValue["id"].asString();
		Scene *scene = SceneManager::getInstance()->GetSceneFromId(sceneId);
		if (scene)
		{
			uint16_t roomAddr = 0;
			Room *room = RoomManager::getInstance()->GetRoomSceneIn(scene);
			if (room)
			{
				roomAddr = room->GetAddr();
			}
			for (auto &deviceValue : deviceList)
			{
				if (deviceValue.isObject() &&
					deviceValue.isMember("id") && deviceValue["id"].isString() &&
					deviceValue.isMember("data") && deviceValue["data"].isObject())
				{
					Json::Value deviceProperties = deviceValue["data"];
					string deviceId = deviceValue["id"].asString();
					Device *device = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
					if (device)
					{
						int prositionDevInScene = scene->GetPositionDevice(device);
						if (prositionDevInScene > -1)
						{
							if (device->RemoveFromScene(scene->GetAddr(), scene->deviceList[prositionDevInScene]->data, roomAddr) == CODE_OK)
							{
								scene->DelDevice(device);
								Database::getInstance()->DeviceInSceneDel(scene, device);
								successList.append(deviceId);
							}
							else
							{
								failedList.append(deviceId);
							}
						}
						else
						{
							LOGW("Device %s not in scene %s", deviceId.c_str(), sceneId.c_str());
							failedList.append(deviceId);
						}
					}
					SLEEP_MS(100);
				}
			}

			respValue["params"]["code"] = CODE_OK;
			respValue["params"]["id"] = scene->GetId();
			respValue["params"]["success"] = successList;
			respValue["params"]["failed"] = failedList;

			SceneManager::getInstance()->PrintScene();
		}
		else
		{
			respValue["params"]["code"] = CODE_NOT_FOUND_SCENE;
		}
	}
	else
	{
		respValue["params"]["code"] = CODE_FORMAT_ERROR;
	}
	respValue["method"] = "delDevFromSceneRspComplete";
	return CODE_OK;
}

int Gateway::OnEditScene(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGD("EditScene");
	if (reqValue.isMember("id") && reqValue["id"].isString() &&
		reqValue.isMember("name") && reqValue["name"].isString() &&
		reqValue.isMember("devices") && reqValue["devices"].isArray())
	{
		Json::Value dataRsp;
		dataRsp["method"] = "editSceneRsp";
		dataRsp["params"] = reqValue;
		dataRsp["params"]["code"] = CODE_OK;
		CloudProtocol::Publish(topicRsp, dataRsp.toString());

		// int msgId = CloudProtocol::GetMsgId();
		// CloudProtocol::SetMsgId(++msgId);
		// topicRsp = "v1/devices/me/rpc/request/" + to_string(CloudProtocol::GetMsgId());

		Json::Value successList = Json::arrayValue;
		Json::Value failedList = Json::arrayValue;
		Json::Value successDelList = Json::arrayValue;
		Json::Value failedDelList = Json::arrayValue;
		string sceneId = reqValue["id"].asString();
		string sceneName = reqValue["name"].asString();
		uint16_t roomAddr = 0;
		Scene *scene = SceneManager::getInstance()->GetSceneFromId(sceneId);
		if (!scene)
		{
			scene = new Scene(sceneId, SceneManager::getInstance()->GetNextSceneAddr(), sceneName);
			if (scene)
			{
				SceneManager::getInstance()->AddScene(scene);
				Database::getInstance()->SceneAdd(scene);
			}
		}
		if (scene)
		{
			if (reqValue.isMember("room") && reqValue["room"].isString())
			{
				string roomId = reqValue["room"].asString();
				Room *room = RoomManager::getInstance()->GetRoomFromId(roomId);
				if (room)
				{
					respValue["params"]["room"] = room->GetId();
					roomAddr = room->GetAddr();
					room->AddScene(scene);
					Database::getInstance()->SceneUpdateRoom(scene, roomId);
				}
			}

			vector<Device *> devsInScene; // list dev in scene
			map<Device *, Json::Value> dataDevsInScene;
			for (auto &devs : scene->deviceList)
			{
				devsInScene.push_back(devs->device);
				dataDevsInScene[devs->device] = devs->data;
			}
			// vector<Device *> listDevicesDel; // list old dev del scene
			vector<Device *> devsDelFromScene;
			// vector<Device *> devsEditScene;	 // list dev edit scene
			vector<Device *> devsEditScene;

			Json::Value deviceList = reqValue["devices"];
			map<Device *, Json::Value> listData;
			for (auto &deviceValue : deviceList)
			{
				if (deviceValue.isObject() &&
					deviceValue.isMember("id") && deviceValue["id"].isString() &&
					deviceValue.isMember("data") && deviceValue["data"].isObject())
				{
					Json::Value deviceProperties = deviceValue["data"];
					string deviceId = deviceValue["id"].asString();
					Device *device = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
					if (device)
					{
						listData[device] = deviceProperties;
						devsEditScene.push_back(device);
					}
				}
			}

			// Find list device del scene
			for (auto &item : devsInScene)
			{
				if (find(devsEditScene.begin(), devsEditScene.end(), item) == devsEditScene.end())
				{
					devsDelFromScene.push_back(item);
				}
			}

			// Phai xoa list device delscene truoc
			for (auto &item : devsDelFromScene)
			{
				if (item->RemoveFromScene(scene->GetAddr(), dataDevsInScene[item]) == CODE_OK)
				{
					scene->DelDevice(item);
					Database::getInstance()->DeviceInSceneDel(scene, item);
					Json::Value sceneRs = Json::objectValue;
					sceneRs["id"] = item->GetId();
					sceneRs["data"] = dataDevsInScene[item];
					successDelList.append(sceneRs);
				}
				else
				{
					failedDelList.append(item->GetId());
				}
				SLEEP_MS(100);
			}

			for (auto &item : devsEditScene)
			{
				// Check edit scene nếu là công tắc ble phải xóa data trước
				if (item->EditInScene(scene->GetAddr(), dataDevsInScene[item]) == CODE_OK)
				{
					scene->DelDevice(item);
					Database::getInstance()->DeviceInSceneDel(scene, item);
				}
				if (item->AddToScene(scene->GetAddr(), listData[item], roomAddr) == CODE_OK)
				{
					scene->AddDevice(item, listData[item]);
					Database::getInstance()->DeviceInSceneAdd(scene, item, listData[item].toString());
					Json::Value sceneRs = Json::objectValue;
					sceneRs["id"] = item->GetId();
					sceneRs["data"] = listData[item];
					successList.append(sceneRs);
				}
				else
				{
					item->RemoveFromScene(scene->GetAddr(), listData[item], roomAddr);
					failedList.append(item->GetId());
				}
				SLEEP_MS(100);
			}

			respValue["params"]["code"] = CODE_OK;
			respValue["params"]["id"] = scene->GetId();
			respValue["params"]["name"] = scene->GetName();
			respValue["params"]["success"] = successList;
			respValue["params"]["failed"] = failedList;
			respValue["params"]["successDel"] = successDelList;
			respValue["params"]["failedDel"] = failedDelList;

			SceneManager::getInstance()->PrintScene();
		}
		else
		{
			respValue["params"]["code"] = CODE_NOT_FOUND_SCENE;
		}
	}
	else
	{
		respValue["params"]["code"] = CODE_FORMAT_ERROR;
	}
	respValue["method"] = "editSceneRspComplete";
	return CODE_OK;
}

int Gateway::OnDeleteScene(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	if (reqValue.isMember("id") && reqValue["id"].isString())
	{
		Json::Value successList = Json::arrayValue;
		Json::Value failedList = Json::arrayValue;
		string sceneId = reqValue["id"].asString();
		mtxDelScene.lock();
		Scene *scene = SceneManager::getInstance()->GetSceneFromId(sceneId);
		if (scene)
		{
			uint16_t roomAddr = 0;
			Room *room = RoomManager::getInstance()->GetRoomSceneIn(scene);
			if (room)
			{
				roomAddr = room->GetAddr();
			}
			vector<DeviceInScene *> devicesInScene = scene->deviceList;
			for (auto &deviceInScene : devicesInScene)
			{
				if (deviceInScene->device->RemoveFromScene(scene->GetAddr(), deviceInScene->data, roomAddr) == CODE_OK)
				{
					scene->DelDevice(deviceInScene->device);
					Database::getInstance()->DeviceInSceneDel(scene, deviceInScene->device);
					successList.append(deviceInScene->device->GetId());
				}
				else
				{
					failedList.append(deviceInScene->device->GetId());
				}
				SLEEP_MS(100);
			}

			respValue["params"]["code"] = CODE_OK;
			respValue["params"]["id"] = sceneId;
			respValue["params"]["success"] = successList;
			respValue["params"]["failed"] = failedList;

			if (scene->deviceList.size() <= 0)
			{
				vector<Room *> listRoom;
				RoomManager::getInstance()->ForEach([&](Room *room)
													{ listRoom.push_back(room); });
				for (auto &room : listRoom)
				{
					if (room && room->IsSceneInRoom(scene))
					{
						room->DelScene(scene);
					}
				}

				Database::getInstance()->SceneDel(scene);
				SceneManager::getInstance()->DelScene(scene);
			}
			SceneManager::getInstance()->PrintScene();
		}
		else
		{
			respValue["params"]["code"] = CODE_NOT_FOUND_SCENE;
		}
		mtxDelScene.unlock();
	}
	else
	{
		respValue["params"]["code"] = CODE_FORMAT_ERROR;
	}
	respValue["method"] = "delSceneRsp";
	return CODE_OK;
}

int Gateway::OnUpdateSceneName(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	if (reqValue.isMember("name") && reqValue["name"].isString() && reqValue.isMember("id") && reqValue["id"].isString())
	{
		string id = reqValue["id"].asString();
		string name = reqValue["name"].asString();
		Scene *scene = SceneManager::getInstance()->GetSceneFromId(id);
		if (scene)
		{
			scene->SetName(name);
			Database::getInstance()->SceneUpdate(scene);
		}
	}
	respValue["params"]["code"] = CODE_OK;
	respValue["method"] = "updateSceneNameRsp";
	return CODE_OK;
}

int Gateway::OnGetSceneList(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGW("OnGetSceneList");
	return CODE_OK;
}

int Gateway::OnGetDevListInScene(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGD("OnGetDevListInScene");
	if (reqValue.isMember("scenes") && reqValue["scenes"].isArray() && reqValue["scenes"].size() > 0)
	{
		Json::Value scenesList = Json::arrayValue;
		Json::Value scenes = reqValue["scenes"];
		for (auto &sceneValue : scenes)
		{
			Json::Value scenesData = Json::objectValue;
			if (sceneValue.isString())
			{
				string sceneId = sceneValue.asString();
				scenesData["id"] = sceneId;
				Scene *temp_scene = SceneManager::getInstance()->GetSceneFromId(sceneId);
				if (temp_scene)
				{
					Json::Value temp_devicesList = Json::arrayValue;
					for (unsigned int i = 0; i < temp_scene->deviceList.size(); i++)
					{
						DeviceInScene *deviceInScene = temp_scene->deviceList[i];
						string deviceId = deviceInScene->device->GetId();
						temp_devicesList.append(deviceId);
					}
					scenesData["devices"] = temp_devicesList;
				}
				else
					respValue["params"]["code"] = CODE_NOT_FOUND_ROOM;
			}
			else
				respValue["params"]["code"] = CODE_FORMAT_ERROR;
			scenesList.append(scenesData);
		}
		respValue["params"]["scenes"] = scenesList;
	}
	respValue["params"]["code"] = CODE_OK;
	respValue["method"] = "getDevListInScene";
	return CODE_OK;
}

int Gateway::ConfigSceneForSeftPowerRemote(Device *device, Json::Value &data, Json::Value &scene, bool isAddScene)
{
#ifdef CONFIG_ENABLE_BLE
	if (!device)
		return CODE_ERROR;
	if (data.isArray() && scene.isObject())
	{
		int result = CODE_OK;

		if (scene.isMember("id") && scene["id"].isString())
		{
			string sceneId = scene["id"].asString();
			Scene *scene = SceneManager::getInstance()->GetSceneFromId(sceneId);
			if (scene)
			{
				uint16_t btnValue = 0;

				for (auto &dt : data)
				{
					if (dt.isObject())
					{
						// if (device->GetType() == BLE_SEFTPOWER_REMOTE_1 || device->GetType() == BLE_SEFTPOWER_REMOTE_2 || device->GetType() == BLE_SEFTPOWER_REMOTE_3)
						// {
						// 	if (dt.isMember("modePress1") && dt["modePress1"].isInt())
						// 		btnValue = 1;
						// 	else if (dt.isMember("modePress2") && dt["modePress2"].isInt())
						// 		btnValue = 2;
						// 	else if (dt.isMember("modePress3") && dt["modePress3"].isInt())
						// 		btnValue = 4;
						// 	else if (dt.isMember("modePress4") && dt["modePress4"].isInt())
						// 		btnValue = 8;
						// 	else if (dt.isMember("modePress12") && dt["modePress12"].isInt())
						// 		btnValue = 3;
						// 	else if (dt.isMember("modePress13") && dt["modePress13"].isInt())
						// 		btnValue = 5;
						// 	else if (dt.isMember("modePress14") && dt["modePress14"].isInt())
						// 		btnValue = 9;
						// 	else if (dt.isMember("modePress23") && dt["modePress23"].isInt())
						// 		btnValue = 6;
						// 	else if (dt.isMember("modePress24") && dt["modePress24"].isInt())
						// 		btnValue = 10;
						// 	else if (dt.isMember("modePress34") && dt["modePress34"].isInt())
						// 		btnValue = 12;
						// 	else if (dt.isMember("modePress5") && dt["modePress5"].isInt())
						// 		btnValue = 16;
						// 	else if (dt.isMember("modePress6") && dt["modePress6"].isInt())
						// 		btnValue = 32;
						// 	else if (dt.isMember("modePress45") && dt["modePress45"].isInt())
						// 		btnValue = 24;
						// 	else if (dt.isMember("modePress56") && dt["modePress56"].isInt())
						// 		btnValue = 48;

						// 	// DeviceBleSeftPowerRemote *deviceBleSeftPowerRemote = dynamic_cast<DeviceBleSeftPowerRemote *>(device);
						// 	// if (deviceBleSeftPowerRemote)
						// 	// {
						// 	// 	Device *parent = deviceBleSeftPowerRemote->GetParent();
						// 	// 	if (parent)
						// 	// 	{
						// 	// 		if (isAddScene)
						// 	// 		{
						// 	// 			if (BleProtocol::getInstance()->SetSceneSeftPowerRemote(parent->GetAddr(), deviceBleSeftPowerRemote->GetAddr(), btnValue, 1, scene->GetAddr()) != CODE_OK)
						// 	// 				result = CODE_ERROR;
						// 	// 		}
						// 	// 		else
						// 	// 		{
						// 	// 			if (BleProtocol::getInstance()->DelSceneSeftPowerRemote(parent->GetAddr(), deviceBleSeftPowerRemote->GetAddr(), btnValue, 1) != CODE_OK)
						// 	// 				result = CODE_ERROR;
						// 	// 		}
						// 	// 	}
						// 	// 	else
						// 	// 		LOGW("Parent is null");
						// 	// }
						// 	else
						// 		LOGW("device seftpower remote is null");
						// }
					}
				}
			}
			else
			{
				LOGW("SceneBle %s not found", sceneId.c_str());
				result = CODE_ERROR;
			}
		}
	}
	else
	{
		LOGW("Data error format");
	}
#endif
	return CODE_ERROR;
}

int Gateway::ConfigSceneForPirSensor(Device *device, Json::Value &data, Json::Value &scene, bool isAddScene)
{
#ifdef CONFIG_ENABLE_BLE
	if (!device)
		return CODE_ERROR;
	if (data.isArray() && scene.isObject())
	{
		int result = CODE_OK;
		if (scene.isMember("id") && scene["id"].isString())
		{
			string sceneId = scene["id"].asString();
			Scene *scene = SceneManager::getInstance()->GetSceneFromId(sceneId);
			if (scene)
			{
				if (isAddScene)
				{
					int pir = 0;
					uint16_t luxHigh = 0;
					uint16_t luxLow = 0;
					bool isPir = false;
					bool isLux = false;
					for (auto &dt : data)
					{
						if (dt.isObject())
						{
							if (dt.isMember("pir") && dt["pir"].isInt())
							{
								pir = dt["pir"].asInt();
								isPir = true;
							}
							if (dt.isMember("lux") && dt["lux"].isArray())
							{
								if (dt["lux"][0].isInt() && dt["lux"][1].isInt())
								{
									luxLow = dt["lux"][0].asInt();
									luxHigh = dt["lux"][1].asInt();
									isLux = true;
								}
							}
							if (isLux && isPir)
								if (BleProtocol::getInstance()->SetScenePirLightSensor(device->GetAddr(), 2, pir, luxLow / 10, luxHigh / 10, scene->GetAddr(), 0) != CODE_OK)
									result = CODE_ERROR;
						}
					}
				}
				else
				{
					for (auto &dt : data)
					{
						if (dt.isMember("pir") && dt["pir"].isInt())
						{
							if (BleProtocol::getInstance()->SetScenePirLightSensor(device->GetAddr(), 2, dt["pir"].asInt(), 0, 0, 0, 0) != CODE_OK)
								result = CODE_ERROR;
						}
					}
				}
			}
		}
		return result;
	}
	else
	{
		LOGW("Data error format");
		return CODE_ERROR;
	}
#endif
	return CODE_OK;
}

int Gateway::OnCreateSceneController(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	respValue["method"] = "createSceneControllerRsp";
	if (reqValue.isMember("devId") && reqValue["devId"].isString() &&
		reqValue.isMember("data") && reqValue["data"].isArray())
	{
		string deviceId = reqValue["devId"].asString();
		Json::Value dataJson = reqValue["data"];
		int result = CODE_OK;

		Device *device = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
		if (device)
		{
			for (auto &dt : dataJson)
			{
				if (dt.isObject() && dt.isMember("properties") && dt["properties"].isArray() && dt.isMember("scene") && dt["scene"].isObject())
				{
					Json::Value propertiesJson = dt["properties"];
					Json::Value sceneJson = dt["scene"];
					// int indexType1 = (device->GetType() >> 16) & 0xFF;
					// int indexType2 = (device->GetType() >> 8) & 0xFF;
					// if (indexType1 == 2 && indexType2 == 7) // seftpower remote
					// {
					// 	result = ConfigSceneForSeftPowerRemote(device, propertiesJson, sceneJson, true);
					// }
					// else if (indexType1 == 3 && indexType2 == 2)
					// {
					// 	result = ConfigSceneForPirSensor(device, propertiesJson, sceneJson, true);
					// }
				}
				else
				{
					LOGW("Data error");
				}
			}
			respValue["params"]["code"] = result;
		}
		else
		{
			respValue["params"]["code"] = CODE_NOT_FOUND_DEVICE;
		}
	}
	return CODE_OK;
}

int Gateway::OnDelSceneController(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	respValue["method"] = "delSceneControllerRsp";
	if (reqValue.isMember("devId") && reqValue["devId"].isString() &&
		reqValue.isMember("data") && reqValue["data"].isArray())
	{
		string deviceId = reqValue["devId"].asString();
		Json::Value dataJson = reqValue["data"];
		int result = CODE_OK;

		Device *device = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
		if (device)
		{
			for (auto &dt : dataJson)
			{
				if (dt.isObject() && dt.isMember("properties") && dt["properties"].isArray() && dt.isMember("scene") && dt["scene"].isObject())
				{
					Json::Value propertiesJson = dt["properties"];
					Json::Value sceneJson = dt["scene"];
					// int indexType1 = (device->GetType() >> 16) & 0xFF;
					// int indexType2 = (device->GetType() >> 8) & 0xFF;
					// if (indexType1 == 2 && indexType2 == 7) // seftpower remote
					// {
					// 	result = ConfigSceneForSeftPowerRemote(device, propertiesJson, sceneJson, false);
					// }
					// else if (indexType1 == 3 && indexType2 == 2)
					// {
					// 	result = ConfigSceneForPirSensor(device, propertiesJson, sceneJson, false);
					// }
				}
			}
			respValue["params"]["code"] = result;
		}
		else
		{
			respValue["params"]["code"] = CODE_NOT_FOUND_DEVICE;
		}
	}
	return CODE_OK;
}

int Gateway::OnAddFavoriteScene(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGD("OnAddFavoriteScene");
	if (reqValue.isMember("scenelist") && reqValue["scenelist"].isArray())
	{
		Json::Value scenes = reqValue["scenelist"];
		for (auto &temp : scenes)
		{
			if (temp.isString())
			{
				string temp_scene = temp.asString();
				Scene *scene = SceneManager::getInstance()->GetSceneFromId(temp_scene);
				if (scene)
				{
					scene->SetIsFavorite(true);
					Database::getInstance()->SceneUpdateFavorite(scene);
				}
			}
		}
	}
	respValue["params"]["code"] = CODE_OK;
	respValue["method"] = "addFavoriteSceneRsp";
	return CODE_OK;
}

int Gateway::OnDelFavoriteScene(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGW("OnDelFavoriteScene");
	respValue["params"]["code"] = CODE_OK;
	respValue["method"] = "delFavoriteSceneRsp";
	return CODE_OK;
}

int Gateway::OnGetFavoriteScene(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGW("OnGetFavoriteScene");
	respValue["params"]["code"] = CODE_OK;
	respValue["method"] = "getFavoriteSceneRsp";
	return CODE_OK;
}

int Gateway::OnScenesSync(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGD("OnScenesSync");

	std::unordered_set<std::string> syncSceneIds;

	if (reqValue.isArray())
	{
		for (const auto &value : reqValue)
		{
			if (value.isObject() &&
				value.isMember("id") && value["id"].isString() &&
				value.isMember("name") && value["name"].isString() &&
				value.isMember("listDevices") && value["listDevices"].isArray())
			{
				std::string sceneId = value["id"].asString();
				syncSceneIds.insert(sceneId);
			}
		}
	}

	std::vector<std::string> scenesToDelete;
	mtxDelScene.lock();
	SceneManager::getInstance()->ForEach([&](Scene *scene)
										 {
		if (scene && syncSceneIds.find(scene->GetId()) == syncSceneIds.end())
		{
			scenesToDelete.push_back(scene->GetId());
		} });

	for (const auto &sceneId : scenesToDelete)
	{
		Scene *scene = SceneManager::getInstance()->GetSceneFromId(sceneId);
		if (!scene)
			continue;

		for (auto &devInScene : scene->deviceList)
		{
			if (devInScene && devInScene->device)
			{
				devInScene->device->RemoveFromScene(scene->GetAddr(), devInScene->data);
				Database::getInstance()->DeviceInSceneDel(scene, devInScene->device);
			}
		}

		vector<Room *> listRoom;
		RoomManager::getInstance()->ForEach([&](Room *room)
											{ listRoom.push_back(room); });
		for (auto &room : listRoom)
		{
			if (room && room->IsSceneInRoom(scene))
			{
				room->DelScene(scene);
			}
		}

		Database::getInstance()->SceneDel(scene->GetId());
		SceneManager::getInstance()->DelScene(scene);
	}
	mtxDelScene.unlock();

	return CODE_NOT_RESPONSE;
}

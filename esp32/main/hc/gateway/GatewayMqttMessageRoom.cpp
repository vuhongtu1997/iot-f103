#include "Gateway.h"
#include "Log.h"
#include "Database.h"
#include <unordered_set>

void Gateway::InitMqttMessageRoom()
{
	// OnDeviceRpcCallbackRegister("getRoomList", bind(&Gateway::OnGetRoomList, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("getDevListInRoom", bind(&Gateway::OnGetDevListInRoom, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("createRoom", bind(&Gateway::OnCreateRoom, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("addDevToRoom", bind(&Gateway::OnAddDeviceToRoom, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("delDevFromRoom", bind(&Gateway::OnDeleteDeviceFromRoom, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("delRoom", bind(&Gateway::OnDeleteRoom, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("getGroupIntoRoom", bind(&Gateway::OnGetGroupIntoRoom, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("getSceneIntoRoom", bind(&Gateway::OnGetSceneIntoRoom, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("updateRoomName", bind(&Gateway::OnUpdateRoomName, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("checkRoom", bind(&Gateway::OnCheckRoom, this, placeholders::_1, placeholders::_2, placeholders::_3));
	// OnDeviceRpcCallbackRegister("rooms", bind(&Gateway::OnRoomsSync, this, placeholders::_1, placeholders::_2, placeholders::_3));
}

int Gateway::OnGetRoomList(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGW("OnGetRoomList");
	respValue["params"]["code"] = CODE_OK;
	respValue["method"] = "getRoomListRsp";
	return CODE_OK;
}

int Gateway::OnGetDevListInRoom(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGD("OnGetDeviceStatus");
	if (reqValue.isMember("rooms") && reqValue["rooms"].isArray() && reqValue["rooms"].size() > 0)
	{
		Json::Value roomsData = Json::arrayValue;
		Json::Value rooms = reqValue["rooms"];
		for (auto &roomValue : rooms)
		{
			Json::Value temp_roomsData;
			if (roomValue.isString())
			{
				string roomId = roomValue.asString();
				temp_roomsData["id"] = roomId;
				Room *temp_room = RoomManager::getInstance()->GetRoomFromId(roomId);
				if (temp_room)
				{
					Json::Value temp_devicesList = Json::arrayValue;
					vector<string> listDevId;
					bool isExist;
					for (unsigned int i = 0; i < temp_room->deviceList.size(); i++)
					{
						Json::Value device;
						DeviceInGroup *deviceInRoom = temp_room->deviceList[i];
						string deviceId = deviceInRoom->device->GetId();

						isExist = false;
						for (auto &id : listDevId)
						{
							if (deviceId == id)
							{
								isExist = true;
								break;
							}
						}

						if (!isExist)
						{
							listDevId.push_back(deviceId);
							Device *tempDev = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
							device["id"] = deviceId;
							device["name"] = tempDev->GetName();
							// device["type"] = (Json::Value::UInt)tempDev->GetType();
							temp_devicesList.append(device);
						}
					}
					temp_roomsData["devices"] = temp_devicesList;
					roomsData.append(temp_roomsData);
				}
				else
					respValue["params"]["code"] = CODE_NOT_FOUND_ROOM;
			}
			else
				respValue["params"]["code"] = CODE_FORMAT_ERROR;
		}
		respValue["params"]["rooms"] = roomsData;
	}
	else
		respValue["params"]["code"] = CODE_FORMAT_ERROR;
	respValue["params"]["code"] = CODE_OK;
	respValue["method"] = "getDevListInRoomRsp";
	return CODE_OK;
}

/**
 * Thêm thiết bị vào phòng
 * - Check type - version thiết bị để gửi thêm nhanh vào phòng
 * - Check type - version thiết bị để thêm vào nhóm thiết bị: nhóm đèn, nhóm công tắc, nhóm công tắc đèn
 * - Check type - version thiết bị để cấu hình scene
 */
int Gateway::DelDeviceFromAllRoom(string deviceId)
{
	LOGW("DelDeviceFromAllRoom");
	// for (const auto &[id, room] : roomList)
	// {
	// 	for (unsigned int i = 0; i < room->deviceList.size(); i++)
	// 	{
	// 		if (deviceId == room->deviceList[i]->device->GetId())
	// 		{
	// 			room->DelDevice(room->deviceList[i]->device, true, true);
	// 		}
	// 	}
	// }
	return CODE_OK;
}

int Gateway::OnCreateRoom(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGD("CreateRoom");
	respValue["method"] = "createRoomRspComplete";
	if (reqValue.isMember("id") && reqValue["id"].isString() &&
		reqValue.isMember("name") && reqValue["name"].isString() &&
		reqValue.isMember("devices") && reqValue["devices"].isArray() &&
		reqValue.isMember("groups") && reqValue["groups"].isArray() &&
		reqValue.isMember("scenes") && reqValue["scenes"].isArray())
	{

		Json::Value dataRsp;
		dataRsp["method"] = "createRoomRsp";
		dataRsp["params"] = reqValue;
		dataRsp["params"]["code"] = CODE_OK;
		CloudProtocol::Publish(topicRsp, dataRsp.toString());

		// int msgId = CloudProtocol::GetMsgId();
		// CloudProtocol::SetMsgId(++msgId);
		// topicRsp = "v1/devices/me/rpc/request/" + to_string(CloudProtocol::GetMsgId());

		string roomId = reqValue["id"].asString();
		string roomName = reqValue["name"].asString();
		Json::Value devicesValue = reqValue["devices"];
		Json::Value groupsValue = reqValue["groups"];
		Json::Value scenesValue = reqValue["scenes"];

		vector<Device *> devicesAddRoom;
		vector<Device *> listDevInRoom;
		Room *room = RoomManager::getInstance()->GetRoomFromId(roomId);
		if (!room)
		{
			uint16_t roomAddr = RoomManager::getInstance()->GetNextRoomAddr();
			room = new Room(roomId, roomAddr, roomName);
			if (room)
			{
				RoomManager::getInstance()->AddRoom(room);
				Database::getInstance()->RoomAdd(room);
			}
		}
		if (room)
		{
			Database::getInstance()->SqliteBenginTransaction();
			respValue["params"]["room"]["id"] = room->GetId();
			respValue["params"]["room"]["name"] = room->GetName();
			// Thêm thiết bị vào phòng
			for (auto &deviceValue : devicesValue)
			{
				if (deviceValue.isString())
				{
					string deviceId = deviceValue.asString();
					Device *device = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
					if (device)
					{
						if (device->AddToRoom(room->GetAddr()) == CODE_OK)
						{
							respValue["params"]["room"]["success"].append(device->GetId());
							listDevInRoom.push_back(device);
							if (device->GetProtocol() == BLE_DEVICE) // Kiểm tra thêm thiết bị BLE vào phòng
							{
								// int indexType1 = (device->GetType() >> 16) & 0xFF;
								// int indexType2 = (device->GetType() >> 8) & 0xFF;
								// if ((indexType1 == 1) ||				  // Đèn
								// 	(indexType1 == 2 && indexType2 == 1)) // công tắc chuyển mạch
								// {
								// 	room->AddDevice(device, device->GetAddr());
								// 	Database::getInstance()->DeviceInRoomAdd(room, device, device->GetAddr());
								// }
								// else if ((indexType1 == 2 && indexType2 == 2) ||
								// 		 (indexType1 == 2 && indexType2 == 4) ||
								// 		 (indexType1 == 2 && indexType2 == 6))
								// {
								// 	for (int i = 0; i < device->GetNumElement(); i++)
								// 	{
								// 		room->AddDevice(device, device->GetAddr() + i);
								// 		Database::getInstance()->DeviceInRoomAdd(room, device, device->GetAddr() + i);
								// 	}
								// }
							}
						}
						else
						{
							respValue["params"]["room"]["failed"].append(device->GetId());
						}
					}
					else
					{
						respValue["params"]["room"]["failed"].append(deviceId);
					}
					SLEEP_MS(100);
				}
			}

			RoomManager::getInstance()->PrintRoom();

			for (auto &groupValue : groupsValue)
			{
				if (groupValue.isObject() &&
					groupValue.isMember("id") && groupValue["id"].isString() &&
					groupValue.isMember("name") && groupValue["name"].isString() &&
					groupValue.isMember("type") && groupValue["type"].isString())
				{
					string groupId = groupValue["id"].asString();
					string groupName = groupValue["name"].asString();
					uint32_t groupType = Device::BleGroupIntByString(groupValue["type"].asString().c_str());
					Group *group = GroupManager::getInstance()->GetGroupFromId(groupId);
					if (!group)
					{
						group = new Group(groupId, room->GetAddr() + Device::BleTypeToGroupId(groupType), groupName);
						if (group)
						{
							GroupManager::getInstance()->AddGroup(group);
							Database::getInstance()->GroupAdd(group);
							room->AddGroup(group);
							Database::getInstance()->GroupUpdateRoom(group, room->GetId());
						}
					}
					if (group)
					{
						Json::Value groupRs = Json::objectValue;
						groupRs["id"] = group->GetId();
						groupRs["name"] = group->GetName();
						groupRs["type"] = (Json::UInt)groupType;
						for (auto &devInRoom : listDevInRoom)
						{
							if (devInRoom->AddToGroupInRoom(group->GetAddr(), groupType) == CODE_OK)
							{
								Json::Value dataDevInGroup = Json::objectValue;
								if (devInRoom->GetProtocol() == BLE_DEVICE)
								{
									// int indexType1 = (devInRoom->GetType() >> 16) & 0xFF;
									// int indexType2 = (devInRoom->GetType() >> 8) & 0xFF;
									// if ((indexType1 == 1) ||
									// 	(indexType1 == 2 && indexType2 == 1))
									// {
									// 	if (groupType == devInRoom->GetType())
									// 	{
									// 		dataDevInGroup["id"] = devInRoom->GetId();
									// 		dataDevInGroup["epId"].append(devInRoom->GetAddr());
									// 		groupRs["success"].append(dataDevInGroup);
									// 		group->AddDevice(devInRoom, devInRoom->GetAddr());
									// 		Database::getInstance()->DeviceInGroupAdd(group, devInRoom, devInRoom->GetAddr());
									// 	}
									// }
									// else if ((indexType1 == 2 && indexType2 == 2) ||
									// 		 (indexType1 == 2 && indexType2 == 4) ||
									// 		 (indexType1 == 2 && indexType2 == 6))
									// {
									// 	if (groupType == 1)
									// 	{
									// 		dataDevInGroup["id"] = devInRoom->GetId();
									// 		for (int i = 0; i < devInRoom->GetNumElement(); i++)
									// 		{
									// 			dataDevInGroup["epId"].append(devInRoom->GetAddr() + i);
									// 			group->AddDevice(devInRoom, devInRoom->GetAddr() + i);
									// 			Database::getInstance()->DeviceInGroupAdd(group, devInRoom, devInRoom->GetAddr() + i);
									// 		}
									// 		groupRs["success"].append(dataDevInGroup);
									// 	}
									// }
								}
							}
							else
							{
								groupRs["failed"].append(devInRoom->GetId());
							}
							SLEEP_MS(100);
						}
						respValue["params"]["groups"].append(groupRs);
					}
					else
					{
						LOGW("Group %s create error", groupId.c_str());
					}
				}
			}

			GroupManager::getInstance()->PrintGroup();

			int countScene = 0;
			for (auto &sceneValue : scenesValue)
			{
				if (sceneValue.isObject() &&
					sceneValue.isMember("id") && sceneValue["id"].isString() &&
					sceneValue.isMember("name") && sceneValue["name"].isString())
				{
					string sceneId = sceneValue["id"].asString();
					string sceneName = sceneValue["name"].asString();
					Scene *scene = SceneManager::getInstance()->GetSceneFromId(sceneId);
					if (!scene)
					{
						scene = new Scene(sceneId, room->GetAddr() + countScene, sceneName);
						if (scene)
						{
							countScene++;
							SceneManager::getInstance()->AddScene(scene);
							Database::getInstance()->SceneAdd(scene);
							room->AddScene(scene);
							Database::getInstance()->SceneUpdateRoom(scene, room->GetId());
						}
					}
					if (scene)
					{
						Json::Value sceneRs = Json::objectValue;
						sceneRs["id"] = scene->GetId();
						sceneRs["name"] = scene->GetName();
						if (sceneValue.isMember("groups") && sceneValue["groups"].isArray())
						{
							Json::Value groupsAddScene = sceneValue["groups"];
							for (auto &groupAddScene : groupsAddScene)
							{
								if (groupAddScene.isObject() &&
									groupAddScene.isMember("id") && groupAddScene["id"].isString() &&
									groupAddScene.isMember("data") && groupAddScene["data"].isObject())
								{
									string grpId = groupAddScene["id"].asString();
									Json::Value propertyDevInScene = groupAddScene["data"];
									Group *grp = GroupManager::getInstance()->GetGroupFromId(grpId);
									if (grp)
									{
										grp->Do(propertyDevInScene, false);
										for (auto &devInRoom : listDevInRoom)
										{
											if (grp->GetPositionDevice(devInRoom, devInRoom->GetAddr()) >= 0)
											{
												if (devInRoom->AddToSceneInRoom(scene->GetAddr(), propertyDevInScene, room->GetAddr()) == CODE_OK)
												{
													Json::Value dataDevInScene;
													dataDevInScene["id"] = devInRoom->GetId();
													dataDevInScene["data"] = propertyDevInScene;
													sceneRs["success"].append(dataDevInScene);
													scene->AddDevice(devInRoom, propertyDevInScene);
													Database::getInstance()->DeviceInSceneAdd(scene, devInRoom, propertyDevInScene.toString());
												}
												else
												{
													sceneRs["failed"].append(devInRoom->GetId());
												}
											}
											SLEEP_MS(100);
										}
									}
								}
							}
						}
						respValue["params"]["scenes"].append(sceneRs);
					}
					else
					{
						LOGW("Scene %s create error", sceneId.c_str());
					}
				}
			}
			Database::getInstance()->SqliteEndTransaction();
			SceneManager::getInstance()->PrintScene();

			respValue["params"]["code"] = CODE_OK;

			room->SetDataConfig(respValue.toString());
			Database::getInstance()->RoomUpdateData(room);
		}
		else
		{
			respValue["params"]["code"] = CODE_ERROR;
			LOGW("Room id %s exist", roomId.c_str());
		}
	}
	else
	{
		respValue["params"]["code"] = CODE_FORMAT_ERROR;
	}
	return CODE_OK;
}

int Gateway::OnAddDeviceToRoom(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGD("OnAddDeviceToRoom");
	respValue["method"] = "addDevToRoomRspComplete";
	if (reqValue.isMember("id") && reqValue["id"].isString() &&
		reqValue.isMember("name") && reqValue["name"].isString() &&
		reqValue.isMember("devices") && reqValue["devices"].isArray() &&
		reqValue.isMember("groups") && reqValue["groups"].isArray() &&
		reqValue.isMember("scenes") && reqValue["scenes"].isArray())
	{

		Json::Value dataRsp;
		dataRsp["method"] = "addDevToRoomRsp";
		dataRsp["params"] = reqValue;
		dataRsp["params"]["code"] = CODE_OK;
		CloudProtocol::Publish(topicRsp, dataRsp.toString());

		// int msgId = CloudProtocol::GetMsgId();
		// CloudProtocol::SetMsgId(++msgId);
		// topicRsp = "v1/devices/me/rpc/request/" + to_string(CloudProtocol::GetMsgId());

		string roomId = reqValue["id"].asString();
		string roomName = reqValue["name"].asString();
		Json::Value devicesValue = reqValue["devices"];
		Json::Value groupsValue = reqValue["groups"];
		Json::Value scenesValue = reqValue["scenes"];

		vector<Device *> devicesAddRoom;
		vector<Device *> listDevInRoom;
		Room *room = RoomManager::getInstance()->GetRoomFromId(roomId);
		if (!room)
		{
			uint16_t roomAddr = RoomManager::getInstance()->GetNextRoomAddr();
			room = new Room(roomId, roomAddr, roomName);
			if (room)
			{
				RoomManager::getInstance()->AddRoom(room);
				Database::getInstance()->RoomAdd(room);
			}
		}
		if (room)
		{
			Database::getInstance()->SqliteBenginTransaction();
			respValue["params"]["room"]["id"] = room->GetId();
			respValue["params"]["room"]["name"] = room->GetName();
			// Thêm thiết bị vào phòng
			for (auto &deviceValue : devicesValue)
			{
				if (deviceValue.isString())
				{
					string deviceId = deviceValue.asString();
					Device *device = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
					if (device)
					{
						if (device->AddToRoom(room->GetAddr()) == CODE_OK)
						{
							respValue["params"]["room"]["success"].append(device->GetId());
							listDevInRoom.push_back(device);
							if (device->GetProtocol() == BLE_DEVICE) // Kiểm tra thêm thiết bị BLE vào phòng
							{
								// int indexType1 = (device->GetType() >> 16) & 0xFF;
								// int indexType2 = (device->GetType() >> 8) & 0xFF;
								// if ((indexType1 == 1) ||				  // Đèn
								// 	(indexType1 == 2 && indexType2 == 1)) // công tắc chuyển mạch
								// {
								// 	room->AddDevice(device, device->GetAddr());
								// 	Database::getInstance()->DeviceInRoomAdd(room, device, device->GetAddr());
								// }
								// else if ((indexType1 == 2 && indexType2 == 2) ||
								// 		 (indexType1 == 2 && indexType2 == 4) ||
								// 		 (indexType1 == 2 && indexType2 == 6))
								// {
								// 	for (int i = 0; i < device->GetNumElement(); i++)
								// 	{
								// 		room->AddDevice(device, device->GetAddr() + i);
								// 		Database::getInstance()->DeviceInRoomAdd(room, device, device->GetAddr() + i);
								// 	}
								// }
							}
						}
						else
						{
							respValue["params"]["room"]["failed"].append(device->GetId());
						}
					}
					else
					{
						respValue["params"]["room"]["failed"].append(deviceId);
					}
					SLEEP_MS(100);
				}
			}

			RoomManager::getInstance()->PrintRoom();

			for (auto &groupValue : groupsValue)
			{
				if (groupValue.isObject() &&
					groupValue.isMember("id") && groupValue["id"].isString() &&
					groupValue.isMember("name") && groupValue["name"].isString() &&
					groupValue.isMember("type") && groupValue["type"].isString())
				{
					string groupId = groupValue["id"].asString();
					string groupName = groupValue["name"].asString();
					uint32_t groupType = Device::BleGroupIntByString(groupValue["type"].asString().c_str());
					Group *group = GroupManager::getInstance()->GetGroupFromId(groupId);
					if (!group)
					{
						group = new Group(groupId, room->GetAddr() + Device::BleTypeToGroupId(groupType), groupName);
						if (group)
						{
							GroupManager::getInstance()->AddGroup(group);
							Database::getInstance()->GroupAdd(group);
							room->AddGroup(group);
							Database::getInstance()->GroupUpdateRoom(group, room->GetId());
						}
					}
					if (group)
					{
						Json::Value groupRs = Json::objectValue;
						groupRs["id"] = group->GetId();
						groupRs["name"] = group->GetName();
						groupRs["type"] = (Json::UInt)groupType;
						for (auto &devInRoom : listDevInRoom)
						{
							if (devInRoom->AddToGroupInRoom(group->GetAddr(), groupType) == CODE_OK)
							{
								Json::Value dataDevInGroup = Json::objectValue;
								if (devInRoom->GetProtocol() == BLE_DEVICE)
								{
									// int indexType1 = (devInRoom->GetType() >> 16) & 0xFF;
									// int indexType2 = (devInRoom->GetType() >> 8) & 0xFF;
									// if ((indexType1 == 1) ||
									// 	(indexType1 == 2 && indexType2 == 1))
									// {
									// 	if (groupType == devInRoom->GetType())
									// 	{
									// 		dataDevInGroup["id"] = devInRoom->GetId();
									// 		dataDevInGroup["epId"].append(devInRoom->GetAddr());
									// 		groupRs["success"].append(dataDevInGroup);
									// 		group->AddDevice(devInRoom, devInRoom->GetAddr());
									// 		Database::getInstance()->DeviceInGroupAdd(group, devInRoom, devInRoom->GetAddr());
									// 	}
									// }
									// else if ((indexType1 == 2 && indexType2 == 2) ||
									// 		 (indexType1 == 2 && indexType2 == 4) ||
									// 		 (indexType1 == 2 && indexType2 == 6))
									// {
									// 	if (groupType == 1)
									// 	{
									// 		dataDevInGroup["id"] = devInRoom->GetId();
									// 		for (int i = 0; i < devInRoom->GetNumElement(); i++)
									// 		{
									// 			dataDevInGroup["epId"].append(devInRoom->GetAddr() + i);
									// 			group->AddDevice(devInRoom, devInRoom->GetAddr() + i);
									// 			Database::getInstance()->DeviceInGroupAdd(group, devInRoom, devInRoom->GetAddr() + i);
									// 		}
									// 		groupRs["success"].append(dataDevInGroup);
									// 	}
									// }
								}
							}
							else
							{
								groupRs["failed"].append(devInRoom->GetId());
							}
							SLEEP_MS(100);
						}
						respValue["params"]["groups"].append(groupRs);
					}
					else
					{
						LOGW("Group %s create error", groupId.c_str());
					}
				}
			}

			GroupManager::getInstance()->PrintGroup();

			int countScene = 0;
			for (auto &sceneValue : scenesValue)
			{
				if (sceneValue.isObject() &&
					sceneValue.isMember("id") && sceneValue["id"].isString() &&
					sceneValue.isMember("name") && sceneValue["name"].isString())
				{
					string sceneId = sceneValue["id"].asString();
					string sceneName = sceneValue["name"].asString();
					Scene *scene = SceneManager::getInstance()->GetSceneFromId(sceneId);
					if (!scene)
					{
						scene = new Scene(sceneId, room->GetAddr() + countScene, sceneName);
						if (scene)
						{
							countScene++;
							SceneManager::getInstance()->AddScene(scene);
							Database::getInstance()->SceneAdd(scene);
							room->AddScene(scene);
							Database::getInstance()->SceneUpdateRoom(scene, room->GetId());
						}
					}
					if (scene)
					{
						Json::Value sceneRs = Json::objectValue;
						sceneRs["id"] = scene->GetId();
						sceneRs["name"] = scene->GetName();
						if (sceneValue.isMember("groups") && sceneValue["groups"].isArray())
						{
							Json::Value groupsAddScene = sceneValue["groups"];
							for (auto &groupAddScene : groupsAddScene)
							{
								if (groupAddScene.isObject() &&
									groupAddScene.isMember("id") && groupAddScene["id"].isString() &&
									groupAddScene.isMember("data") && groupAddScene["data"].isObject())
								{
									string grpId = groupAddScene["id"].asString();
									Json::Value propertyDevInScene = groupAddScene["data"];
									Group *grp = GroupManager::getInstance()->GetGroupFromId(grpId);
									if (grp)
									{
										grp->Do(propertyDevInScene, false);
										for (auto &devInRoom : listDevInRoom)
										{
											if (grp->GetPositionDevice(devInRoom, devInRoom->GetAddr()) >= 0)
											{
												if (devInRoom->AddToSceneInRoom(scene->GetAddr(), propertyDevInScene, room->GetAddr()) == CODE_OK)
												{
													Json::Value dataDevInScene;
													dataDevInScene["id"] = devInRoom->GetId();
													dataDevInScene["data"] = propertyDevInScene;
													sceneRs["success"].append(dataDevInScene);
													scene->AddDevice(devInRoom, propertyDevInScene);
													Database::getInstance()->DeviceInSceneAdd(scene, devInRoom, propertyDevInScene.toString());
												}
												else
												{
													sceneRs["failed"].append(devInRoom->GetId());
												}
											}
											SLEEP_MS(100);
										}
									}
								}
							}
						}
						respValue["params"]["scenes"].append(sceneRs);
					}
					else
					{
						LOGW("Scene %s create error", sceneId.c_str());
					}
				}
			}
			Database::getInstance()->SqliteEndTransaction();
			SceneManager::getInstance()->PrintScene();

			respValue["params"]["code"] = CODE_OK;

			room->SetDataConfig(respValue.toString());
			Database::getInstance()->RoomUpdateData(room);
		}
		else
		{
			respValue["params"]["code"] = CODE_ERROR;
			LOGW("Room id %s exist", roomId.c_str());
		}
	}
	else
	{
		respValue["params"]["code"] = CODE_FORMAT_ERROR;
	}
	return CODE_OK;
}

int Gateway::OnDeleteDeviceFromRoom(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGD("OnDeleteDeviceFromRoom");
	respValue["method"] = "delDevFromRoomRspComplete";
	if (reqValue.isMember("id") && reqValue["id"].isString() &&
		reqValue.isMember("devices") && reqValue["devices"].isArray())
	{
		Json::Value dataRsp;
		dataRsp["method"] = "delDevFromRoomRsp";
		dataRsp["params"] = reqValue;
		dataRsp["params"]["code"] = CODE_OK;
		CloudProtocol::Publish(topicRsp, dataRsp.toString());

		// int msgId = CloudProtocol::GetMsgId();
		// CloudProtocol::SetMsgId(++msgId);
		// topicRsp = "v1/devices/me/rpc/request/" + to_string(CloudProtocol::GetMsgId());

		map<string, bool> devicesStatusConfig;
		string roomId = reqValue["id"].asString();
		Json::Value devicesList = reqValue["devices"];
		Room *room = RoomManager::getInstance()->GetRoomFromId(roomId);
		if (room)
		{
			respValue["params"]["room"]["id"] = room->GetId();
			for (auto &dev : devicesList)
			{
				if (dev.isString())
				{
					Database::getInstance()->SqliteBenginTransaction();
					string deviceId = dev.asString();
					devicesStatusConfig[deviceId] = true;
					Device *device = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
					if (device)
					{
						if (device->RemoveFromRoom(room->GetAddr()) == CODE_OK)
						{
							respValue["params"]["room"]["success"].append(device->GetId());
							if (device->GetProtocol() == BLE_DEVICE) // Kiểm tra thêm thiết bị BLE vào phòng
							{
								// int indexType1 = (device->GetType() >> 16) & 0xFF;
								// int indexType2 = (device->GetType() >> 8) & 0xFF;
								// if ((indexType1 == 1) ||				  // Đèn
								// 	(indexType1 == 2 && indexType2 == 1)) // công tắc chuyển mạch
								// {
								// 	room->DelDevice(device, device->GetAddr());
								// 	Database::getInstance()->DeviceInRoomDel(room, device, device->GetAddr());
								// }
								// else if ((indexType1 == 2 && indexType2 == 2) ||
								// 		 (indexType1 == 2 && indexType2 == 4) ||
								// 		 (indexType1 == 2 && indexType2 == 6))
								// {
								// 	for (int i = 0; i < device->GetNumElement(); i++)
								// 	{
								// 		room->DelDevice(device, device->GetAddr() + i);
								// 		Database::getInstance()->DeviceInRoomDel(room, device, device->GetAddr() + i);
								// 	}
								// }
							}
						}
						else
						{
							respValue["params"]["room"]["failed"].append(device->GetId());
						}

						for (auto &groupInRoom : room->groupList)
						{
							Json::Value groupRs = Json::objectValue;
							vector<DeviceInGroup *> listDevInGroup = groupInRoom->deviceList;
							for (auto &devInGroup : listDevInGroup)
							{
								if (devInGroup->device->GetId() == device->GetId())
								{
									groupRs["id"] = groupInRoom->GetId();
									if (device->RemoveFromGroupInRoom(groupInRoom->GetAddr()) == CODE_OK)
									{
										groupRs["success"].append(device->GetId());
										if (device->GetProtocol() == BLE_DEVICE)
										{
											// int indexType1 = (device->GetType() >> 16) & 0xFF;
											// int indexType2 = (device->GetType() >> 8) & 0xFF;
											// if ((indexType1 == 1) ||
											// 	(indexType1 == 2 && indexType2 == 1))
											// {
											// 	groupInRoom->DelDevice(device, device->GetAddr());
											// 	Database::getInstance()->DeviceInGroupDel(groupInRoom, device, device->GetAddr());
											// }
											// else if ((indexType1 == 2 && indexType2 == 2) ||
											// 		 (indexType1 == 2 && indexType2 == 4) ||
											// 		 (indexType1 == 2 && indexType2 == 6))
											// {
											// 	for (int i = 0; i < device->GetNumElement(); i++)
											// 	{
											// 		groupInRoom->DelDevice(device, device->GetAddr() + i);
											// 		Database::getInstance()->DeviceInGroupDel(groupInRoom, device, device->GetAddr() + i);
											// 	}
											// }
										}
									}
									else
									{
										groupRs["failed"].append(device->GetId());
									}
									respValue["params"]["groups"].append(groupRs);
									SLEEP_MS(100);
									break;
								}
							}
						}

						for (auto &sceneInRoom : room->sceneList)
						{
							Json::Value sceneRs = Json::objectValue;
							vector<DeviceInScene *> listDevInScene = sceneInRoom->deviceList;
							for (auto &deviceInScene : listDevInScene)
							{
								if (deviceInScene->device->GetId() == device->GetId())
								{
									sceneRs["id"] = sceneInRoom->GetId();
									if (device->RemoveFromSceneInRoom(sceneInRoom->GetAddr(), deviceInScene->data) == CODE_OK)
									{
										sceneRs["success"].append(device->GetId());
										sceneInRoom->DelDevice(device);
										Database::getInstance()->DeviceInSceneDel(sceneInRoom, device);
									}
									else
									{
										sceneRs["failed"].append(device->GetId());
									}
									respValue["params"]["scenes"].append(sceneRs);
									SLEEP_MS(100);
								}
							}
						}
						SLEEP_MS(100);
					}
					else
					{
						LOGW("Device %s not found", deviceId.c_str());
						respValue["params"]["room"]["failed"].append(deviceId);
					}
					Database::getInstance()->SqliteEndTransaction();
				}
			}
			respValue["params"]["code"] = CODE_OK;
			respValue["params"]["id"] = room->GetId();
			room->SetDataConfig(respValue.toString());
			Database::getInstance()->RoomUpdateData(room);

			RoomManager::getInstance()->PrintRoom();
			GroupManager::getInstance()->PrintGroup();
			SceneManager::getInstance()->PrintScene();
		}
		else
		{
			LOGW("room %s not found", roomId.c_str());
			respValue["params"]["code"] = CODE_NOT_FOUND_ROOM;
		}
	}
	else
	{
		respValue["params"]["code"] = CODE_FORMAT_ERROR;
	}

	return CODE_OK;
}

int Gateway::OnDeleteRoom(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGD("OnDeleteRoom");
	respValue["method"] = "delRoomRsp";
	if (reqValue.isMember("id") && reqValue["id"].isString())
	{
		Json::Value successList = Json::arrayValue;
		Json::Value failedList = Json::arrayValue;
		string roomId = reqValue["id"].asString();
		map<string, bool> devicesStatusConfig;
		mtxDelRoom.lock();
		Room *room = RoomManager::getInstance()->GetRoomFromId(roomId);
		if (room)
		{
			Database::getInstance()->SqliteBenginTransaction();
			for (auto &devInRoom : room->deviceList)
			{
				devicesStatusConfig[devInRoom->device->GetId()] = true;
			}

			vector<DeviceInGroup *> devicesInRoom = room->deviceList;
			for (auto &devInRoom : devicesInRoom)
			{
				if (devInRoom->device->RemoveFromGroup(room->GetAddr(), devInRoom->epId) != CODE_OK)
				{
					devicesStatusConfig[devInRoom->device->GetId()] = false;
				}
				room->DelDevice(devInRoom->device, devInRoom->epId);
				Database::getInstance()->DeviceInRoomDel(room, devInRoom->device, devInRoom->epId);
				SLEEP_MS(100);
			}
			devicesInRoom.clear();

			vector<Group *> groupsInRoom = room->groupList;
			for (auto &groupInRoom : groupsInRoom)
			{
				vector<DeviceInGroup *> devicesInGroup = groupInRoom->deviceList;
				for (auto &deviceIngroup : devicesInGroup)
				{
					if (deviceIngroup->device->RemoveFromGroup(groupInRoom->GetAddr(), deviceIngroup->epId) != CODE_OK)
					{
						devicesStatusConfig[deviceIngroup->device->GetId()] = false;
					}
					groupInRoom->DelDevice(deviceIngroup->device, deviceIngroup->epId);
					Database::getInstance()->DeviceInGroupDel(groupInRoom, deviceIngroup->device, deviceIngroup->epId);
				}
				Database::getInstance()->GroupDel(groupInRoom);
				GroupManager::getInstance()->DelGroup(groupInRoom);
				devicesInGroup.clear();
			}
			groupsInRoom.clear();

			vector<Scene *> scenesInRoom = room->sceneList;
			for (auto &sceneInRoom : scenesInRoom)
			{
				vector<DeviceInScene *> devicesInScene = sceneInRoom->deviceList;
				for (auto &deviceInScene : devicesInScene)
				{
					if (deviceInScene->device->RemoveFromSceneInRoom(sceneInRoom->GetAddr(), deviceInScene->data) != CODE_OK)
					{
						devicesStatusConfig[deviceInScene->device->GetId()] = false;
					}
					sceneInRoom->DelDevice(deviceInScene->device);
					Database::getInstance()->DeviceInSceneDel(sceneInRoom, deviceInScene->device);
				}
				Database::getInstance()->SceneDel(sceneInRoom);
				SceneManager::getInstance()->DelScene(sceneInRoom);
				devicesInScene.clear();
			}
			scenesInRoom.clear();

			Database::getInstance()->SqliteEndTransaction();

			// TODO: Cân nhắc có xóa Rule gắn với phòng khi xóa phòng
			vector<Rule *> ruleInRoom = room->ruleList;
			for (auto &rule : ruleInRoom)
			{
				Database::getInstance()->RuleDel(rule);
				RuleManager::getInstance()->DelRule(rule);
			}
			ruleInRoom.clear();

			for (auto &[id, status] : devicesStatusConfig)
			{
				if (status)
				{
					successList.append(id);
				}
				else
					failedList.append(id);
			}

			respValue["params"]["code"] = CODE_OK;
			respValue["params"]["id"] = roomId;
			respValue["params"]["success"] = successList;
			respValue["params"]["failed"] = failedList;

			Database::getInstance()->RoomDel(room);
			RoomManager::getInstance()->DelRoom(room);

			RoomManager::getInstance()->PrintRoom();
			GroupManager::getInstance()->PrintGroup();
			SceneManager::getInstance()->PrintScene();
		}
		else
		{
			LOGW("Room %s not found", roomId.c_str());
			respValue["params"]["code"] = CODE_NOT_FOUND_ROOM;
		}
		mtxDelRoom.unlock();
	}
	else
	{
		respValue["params"]["code"] = CODE_FORMAT_ERROR;
	}
	return CODE_OK;
}

int Gateway::OnGetGroupIntoRoom(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	if (reqValue.isMember("rooms") && reqValue["rooms"].isArray())
	{
		Json::Value rooms = reqValue["rooms"];
		for (auto &room : rooms)
		{
			if (room.isString())
			{
				string roomId = room.asString();
				Room *temp_room = RoomManager::getInstance()->GetRoomFromId(roomId);
				if (temp_room)
				{
					Json::Value roomRsp;
					roomRsp["id"] = roomId;
					for (unsigned int i = 0; i < temp_room->groupList.size(); i++)
					{
						Json::Value groupRsp;
						groupRsp["id"] = temp_room->groupList[i]->GetId();
						groupRsp["name"] = temp_room->groupList[i]->GetName();
						roomRsp["groups"].append(groupRsp);
					}
					respValue["params"]["rooms"].append(roomRsp);
					respValue["params"]["code"] = CODE_OK;
				}
			}
		}
	}
	else
		respValue["params"]["code"] = CODE_FORMAT_ERROR;
	respValue["method"] = "getGroupIntoRoomRsp";
	return CODE_OK;
}

int Gateway::OnGetSceneIntoRoom(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	if (reqValue.isMember("rooms") && reqValue["rooms"].isArray())
	{
		Json::Value rooms = reqValue["rooms"];
		for (auto &room : rooms)
		{
			if (room.isString())
			{
				string roomId = room.asString();
				Room *temp_room = RoomManager::getInstance()->GetRoomFromId(roomId);
				if (temp_room)
				{
					Json::Value roomRsp;
					roomRsp["id"] = roomId;
					for (unsigned int i = 0; i < temp_room->sceneList.size(); i++)
					{
						Json::Value sceneRsp;
						sceneRsp["id"] = temp_room->sceneList[i]->GetId();
						sceneRsp["name"] = temp_room->sceneList[i]->GetName();
						roomRsp["scenes"].append(sceneRsp);
					}
					respValue["params"]["rooms"].append(roomRsp);
					respValue["params"]["code"] = CODE_OK;
				}
			}
		}
	}
	else
		respValue["params"]["code"] = CODE_FORMAT_ERROR;
	respValue["method"] = "getSceneIntoRoomRsp";
	return CODE_OK;
}

int Gateway::OnUpdateRoomName(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	if (reqValue.isMember("name") && reqValue["name"].isString() && reqValue.isMember("id") && reqValue["id"].isString())
	{
		string id = reqValue["id"].asString();
		string name = reqValue["name"].asString();
		Room *room = RoomManager::getInstance()->GetRoomFromId(id);
		if (room)
		{
			room->SetName(name);
			Database::getInstance()->RoomUpdateName(room);
		}
		Group *group = GroupManager::getInstance()->GetGroupFromId(id);
		if (group)
		{
			group->SetName(name);
		}
	}
	respValue["params"]["code"] = CODE_OK;
	respValue["params"]["id"] = id;
	respValue["method"] = "updateRoomNameRsp";
	return CODE_OK;
}

int Gateway::OnCheckRoom(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	if (reqValue.isMember("id") && reqValue["id"].isString())
	{
		string id = reqValue["id"].asString();
		Room *room = RoomManager::getInstance()->GetRoomFromId(id);
		if (room)
		{
			Json::Value dataRsp;
			if (dataRsp.parse(room->GetDataConfig()) && dataRsp.isObject())
			{
				respValue = dataRsp;
				return CODE_OK;
			}
			else
				LOGW("Data error: %s", dataRsp.toString().c_str());
		}
		else
			LOGW("Room %s not found", id.c_str());
	}
	else
		LOGW("Data error: %s", reqValue.toString().c_str());

	return CODE_ERROR;
}

int Gateway::OnRoomsSync(Json::Value &reqValue, Json::Value &respValue, string &topicRsp)
{
	LOGD("OnRoomsSync");

	std::unordered_set<std::string> syncRoomIds;

	if (reqValue.isArray())
	{
		for (const auto &value : reqValue)
		{
			if (value.isObject() && value.isMember("id") && value["id"].isString())
			{
				std::string roomId = value["id"].asString();
				syncRoomIds.insert(roomId);
			}
		}
	}

	std::vector<std::string> roomsToDelete;
	mtxDelRoom.lock();
	RoomManager::getInstance()->ForEach([&](Room *room)
										{
		if (room && syncRoomIds.find(room->GetId()) == syncRoomIds.end())
		{
			roomsToDelete.push_back(room->GetId());
		} });

	for (const auto &roomId : roomsToDelete)
	{
		Room *room = RoomManager::getInstance()->GetRoomFromId(roomId);
		if (!room)
			continue;

		for (auto &devInRoom : room->deviceList)
		{
			if (devInRoom && devInRoom->device)
			{
				devInRoom->device->RemoveFromRoom(room->GetAddr());
				Database::getInstance()->DeviceInRoomDel(room, devInRoom->device, devInRoom->epId);
			}
		}

		std::vector<Group *> listGroup = room->groupList;
		for (auto &groupInRoom : listGroup)
		{
			if (groupInRoom)
			{
				for (auto &devInGroup : groupInRoom->deviceList)
				{
					if (devInGroup && devInGroup->device)
					{
						devInGroup->device->RemoveFromGroup(groupInRoom->GetAddr(), devInGroup->epId);
						Database::getInstance()->DeviceInGroupDel(groupInRoom, devInGroup->device, devInGroup->epId);
					}
				}
				Database::getInstance()->GroupDel(groupInRoom->GetId());
				GroupManager::getInstance()->DelGroup(groupInRoom);
			}
		}

		std::vector<Scene *> listScene = room->sceneList;
		for (auto &sceneInRoom : listScene)
		{
			if (sceneInRoom)
			{
				for (auto &devInScene : sceneInRoom->deviceList)
				{
					if (devInScene && devInScene->device)
					{
						devInScene->device->RemoveFromScene(sceneInRoom->GetAddr(), devInScene->data);
						Database::getInstance()->DeviceInSceneDel(sceneInRoom, devInScene->device);
					}
				}
				Database::getInstance()->SceneDel(sceneInRoom->GetId());
				SceneManager::getInstance()->DelScene(sceneInRoom);
			}
		}

		Database::getInstance()->RoomDel(room->GetId());
		RoomManager::getInstance()->DelRoom(room);
	}
	mtxDelRoom.unlock();

	return CODE_NOT_RESPONSE;
}
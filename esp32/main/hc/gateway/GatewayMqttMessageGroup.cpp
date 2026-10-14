#include "Gateway.h"
#include "Log.h"
#include "Database.h"
#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <utility>

namespace
{
Device *FindGroupLinkDevice(const string &identifier)
{
	if (identifier.empty())
		return nullptr;

	Device *device = DeviceManager::getInstance()->GetDeviceFromId(identifier);
	if (device)
		return device;

	auto toLower = [](string value)
	{
		transform(value.begin(), value.end(), value.begin(),
				  [](unsigned char ch)
				  { return (char)tolower(ch); });
		return value;
	};

	const string normalizedId = toLower(identifier);
	DeviceManager::getInstance()->ForEach([&](Device *candidate)
										  {
		if (!device && candidate && toLower(candidate->GetCloudId()) == normalizedId)
			device = candidate; });
	if (device)
		return device;

	string normalizedMac = normalizedId;
	normalizedMac.erase(remove(normalizedMac.begin(), normalizedMac.end(), ':'),
						normalizedMac.end());
	return DeviceManager::getInstance()->GetDeviceFromMac(normalizedMac);
}

// "onoff" / "onoff0" -> 0, "onoff1" -> 1, ... Trả về -1 nếu sai định dạng.
int ParseOnoffKeyToIndex(const string &key)
{
	const string prefix = KEY_ATTRIBUTE_ONOFF;
	if (key.compare(0, prefix.size(), prefix) != 0)
		return -1;
	string index = key.substr(prefix.size());
	if (index.empty())
		return 0;
	if (!all_of(index.begin(), index.end(), [](unsigned char ch)
				{ return isdigit(ch) != 0; }))
		return -1;
	return atoi(index.c_str());
}

void AppendLinkResult(Json::Value &list, const string &deviceId, const Json::Value &keys)
{
	if (keys.empty())
		return;
	Json::Value item = Json::objectValue;
	item["id"] = deviceId;
	item["onoff"] = keys;
	list.append(item);
}

// Thêm/xoá các nút onoff trong lstBt vào group link, điền kết quả vào success/failed.
void ApplyGroupLinkDevices(Group *group, const Json::Value &lstBt, bool isAdd,
						   Json::Value &success, Json::Value &failed)
{
	for (auto &entry : lstBt)
	{
		if (!entry.isObject() || !entry.isMember("id") || !entry["id"].isString() ||
			!entry.isMember("onoff") || !entry["onoff"].isArray())
		{
			LOGW("invalid group link entry %s", entry.toString().c_str());
			continue;
		}

		string deviceId = entry["id"].asString();
		Device *device = FindGroupLinkDevice(deviceId);
		Json::Value successKeys = Json::arrayValue;
		Json::Value failedKeys = Json::arrayValue;

		for (auto &keyValue : entry["onoff"])
		{
			if (!keyValue.isString())
				continue;
			string key = keyValue.asString();
			int index = ParseOnoffKeyToIndex(key);

			if (!device || index < 0 || index >= device->GetNumElement())
			{
				LOGW("invalid group link device=%s key=%s found=%d numElement=%d",
					 deviceId.c_str(), key.c_str(), device ? 1 : 0,
					 device ? device->GetNumElement() : 0);
				failedKeys.append(key);
				continue;
			}

			uint16_t epId = device->GetAddr() + index;
			bool exists = group->GetPositionDevice(device, epId) >= 0;
			if (isAdd)
			{
				if (!exists)
				{
					if (device->AddToGroup(group->GetAddr(), epId) != CODE_OK)
					{
						LOGW("add group link device=%s key=%s group=%s failed",
							 deviceId.c_str(), key.c_str(), group->GetId().c_str());
						failedKeys.append(key);
						continue;
					}
					group->AddDevice(device, epId);
					Database::getInstance()->DeviceInGroupAdd(group, device, epId);
				}
			}
			else if (exists)
			{
				if (device->RemoveFromGroup(group->GetAddr(), epId) != CODE_OK)
				{
					LOGW("remove group link device=%s key=%s group=%s failed",
						 deviceId.c_str(), key.c_str(), group->GetId().c_str());
					failedKeys.append(key);
					continue;
				}
				group->DelDevice(device, epId);
				Database::getInstance()->DeviceInGroupDel(group, device, epId);
			}
			successKeys.append(key);
			SLEEP_MS(100);
		}

		AppendLinkResult(success, deviceId, successKeys);
		AppendLinkResult(failed, deviceId, failedKeys);
	}
}

void DeleteEmptyGroup(Group *group)
{
	string groupId = group->GetId();
	GroupManager::getInstance()->DelGroup(group);
	Database::getInstance()->GroupDel(groupId);
}
} // namespace

void Gateway::InitMqttMessageGroup()
{
	OnDeviceRpcCallbackRegister("controlGroup", bind(&Gateway::OnControlGroup, this, placeholders::_1, placeholders::_2));
	OnDeviceRpcCallbackRegister("createGroup", bind(&Gateway::OnCreateGroup, this, placeholders::_1, placeholders::_2));
	OnDeviceRpcCallbackRegister("addDevToGroup", bind(&Gateway::OnAddDeviceToGroup, this, placeholders::_1, placeholders::_2));
	OnDeviceRpcCallbackRegister("delDevFromGroup", bind(&Gateway::OnDeleteDeviceFromGroup, this, placeholders::_1, placeholders::_2));
	OnDeviceRpcCallbackRegister("delGroup", bind(&Gateway::OnDeleteGroup, this, placeholders::_1, placeholders::_2));
	OnDeviceRpcCallbackRegister("createGroupLink", bind(&Gateway::OnCreateGroupLink, this, placeholders::_1, placeholders::_2));
	OnDeviceRpcCallbackRegister("delGroupLink", bind(&Gateway::OnDeleteGroupLink, this, placeholders::_1, placeholders::_2));
	OnDeviceRpcCallbackRegister("addDevToGroupLink", bind(&Gateway::OnAddDeviceToGroupLink, this, placeholders::_1, placeholders::_2));
	OnDeviceRpcCallbackRegister("delDevFromGroupLink", bind(&Gateway::OnDeleteDeviceFromGroupLink, this, placeholders::_1, placeholders::_2));
}

int Gateway::OnControlGroup(Json::Value &reqValue, Json::Value &respValue)
{
	LOGD("OnControlGroup");
	if (reqValue.isMember("id") && reqValue["id"].isString() &&
		reqValue.isMember("data") && reqValue["data"].isObject())
	{
		string groupId = reqValue["id"].asString();
		Json::Value devData = reqValue["data"];
		Group *group = GroupManager::getInstance()->GetGroupFromId(groupId);
		if (group)
		{
			int rs = CODE_OK;
			if (group->deviceList.size() <= 20)
				rs = group->Do(devData, true);
			else
			{
				rs = group->Do(devData, false);
				Json::Value devicesData = Json::arrayValue;
				for (auto &devInGroup : group->deviceList)
				{
					if (devInGroup->device->IsOnline())
					{
						Json::Value deviceData;
						deviceData["id"] = devInGroup->device->GetId();
						// if ((devInGroup->device->GetType() / 1000) == 22 || (devInGroup->device->GetType() / 1000) == 24)
						// {
						// 	Json::Value dataSw = devData;
						// 	if (dataSw.isMember(KEY_ATTRIBUTE_ONOFF) && dataSw[KEY_ATTRIBUTE_ONOFF].isInt())
						// 	{
						// 		int value = dataSw[KEY_ATTRIBUTE_ONOFF].asInt();
						// 		dataSw.removeMember(KEY_ATTRIBUTE_ONOFF);
						// 		for (int i = 0; i < devInGroup->device->GetNumElement(); i++)
						// 		{
						// 			// key = onoffKey + (index ? to_string(index + 1) : "");
						// 			dataSw[KEY_ATTRIBUTE_BUTTON + ((i) ? to_string(i + 1) : "")] = value;
						// 		}
						// 		deviceData["data"] = dataSw;
						// 	}
						// 	else
						// 	{
						// 		deviceData["data"] = devData;
						// 	}
						// }
						// else
						// {
							deviceData["data"] = devData;
						// }

						devicesData.append(deviceData);
					}
				}
				Json::Value dataPush;
				dataPush["device"] = devicesData;
				PushDeviceUpdateCloud(dataPush);
			}
			respValue["data"]["code"] = rs;
		}
		else
		{
			LOGW("Group id %s not found", groupId.c_str());
			respValue["data"]["code"] = CODE_NOT_FOUND_GROUP;
		}
	}
	else
	{
		respValue["data"]["code"] = CODE_FORMAT_ERROR;
		LOGW("OnControlGroup %s format error", reqValue.toString().c_str());
	}
	respValue["cmd"] = "controlGroupRsp";
	return CODE_OK;
}

/*
{
  "cmd": "createGroup",
  "rqi": "abc123456",
  "data": {
    "id": "aa3549d4-5471-4d75-b0b2-b70fa5c10fb2",
    "name": "abc",
    "devices": [
      "b717f8d8-6f18-43c0-ae46-69c32998f653",
      "b717f8d8-6f18-43c0-ae46-69c32998f654"
    ],
    "roomId": "670bf986-0605-4709-850c-225475d8950b"
  }
}
*/
int Gateway::OnCreateGroup(Json::Value &reqValue, Json::Value &respValue)
{
	LOGI("OnCreateGroup");
	if (reqValue.isMember("id") && reqValue["id"].isString() && 
		reqValue.isMember("name") && reqValue["name"].isString() &&
		reqValue.isMember("devices") && reqValue["devices"].isArray())
	{
		string groupId = reqValue["id"].asString();
		string name = reqValue["name"].asString();
		Json::Value devices = reqValue["devices"];
		Json::Value successDevices(Json::arrayValue);
		Json::Value failedDevices(Json::arrayValue);
		Group *group = GroupManager::getInstance()->GetGroupFromId(groupId);
		if (!group)
		{
			group = new Group(groupId, GroupManager::getInstance()->GetNextGroupAddr(), name);
			if (group)
			{
				GroupManager::getInstance()->AddGroup(group);
				Database::getInstance()->GroupAdd(group);
			}
		}
		if (group)
		{
			for (auto &device : devices)
			{
				string deviceId = device.asString();
				Device *tempDevice = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
				if (tempDevice)
				{
					LOGI("Device found: %s", deviceId.c_str());
					uint16_t epId = tempDevice->GetAddr();					
					if (group->GetPositionDevice(tempDevice, epId) >= 0)
					{
						successDevices.append(deviceId);
						continue;
					}
					for (int i = 0; i < tempDevice->GetNumElement(); i++)
					{
						if (tempDevice->AddToGroup(group->GetAddr(), tempDevice->GetAddr() + i) == CODE_OK)
						{
							group->AddDevice(tempDevice, tempDevice->GetAddr() + i);
							Database::getInstance()->DeviceInGroupAdd(group, tempDevice, tempDevice->GetAddr() + i);
							successDevices.append(deviceId);
						}
						else
						{
							LOGE("Failed to add device %s to group %s", deviceId.c_str(), group->GetId().c_str());
							failedDevices.append(deviceId);
						}
					}					
				}
				else
				{
					LOGE("Device not found: %s", deviceId.c_str());
					failedDevices.append(deviceId);
				}			
			}
			respValue["data"]["code"] = CODE_OK;
			respValue["data"]["addr"] = group->GetAddr();
			respValue["data"]["id"] = groupId;
			respValue["data"]["success"] = successDevices;
			respValue["data"]["failed"] = failedDevices;
		}
	}
	respValue["cmd"] = "createGroupRsp";
	return CODE_OK;
}

int Gateway::OnAddDeviceToGroup(Json::Value &reqValue, Json::Value &respValue)
{
	if (reqValue.isMember("id") && reqValue["id"].isString() &&
		reqValue.isMember("devices") && reqValue["devices"].isArray())
	{
		Json::Value successDevices = Json::arrayValue;
		Json::Value failedDevices = Json::arrayValue;
		string groupId = reqValue["id"].asString();
		Json::Value devicesValue = reqValue["devices"];
		Group *group = GroupManager::getInstance()->GetGroupFromId(groupId);
		if (!group)
		{
			string groupName = "";
			if (reqValue.isMember("name") && reqValue["name"].isString())
			{
				groupName = reqValue["name"].asString();
			}
			group = new Group(groupId, GroupManager::getInstance()->GetNextGroupAddr(), groupName);
			if (group)
			{
				GroupManager::getInstance()->AddGroup(group);
			}
		}

		if (group)
		{
			for (auto &deviceValue : devicesValue)
			{
				if (deviceValue.isString())
				{
					string deviceId = deviceValue.asString();
					Device *device = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
					if (device)
					{
						for (int i = 0; i < device->GetNumElement(); i++)
						{
							if (device->AddToGroup(group->GetAddr(), device->GetAddr() + i) == CODE_OK)
							{
								group->AddDevice(device, device->GetAddr() + i);
								Database::getInstance()->DeviceInGroupAdd(group, device, device->GetAddr() + i);
								successDevices.append(deviceId);
							}
							else
							{
								device->RemoveFromGroup(group->GetAddr(), device->GetAddr() + i);
								failedDevices.append(deviceId);
							}
						}
					}
					else
					{
						LOGD("deviceId %s dose not exist", deviceId.c_str());
						failedDevices.append(deviceId);
					}
					SLEEP_MS(100);
				}
			}

			respValue["data"]["code"] = CODE_OK;
			respValue["data"]["success"] = successDevices;
			respValue["data"]["failed"] = failedDevices;
		}
		else
		{
			respValue["data"]["code"] = CODE_NOT_FOUND_GROUP;
		}
	}
	else
	{
		respValue["data"]["code"] = CODE_FORMAT_ERROR;
	}
	respValue["cmd"] = "addDevToGroupRsp";
	return CODE_OK;
}

int Gateway::OnDeleteDeviceFromGroup(Json::Value &reqValue, Json::Value &respValue)
{
	if (reqValue.isMember("id") && reqValue["id"].isString() &&
		reqValue.isMember("devices") && reqValue["devices"].isArray())
	{
		Json::Value successList = Json::arrayValue;
		Json::Value failedList = Json::arrayValue;
		string groupId = reqValue["id"].asString();
		Json::Value devicesValue = reqValue["devices"];
		Group *group = GroupManager::getInstance()->GetGroupFromId(groupId);
		if (group)
		{
			for (auto &deviceValue : devicesValue)
			{
				if (deviceValue.isString())
				{
					string deviceId = deviceValue.asString();
					Device *device = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
					if (device)
					{
						for (int i = 0; i < device->GetNumElement(); i++)
						{
							if (device->RemoveFromGroup(group->GetAddr(), device->GetAddr() + i) == CODE_OK)
							{
								group->DelDevice(device, device->GetAddr() + i);
								successList.append(deviceId);
							}
							else
							{
								failedList.append(deviceId);
							}
						}
					}
					else
					{
						LOGD("deviceId %s dose not exist", deviceId.c_str());
						failedList.append(deviceId);
					}
					SLEEP_MS(100);
				}
			}
			respValue["data"]["code"] = CODE_OK;
			respValue["data"]["success"] = successList;
			respValue["data"]["failed"] = failedList;
		}
		else
		{
			respValue["data"]["code"] = CODE_NOT_FOUND_GROUP;
		}
	}
	else
	{
		respValue["data"]["code"] = CODE_FORMAT_ERROR;
	}
	respValue["cmd"] = "delDevFromGroupRsp";
	return CODE_OK;
}

int Gateway::OnDeleteGroup(Json::Value &reqValue, Json::Value &respValue)
{
	if (reqValue.isMember("id") && reqValue["id"].isString())
	{
		Json::Value successList = Json::arrayValue;
		Json::Value failedList = Json::arrayValue;
		string groupId = reqValue["id"].asString();
		Group *group = GroupManager::getInstance()->GetGroupFromId(groupId);
		if (group)
		{
			vector<DeviceInGroup *> devicesInGroup = group->deviceList;
			for (auto &deviceInGroup : devicesInGroup)
			{
				if (group->DelDevice(deviceInGroup->device, deviceInGroup->epId) == CODE_OK)
				{
					successList.append(deviceInGroup->device->GetId());
				}
				else
				{
					LOGD("delete from group deviceId %s error", deviceInGroup->device->GetId().c_str());
					failedList.append(deviceInGroup->device->GetId());
				}

				SLEEP_MS(100);
			}
			GroupManager::getInstance()->DelGroup(group);
			Database::getInstance()->GroupDel(groupId);
			respValue["data"]["code"] = CODE_OK;
			respValue["data"]["success"] = successList;
			respValue["data"]["failed"] = failedList;
		}
		else
		{
			respValue["data"]["code"] = CODE_NOT_FOUND_GROUP;
		}
	}
	else
	{
		respValue["data"]["code"] = CODE_FORMAT_ERROR;
	}
	respValue["cmd"] = "delGroupRsp";
	return CODE_OK;
}

/**
 {
  "cmd": "createGroupLink",
  "rqi": "request-001",
  "data": {
    "id": "group-001",
    "lstBt": [
      {
        "id": "device-001",
        "onoff": ["onoff0", "onoff1"]
      }
    ]
  }
}

{
  "cmd": "createGroupLinkRsp",
  "data": {
    "code": 0,
    "success": [
      {
        "id": "device-001",
        "onoff": ["onoff0", "onoff1"]
      }
    ],
    "failed": []
  },
  "rqi": "request-001"
}
 */
int Gateway::OnCreateGroupLink(Json::Value &reqValue, Json::Value &respValue)
{
	LOGI("OnCreateGroupLink");
	if (reqValue.isMember("id") && reqValue["id"].isString() &&
		reqValue.isMember("lstBt") && reqValue["lstBt"].isArray())
	{
		string groupId = reqValue["id"].asString();
		Json::Value success = Json::arrayValue;
		Json::Value failed = Json::arrayValue;
		bool created = false;

		Group *group = GroupManager::getInstance()->GetGroupFromId(groupId);
		if (!group)
		{
			string name = (reqValue.isMember("name") && reqValue["name"].isString()) ? reqValue["name"].asString() : groupId;
			group = new Group(groupId, GroupManager::getInstance()->GetNextGroupAddr(), name);
			GroupManager::getInstance()->AddGroup(group);
			if (Database::getInstance()->GroupAdd(group) != CODE_OK)
			{
				LOGE("save group link %s failed", groupId.c_str());
				GroupManager::getInstance()->DelGroup(group);
				respValue["data"]["code"] = CODE_ERROR;
				respValue["cmd"] = "createGroupLinkRsp";
				return CODE_OK;
			}
			created = true;
		}

		ApplyGroupLinkDevices(group, reqValue["lstBt"], true, success, failed);
		if (created && group->deviceList.empty())
			DeleteEmptyGroup(group);

		respValue["data"]["code"] = CODE_OK;
		respValue["data"]["success"] = success;
		respValue["data"]["failed"] = failed;
	}
	else
	{
		respValue["data"]["code"] = CODE_FORMAT_ERROR;
		LOGW("OnCreateGroupLink %s format error", reqValue.toString().c_str());
	}
	respValue["cmd"] = "createGroupLinkRsp";
	return CODE_OK;
}

/**
 {
  "cmd": "addDevToGroupLink",
  "rqi": "request-002",
  "data": {
    "id": "group-001",
    "lstBt": [
      {
        "id": "device-001",
        "onoff": ["onoff0", "onoff1"]
      }
    ]
  }
}
 {
  "cmd": "addDevToGroupLinkRsp",
  "data": {
    "code": 0,
    "success": [
      {
        "id": "device-001",
        "onoff": ["onoff0", "onoff1"]
      }
    ],
    "failed": []
  },
  "rqi": "request-002"
 }
 */
int Gateway::OnAddDeviceToGroupLink(Json::Value &reqValue, Json::Value &respValue)
{
	LOGI("OnAddDeviceToGroupLink");
	if (reqValue.isMember("id") && reqValue["id"].isString() &&
		reqValue.isMember("lstBt") && reqValue["lstBt"].isArray())
	{
		Group *group = GroupManager::getInstance()->GetGroupFromId(reqValue["id"].asString());
		if (group)
		{
			Json::Value success = Json::arrayValue;
			Json::Value failed = Json::arrayValue;
			ApplyGroupLinkDevices(group, reqValue["lstBt"], true, success, failed);
			respValue["data"]["code"] = CODE_OK;
			respValue["data"]["success"] = success;
			respValue["data"]["failed"] = failed;
		}
		else
		{
			respValue["data"]["code"] = CODE_NOT_FOUND_GROUP;
		}
	}
	else
	{
		respValue["data"]["code"] = CODE_FORMAT_ERROR;
		LOGW("OnAddDeviceToGroupLink %s format error", reqValue.toString().c_str());
	}
	respValue["cmd"] = "addDevToGroupLinkRsp";
	return CODE_OK;
}

/**
 {
  "cmd": "deleteGroupLink",
  "rqi": "request-003",
  "data": {
    "id": "group-001"
  }
}

{
  "cmd": "deleteGroupLinkRsp",
  "data": {
    "code": 0
  },
  "rqi": "request-003"
 }
 */
int Gateway::OnDeleteGroupLink(Json::Value &reqValue, Json::Value &respValue)
{
	LOGI("OnDeleteGroupLink");
	if (reqValue.isMember("id") && reqValue["id"].isString())
	{
		Group *group = GroupManager::getInstance()->GetGroupFromId(reqValue["id"].asString());
		if (group)
		{
			// Sao chép danh sách vì RemoveLinkDevice sửa deviceList
			vector<DeviceInGroup *> members = group->deviceList;
			bool allRemoved = true;
			for (auto *member : members)
			{
				uint16_t epId = member->epId;
				Device *device = member->device;
				if (device->RemoveFromGroup(group->GetAddr(), epId) == CODE_OK)
				{
					group->DelDevice(device, epId);
					Database::getInstance()->DeviceInGroupDel(group, device, epId);
				}
				else
				{
					LOGW("remove group link device=%s epId=0x%04X failed", device->GetId().c_str(), epId);
					allRemoved = false;
				}
				SLEEP_MS(100);
			}

			if (allRemoved)
			{
				DeleteEmptyGroup(group);
				respValue["data"]["code"] = CODE_OK;
			}
			else
			{
				respValue["data"]["code"] = CODE_ERROR;
			}
		}
		else
		{
			respValue["data"]["code"] = CODE_NOT_FOUND_GROUP;
		}
	}
	else
	{
		respValue["data"]["code"] = CODE_FORMAT_ERROR;
		LOGW("OnDeleteGroupLink %s format error", reqValue.toString().c_str());
	}
	respValue["cmd"] = "deleteGroupLinkRsp";
	return CODE_OK;
}

/**
 {
  "cmd": "deleteDeviceFromGroupLink",
  "rqi": "request-004",
  "data": {
    "id": "group-001",
    "lstBt": [
      {
        "id": "device-001",
        "onoff": ["onoff0", "onoff1"]
      }
    ]
  }
}

{
  "cmd": "delDevFromGroupLinkRsp",
  "data": {
    "code": 0,
    "success": [
      {
        "id": "device-001",
        "onoff": ["onoff0", "onoff1"]
      }
    ],
    "failed": []
  },
  "rqi": "request-004"
}
 */
int Gateway::OnDeleteDeviceFromGroupLink(Json::Value &reqValue, Json::Value &respValue)
{
	LOGI("OnDeleteDeviceFromGroupLink");
	if (reqValue.isMember("id") && reqValue["id"].isString() &&
		reqValue.isMember("lstBt") && reqValue["lstBt"].isArray())
	{
		Group *group = GroupManager::getInstance()->GetGroupFromId(reqValue["id"].asString());
		if (group)
		{
			Json::Value success = Json::arrayValue;
			Json::Value failed = Json::arrayValue;
			ApplyGroupLinkDevices(group, reqValue["lstBt"], false, success, failed);
			if (group->deviceList.empty())
				DeleteEmptyGroup(group);
			respValue["data"]["code"] = CODE_OK;
			respValue["data"]["success"] = success;
			respValue["data"]["failed"] = failed;
		}
		else
		{
			respValue["data"]["code"] = CODE_NOT_FOUND_GROUP;
		}
	}
	else
	{
		respValue["data"]["code"] = CODE_FORMAT_ERROR;
		LOGW("OnDeleteDeviceFromGroupLink %s format error", reqValue.toString().c_str());
	}
	respValue["cmd"] = "delDevFromGroupLinkRsp";
	return CODE_OK;
}
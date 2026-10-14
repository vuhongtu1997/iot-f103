#include "Group.h"
#include <thread>
#include "Log.h"
#include <cstdlib>

#ifdef CONFIG_ENABLE_BLE
#include "BleProtocol.h"
#endif

using namespace std;

namespace
{
bool TryGetGroupDpInt(const Json::Value &value, int &out)
{
	if (value.isInt())
	{
		out = value.asInt();
		return true;
	}
	if (value.isUInt())
	{
		out = static_cast<int>(value.asUInt());
		return true;
	}
	if (value.isBool())
	{
		out = value.asBool() ? 1 : 0;
		return true;
	}
	if (value.isString())
	{
		out = atoi(value.asString().c_str());
		return true;
	}
	if (value.isObject() && value.isMember("value"))
	{
		return TryGetGroupDpInt(value["value"], out);
	}
	return false;
}

int ClampGroupDp(int value, int minValue, int maxValue)
{
	if (value < minValue)
		return minValue;
	if (value > maxValue)
		return maxValue;
	return value;
}

uint16_t GroupDimToRaw(int value, int maxValue)
{
	value = ClampGroupDp(value, 0, maxValue);
	return (uint16_t)((value * 65535 + (maxValue / 2)) / maxValue);
}

uint16_t GroupCctToRaw(int value)
{
	value = ClampGroupDp(value, 0, 1000);
	return (uint16_t)(800 + (value * 19200 + 500) / 1000);
}

std::string GetGroupDpString(const Json::Value &value)
{
	if (value.isString())
		return value.asString();
	if (value.isObject())
	{
		if (value.isMember("valueString") && value["valueString"].isString())
			return value["valueString"].asString();
		if (value.isMember("value") && value["value"].isString())
			return value["value"].asString();
	}
	return "";
}

bool TryGetGroupModeRgb(const Json::Value &value, int &out)
{
	std::string raw = GetGroupDpString(value);
	const std::string prefix = "scene_";
	if (raw.compare(0, prefix.size(), prefix) == 0)
	{
		out = atoi(raw.substr(prefix.size()).c_str());
		return true;
	}

	return TryGetGroupDpInt(value, out);
}

bool ParseLegacyHsl(const std::string &raw, int &h, int &s, int &l)
{
	if (raw.size() < 12)
		return false;

	char *end = nullptr;
	h = static_cast<int>(strtol(raw.substr(0, 4).c_str(), &end, 16));
	if (!end || *end != '\0')
		return false;
	s = static_cast<int>(strtol(raw.substr(4, 4).c_str(), &end, 16));
	if (!end || *end != '\0')
		return false;
	l = static_cast<int>(strtol(raw.substr(8, 4).c_str(), &end, 16));
	return end && *end == '\0';
}

#ifdef CONFIG_ENABLE_BLE
uint16_t ToMeshGroupControlAddr(uint16_t groupAddr)
{
	if ((groupAddr & 0x00F0) == 0x00C0)
		return groupAddr;
	return static_cast<uint16_t>(groupAddr + BLE_GROUP_OFFSET);
}
#endif
} // namespace

DeviceInGroup::DeviceInGroup(Device *device, uint16_t epId)
{
	this->device = device;
	this->epId = epId;
}

Group::Group(string id, uint16_t addr, string name) : Object(id, addr, name)
{
}

Group::~Group()
{
	mtx.lock();
	deviceList.clear();
	mtx.unlock();
}

int Group::GetPositionDevice(Device *device, uint16_t epId)
{
	string deviceId = device->GetId();
	mtx.lock();
	for (uint32_t i = 0; i < deviceList.size(); i++)
	{
		if ((deviceId == deviceList[i]->device->GetId()) && (deviceList[i]->epId == epId))
		{
			mtx.unlock();
			return i;
		}
	}
	mtx.unlock();
	return -1;
}

int Group::AddDevice(Device *device, uint16_t epId)
{
	if (device)
	{
		DeviceInGroup *deviceInGroup = new DeviceInGroup(device, epId);
		if (deviceInGroup)
		{
			if (GetPositionDevice(device, epId) == CODE_ERROR)
			{
				mtx.lock();
				deviceList.push_back(deviceInGroup);
				mtx.unlock();
			}
			else
				LOGW("Device %s with epId %d is exist in group", device->GetId().c_str(), epId);
			return CODE_OK;
		}
	}
	return CODE_ERROR;
}

int Group::DelDevice(Device *device, uint16_t epId)
{
	if (device)
	{
		int deviceIndex = GetPositionDevice(device, epId);
		if (deviceIndex >= 0)
		{
			mtx.lock();
			delete deviceList[deviceIndex];
			deviceList.erase(deviceList.begin() + deviceIndex);
			mtx.unlock();
		}
		return CODE_OK;
	}
	return CODE_ERROR;
}

uint16_t Group::GetElementFromDp(Device *device, int dpId)
{
	if (!device || dpId <= 0)
		return 0;
	return device->GetAddr() + ((uint16_t)(dpId - 1) << 8);
}

int Group::GetDpFromElement(Device *device, uint16_t epId)
{
	if (!device || epId < device->GetAddr())
		return 0;
	return (int)(((epId - device->GetAddr()) >> 8) + 1);
}

bool Group::HasLinkDevice(Device *device, int dpId)
{
	uint16_t epId = GetElementFromDp(device, dpId);
	return epId != 0 && GetPositionDevice(device, epId) >= 0;
}

int Group::AddLinkDevice(Device *device, int dpId)
{
	uint16_t epId = GetElementFromDp(device, dpId);
	if (!device || epId == 0 || dpId > device->GetNumElement())
		return CODE_ERROR;
	if (GetPositionDevice(device, epId) >= 0)
		return CODE_OK;

#ifdef CONFIG_ENABLE_BLE
	uint16_t element = (uint16_t)((epId >> 8) | (epId << 8));
	uint16_t groupAddr = (uint16_t)((addr >> 8) | (addr << 8));
	if (BleProtocol::getInstance()->AddDev2Group(device->GetAddr(), element,
												 groupAddr) != CODE_OK)
	{
		string deviceKey = device->GetDeviceKey();
		LOGW("Add group did not receive status from %s; reloading device key",
			 device->GetId().c_str());
		// if (BleProtocol::getInstance()->UpdateDeviceKeyDev(
		// 		device->GetAddr(), device->GetNumElement(), deviceKey) != CODE_OK)
		// {
		// 	LOGE("Cannot reload device key for %s", device->GetId().c_str());
		// 	return CODE_ERROR;
		// }
		if (BleProtocol::getInstance()->AddDev2Group(device->GetAddr(), element,
													 groupAddr) != CODE_OK)
			return CODE_ERROR;
	}
#endif
	return AddDevice(device, epId);
}

int Group::RemoveLinkDevice(Device *device, int dpId)
{
	uint16_t epId = GetElementFromDp(device, dpId);
	if (!device || epId == 0 || dpId > device->GetNumElement())
		return CODE_ERROR;
	if (GetPositionDevice(device, epId) < 0)
		return CODE_OK;

#ifdef CONFIG_ENABLE_BLE
	uint16_t element = (uint16_t)((epId >> 8) | (epId << 8));
	uint16_t groupAddr = (uint16_t)((addr >> 8) | (addr << 8));
	if (BleProtocol::getInstance()->DelDev2Group(device->GetAddr(), element,
												 groupAddr) != CODE_OK)
	{
		string deviceKey = device->GetDeviceKey();
		LOGW("Remove group did not receive status from %s; reloading device key",
			 device->GetId().c_str());
		// if (BleProtocol::getInstance()->UpdateDeviceKeyDev(
		// 		device->GetAddr(), device->GetNumElement(), deviceKey) != CODE_OK)
		// {
		// 	LOGE("Cannot reload device key for %s", device->GetId().c_str());
		// 	return CODE_ERROR;
		// }
		if (BleProtocol::getInstance()->DelDev2Group(device->GetAddr(), element,
													 groupAddr) != CODE_OK)
			return CODE_ERROR;
	}
#endif
	return DelDevice(device, epId);
}

Json::Value Group::BuildLinkDevicesJson()
{
	Json::Value devices = Json::arrayValue;
	std::lock_guard<std::mutex> lock(mtx);
	for (auto *member : deviceList)
	{
		if (!member || !member->device)
			continue;

		Json::Value item = Json::objectValue;
		item["deviceID"] = member->device->GetCloudId();
		item["dpID"] = GetDpFromElement(member->device, member->epId);
		item["state"] = 0;
		devices.append(item);
	}
	return devices;
}

int Group::RemoveDevice(Device *device)
{
	mtx.lock();
	bool removed = false;

	for (auto it = deviceList.begin(); it != deviceList.end();)
	{
		if ((*it)->device == device)
		{
			delete *it;
			it = deviceList.erase(it);
			removed = true;
		}
		else
		{
			++it;
		}
	}
	mtx.unlock();
	return removed ? CODE_OK : CODE_ERROR;
}

int Group::Do(Json::Value &dataValue, bool ack)
{
	if (dataValue.isObject())
	{
		if (dataValue.isMember(KEY_ATTRIBUTE_ONOFF) && dataValue[KEY_ATTRIBUTE_ONOFF].isInt())
		{
			int value = dataValue[KEY_ATTRIBUTE_ONOFF].asInt();
			BleProtocol::getInstance()->SetOnOffLight(ToMeshGroupControlAddr(addr), value, TRANSITION_DEFAULT, ack);
		}
		if (dataValue.isMember(KEY_ATTRIBUTE_DIM) && dataValue[KEY_ATTRIBUTE_DIM].isInt() &&
			!dataValue.isMember(KEY_ATTRIBUTE_CCT))
		{
			int value = dataValue[KEY_ATTRIBUTE_DIM].asInt();
			uint16_t dim = (value * 65535) / 100;
			BleProtocol::getInstance()->SetGroupDimmingLight(ToMeshGroupControlAddr(addr), dim);
		}
		if (dataValue.isMember(KEY_ATTRIBUTE_CCT) && dataValue[KEY_ATTRIBUTE_CCT].isInt() &&
			!dataValue.isMember(KEY_ATTRIBUTE_DIM))
		{
			int value = dataValue[KEY_ATTRIBUTE_CCT].asInt();
			uint16_t cct = (value * 192) + 800;
			BleProtocol::getInstance()->SetGroupCctLight(ToMeshGroupControlAddr(addr), cct);
		}
		if (dataValue.isMember(KEY_ATTRIBUTE_CCT) && dataValue[KEY_ATTRIBUTE_CCT].isInt() &&
			dataValue.isMember(KEY_ATTRIBUTE_DIM) && dataValue[KEY_ATTRIBUTE_DIM].isInt())
		{
			int dim = dataValue[KEY_ATTRIBUTE_DIM].asInt();
			int cct = dataValue[KEY_ATTRIBUTE_CCT].asInt();
			uint16_t tempCct = (cct * 192) + 800;
			uint16_t tempDim = (dim * 65535) / 100;
			BleProtocol::getInstance()->SetGroupCctDimLight(ToMeshGroupControlAddr(addr), tempDim, tempCct);
		}
		if (dataValue.isMember(KEY_ATTRIBUTE_HUE) && dataValue[KEY_ATTRIBUTE_HUE].isInt() &&
			dataValue.isMember(KEY_ATTRIBUTE_SATURATION) && dataValue[KEY_ATTRIBUTE_SATURATION].isInt() &&
			dataValue.isMember(KEY_ATTRIBUTE_LUMINANCE) && dataValue[KEY_ATTRIBUTE_LUMINANCE].isInt())
		{
			int h = dataValue[KEY_ATTRIBUTE_HUE].asInt();
			int s = dataValue[KEY_ATTRIBUTE_SATURATION].asInt();
			int l = dataValue[KEY_ATTRIBUTE_LUMINANCE].asInt();
			BleProtocol::getInstance()->SetHSLLight(ToMeshGroupControlAddr(addr), h, s, l, TRANSITION_DEFAULT, ack);
		}
		if (dataValue.isMember(KEY_ATTRIBUTE_MODE_RGB) && dataValue[KEY_ATTRIBUTE_MODE_RGB].isInt())
		{
			int value = dataValue[KEY_ATTRIBUTE_MODE_RGB].asInt();
			BleProtocol::getInstance()->SetGroupRgbMode(ToMeshGroupControlAddr(addr), value);
		}
	}
	return CODE_OK;
}

bool Group::IsDevIn(Device *device)
{
	if (device)
	{
#ifdef CONFIG_ENABLE_BLE
		return (GetPositionDevice(device, device->GetAddr()) > -1);
#endif
	}
	return false;
}

void Group::AddDataToListRsp(Device *device, Json::Value &dataValue)
{
	if (!device)
		return;

	std::string deviceId = device->GetId();
	listRspMtx.lock();
	auto it = listRsp.find(deviceId);
	if (it == listRsp.end())
	{
		listRsp[deviceId] = dataValue;
	}
	else
	{
		Json::Value &existingValue = it->second;
		if (existingValue.isArray())
		{
			bool alreadyExists = false;
			for (const auto &val : dataValue)
			{
				for (const auto &item : existingValue)
				{
					if (val == item)
					{
						alreadyExists = true;
						break;
					}
				}
				if (!alreadyExists)
				{
					existingValue.append(val);
				}
			}
		}
	}
	listRspMtx.unlock();
}

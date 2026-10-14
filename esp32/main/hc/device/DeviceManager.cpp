#include "DeviceManager.h"
#include "BleDeviceConfigs.h"
#include "Log.h"
#include "Gateway.h"
#include "Database.h"
#include "Util.h"
#include <algorithm>
#include <cctype>

#ifdef CONFIG_ENABLE_BLE
// #include "DeviceBleLightOnoffCctDim.h"
// #include "DeviceBleLightOnoffCctDimHslModeRGB.h"
#include "DeviceBleLightWhite.h"
#include "DeviceBleLightRgb.h"
#include "DeviceBleDoorSensor.h"
#include "DeviceBleCurtain.h"
#include "DeviceBleMultiOnOff.h"
#include "DeviceBleIR.h"
#include "DeviceBlePresenceSensor.h"
#include "DeviceBleSmokeSensor.h"
#include "DeviceBleSwitchTouchRgb.h"
// #include "DeviceBleRadaSensorAc.h"
// #include "DeviceBleSeftPowerRemote.h"
#endif

DeviceManager *DeviceManager::getInstance()
{
	static DeviceManager *deviceManager = NULL;
	if (!deviceManager)
	{
		deviceManager = new DeviceManager();
	}
	return deviceManager;
}

DeviceManager::DeviceManager()
{
}

static string NormalizePid(string pid)
{
	pid.erase(remove_if(pid.begin(), pid.end(), [](unsigned char ch)
						{ return isspace(ch); }),
			  pid.end());
	return pid;
}

static inline unsigned char ToLowerUnsigned(unsigned char ch)
{
	return static_cast<unsigned char>(std::tolower(ch));
}

static void NormalizeLowerInPlace(std::string &value)
{
	std::transform(value.begin(), value.end(), value.begin(),
				   [](unsigned char ch)
				   { return static_cast<char>(ToLowerUnsigned(ch)); });
}

static std::mutex &DeviceListMutex()
{
	static std::mutex mutex;
	return mutex;
}

static string ResolveDeviceModelFromPid(string pid)
{
	pid = NormalizePid(pid);
	if (pid.empty())
		return "";

	auto it = getBleDeviceConfigs().find(pid);
	if (it != getBleDeviceConfigs().end())
	{
		return it->second.modelName;
	}

	if (pid == HG_BLE_MECHANICAL_SWITCH_1)
		return "HG_BLE_SWITCH_1";
	if (pid == HG_BLE_MECHANICAL_SWITCH_2)
		return "HG_BLE_SWITCH_2";
	if (pid == HG_BLE_MECHANICAL_SWITCH_3)
		return "HG_BLE_SWITCH_3";
	if (pid == HG_BLE_MECHANICAL_SWITCH_4)
		return "HG_BLE_SWITCH_4";
	if (pid == HG_BLE_IR || pid == HG_BLE_IR_TV || pid == HG_BLE_IR_FAN ||
		pid == HG_BLE_IR_AC || pid == HG_BLE_IR_REMOTE ||
		pid == HG_BLE_IR_V2 || pid == HG_BLE_IR_TV_V2 ||
		pid == HG_BLE_IR_FAN_V2 || pid == HG_BLE_IR_AC_V2 ||
		pid == HG_BLE_IR_REMOTE_V2 || pid == HG_BLE_IR_BLE ||
		pid == HG_BLE_IR_TV_V2_LAO || pid == HG_BLE_IR_FAN_V2_LAO ||
		pid == HG_BLE_IR_AC_V2_LAO || pid == HG_BLE_IR_REMOTE_V2_LAO)
		return "HG_BLE_IR";
	if (pid == HG_BLE_CURTAIN_IH35 || pid == HG_BLE_CURTAIN_IH68 ||
		pid == "BLEHGAA0105" ||
		pid == HG_BLE_CURTAIN_1_LAYER_MECHANICAL ||
		pid == HG_BLE_CURTAIN_NORMAL_NEW ||
		pid == HG_BLE_CURTAIN_MODULE_NEW ||
		pid == HG_BLE_VIP_CURTAIN_1_LAYER_MECHANICAL ||
		pid == HG_BLE_LUXURY_CURTAIN_1_LAYER_MECHANICAL)
		return "HG_BLE_CURTAIN_NEW";

	return "";
}

static string ResolveDeviceTypeForCreate(string &type, Json::Value *dataJson)
{
	string resolvedType = type;
	string resolvedFromType = ResolveDeviceModelFromPid(resolvedType);
	if (!resolvedFromType.empty())
	{
		return resolvedFromType;
	}

	if (dataJson && dataJson->isObject())
	{
		string pid = "";
		if (dataJson->isMember("pid") && (*dataJson)["pid"].isString())
		{
			pid = (*dataJson)["pid"].asString();
		}
		else if (dataJson->isMember("devices") && (*dataJson)["devices"].isString())
		{
			auto tokens = Util::splitString((*dataJson)["devices"].asString(), '|');
			if (tokens.size() >= 2)
				pid = tokens[1];
		}

		string resolvedFromPid = ResolveDeviceModelFromPid(pid);
		if (!resolvedFromPid.empty())
		{
			return resolvedFromPid;
		}
	}

	return resolvedType;
}

static Device *CreateDeviceByModel(string &id, string &mac, string &type, string &name,
								   uint16_t addr, uint16_t version,
								   Json::Value *dataJson)
{
	if (type == "HG_BLE_LIGHT_WHITE" ||
		type == "HG_BLE_SWITCH_DIMMING")
	{
		return new DeviceBleLightWhite(id, name, mac, type, addr, version, dataJson);
	}
	if (type == "HG_BLE_LIGHT_RGB")
	{
		return new DeviceBleLightRgb(id, name, mac, type, addr, version, dataJson);
	}
	if (type == "HG_BLE_SWITCH_1")
	{
		return new DeviceBleSwitchTouchRgb(id, name, mac, type, addr, version, dataJson, 1);
	}
	if (type == "HG_BLE_SWITCH_2")
	{
		return new DeviceBleSwitchTouchRgb(id, name, mac, type, addr, version, dataJson, 2);
	}
	if (type == "HG_BLE_SWITCH_3")
	{
		return new DeviceBleSwitchTouchRgb(id, name, mac, type, addr, version, dataJson, 3);
	}
	if (type == "HG_BLE_SWITCH_4")
	{
		return new DeviceBleSwitchTouchRgb(id, name, mac, type, addr, version, dataJson, 4);
	}
	if (type == "HG_BLE_SWITCH_SCENE")
	{
		return new DeviceBleSwitchTouchRgb(id, name, mac, type, addr, version, dataJson, 4);
	}
	if (type == "HG_BLE_STAIR")
	{
		return new DeviceBleMultiOnOff(id, name, mac, type, addr, version, dataJson, 3);
	}
	if (type == "HG_BLE_CURTAIN_DOUBLE")
	{
		return new DeviceBleMultiOnOff(id, name, mac, type, addr, version, dataJson, 2);
	}
	if (type == "HG_BLE_CURTAIN_OLD")
	{
		return new DeviceBleCurtain(id, name, mac, type, addr, version, dataJson);
	}
	if (type == "HG_BLE_ROLLING_DOOR" ||
		type == "HG_BLE_ROLLING_DOOR_SWITCH" ||
		type == "HG_BLE_ROLLING_DOOR_MODULE")
	{
		return new DeviceBleMultiOnOff(id, name, mac, type, addr, version, dataJson, 1);
	}
	if (type == "RD_BLE_SENSOR_SMOKE")
	{
		return new DeviceBleSmokeSensor(id, name, mac, type, addr, version, dataJson);
	}
	if (type == "RD_BLE_SENSOR_DOOR")
	{
		return new DeviceBleDoorSensor(id, name, mac, type, addr, version, dataJson);
	}
	if (type == "HG_BLE_SENSOR_PRESENCE" || type == "HG_BLE_SENSOR_PRESENCE_1")
	{
		return new DeviceBlePresenceSensor(id, name, mac, type, addr, version, dataJson);
	}
	if (type == "HG_BLE_IR")
	{
		return new DeviceBleIR(id, name, mac, type, addr, version, dataJson);
	}
	if (type == "HG_BLE_CURTAIN_NEW" ||
		type == "HG_BLE_ROLLING_DOOR_NEW")
	{
		return new DeviceBleCurtain(id, name, mac, type, addr, version, dataJson);
	}
	if (type == "HG_BLE_GENERIC")
	{
		// Report-only device: provisioning and persistence are supported, while
		// actuator commands remain disabled because this model has no actuator.
		return new DeviceBle(id, name, mac, type, addr, version, dataJson);
	}
	return nullptr;
}

void DeviceManager::ForEach(function<void(Device *)> func)
{
	DeviceListMutex().lock();
	for (auto &[id, device] : deviceList)
	{
		func(device);
	}
	DeviceListMutex().unlock();
}

Device *DeviceManager::AddDevice(string &mac, string &type, string &name, uint16_t addr, uint16_t version, Json::Value *dataJson)
{
	NormalizeLowerInPlace(mac);
	type = ResolveDeviceTypeForCreate(type, dataJson);
	const char *typeName = type.c_str();
	if (typeName)
	{
		Device *device = nullptr;
		device = DeviceManager::getInstance()->GetDeviceFromId(mac);
		if (device)
		{
#ifdef CONFIG_ENABLE_BLE
			BleProtocol::getInstance()->ResetDev(device->GetAddr());
			// TODO: remove device from room, group,...
			DelDevice(device);
#endif
		}
		device = CreateDeviceByModel(mac, mac, type, name, addr, version, dataJson);
		if (device)
		{
			DeviceListMutex().lock();
			deviceList[device->GetId()] = device;
			DeviceListMutex().unlock();
		}
		else
		{
			LOGW("Device type %s not support for mac %s addr 0x%04X",
				 type.c_str(), mac.c_str(), addr);
		}
		return device;
	}
	else
	{
		LOGW("Device type 0x%08x not support!!!", type);
	}
	return NULL;
}

Device *DeviceManager::AddDeviceNoReset(string &mac, string &type, string &name,
										uint16_t addr, uint16_t version,
										Json::Value *dataJson)
{
	NormalizeLowerInPlace(mac);
	type = ResolveDeviceTypeForCreate(type, dataJson);

	Device *device = CreateDeviceByModel(mac, mac, type, name, addr, version, dataJson);
	if (!device)
	{
		LOGW("Device type %s not support for mac %s addr 0x%04X",
			 type.c_str(), mac.c_str(), addr);
		return NULL;
	}

	DeviceListMutex().lock();
	deviceList[device->GetId()] = device;
	DeviceListMutex().unlock();
	return device;
}

Device *DeviceManager::AddDeviceNoResetWithId(string id, string mac,
											  string &type, string &name,
											  uint16_t addr, uint16_t version,
											  Json::Value *dataJson)
{
	NormalizeLowerInPlace(id);
	NormalizeLowerInPlace(mac);
	type = ResolveDeviceTypeForCreate(type, dataJson);

	Device *oldDevice = GetDeviceFromId(id);
	if (oldDevice)
		DelDevice(oldDevice);

	Device *device = CreateDeviceByModel(id, mac, type, name, addr, version, dataJson);
	if (!device)
	{
		LOGW("Device type %s not support for id %s mac %s addr 0x%04X",
			 type.c_str(), id.c_str(), mac.c_str(), addr);
		return NULL;
	}

	DeviceListMutex().lock();
	deviceList[device->GetId()] = device;
	DeviceListMutex().unlock();
	return device;
}

Device *DeviceManager::GetDeviceFromId(string id)
{
	NormalizeLowerInPlace(id);
	DeviceListMutex().lock();
	if (deviceList.find(id) != deviceList.end())
	{
		DeviceListMutex().unlock();
		return deviceList[id];
	}
	DeviceListMutex().unlock();
	return NULL;
}

Device *DeviceManager::GetDeviceFromCloudId(string cloudId)
{
	NormalizeLowerInPlace(cloudId);
	if (cloudId.empty())
		return nullptr;
	Device *match = nullptr;
	DeviceListMutex().lock();
	for (const auto &[id, device] : deviceList)
	{
		if (device->GetCloudId() == cloudId)
		{
			match = device;
			break;
		}
	}
	DeviceListMutex().unlock();
	return match;
}

Device *DeviceManager::GetDeviceFromMac(string mac)
{
	NormalizeLowerInPlace(mac);
	DeviceListMutex().lock();
	for (const auto &[id, device] : deviceList)
	{
		string devMac = device->GetMac();
		NormalizeLowerInPlace(devMac);
		if (devMac == mac)
		{
			DeviceListMutex().unlock();
			return device;
		}
	}
	DeviceListMutex().unlock();
	return NULL;
}

#ifdef CONFIG_ENABLE_BLE
DeviceBle *DeviceManager::GetDeviceBleFromAddr(uint16_t addr)
{
	DeviceListMutex().lock();
	for (const auto &[id, device] : deviceList)
	{
		if (device->CheckAddr(addr) && device->GetProtocol() == BLE_DEVICE)
		{
			DeviceBle *deviceBle = dynamic_cast<DeviceBle *>(device);
			if (deviceBle)
			{
				DeviceListMutex().unlock();
				return deviceBle;
			}
		}
	}
	DeviceListMutex().unlock();
	return NULL;
}

uint32_t DeviceManager::GetNextAddrBleAndroidProvision()
{
	uint32_t rs = 20000;
	if (deviceList.empty())
		return rs;

	auto it = deviceList.begin();
	Device *devMaxAddr = it->second;
	if (devMaxAddr)
	{
		for (; it != deviceList.end(); ++it)
		{
			if (it->second->GetProtocol() == BLE_DEVICE)
			{
				if (it->second->GetAddr() > devMaxAddr->GetAddr())
				{
					devMaxAddr = it->second;
				}
			}
		}

		if ((devMaxAddr->GetAddr() + devMaxAddr->GetNumElement()) > rs)
		{
			rs = devMaxAddr->GetAddr() + devMaxAddr->GetNumElement();
		}
	}

	return rs;
}

uint32_t DeviceManager::GetMaxAddrBle()
{
	uint32_t nextAddr = 2;
	DeviceListMutex().lock();
	for (const auto &[id, device] : deviceList)
	{
		if (device->GetProtocol() == BLE_DEVICE)
		{
			if ((device->GetAddr() > nextAddr) && (device->GetAddr() < 20000))
			{
				nextAddr = device->GetAddr();
			}
		}
	}
	DeviceListMutex().unlock();
	return nextAddr;
}

vector<Device *> DeviceManager::GetListDeviceBle()
{
	vector<Device *> listBle;
	ForEach([&](Device *device)
			{if (device->GetProtocol() == BLE_DEVICE) 
				listBle.push_back(device); });
	return listBle;
}

#endif

void DeviceManager::DelDevice(Device *device)
{
	if (!device)
		return;
	DeviceListMutex().lock();
	deviceList.erase(device->GetId());
	DeviceListMutex().unlock();
	delete device;
}

void DeviceManager::DetachDevice(Device *device)
{
	if (!device)
		return;
	DeviceListMutex().lock();
	deviceList.erase(device->GetId());
	DeviceListMutex().unlock();
}

void DeviceManager::DelAllDevice()
{
	DeviceListMutex().lock();
	for (auto &[id, device] : deviceList)
		delete device;
	deviceList.clear();
	DeviceListMutex().unlock();
}

int DeviceManager::GetDeviceCount()
{
	DeviceListMutex().lock();
	int count = deviceList.size();
	DeviceListMutex().unlock();
	return count;
}

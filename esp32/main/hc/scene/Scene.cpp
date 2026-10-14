#include "Scene.h"
#include "Log.h"
#ifdef CONFIG_ENABLE_BLE
#include "BleProtocol.h"
#endif

DeviceInScene::DeviceInScene(Device *device, Json::Value data)
{
	this->device = device;
	this->data = data;
}

Scene::Scene(string id, uint16_t addr, string name) : Object(id, addr, name)
{
	this->isFavorite = false;
}

Scene::~Scene()
{
	mtx.lock();
	deviceList.clear();
	mtx.unlock();
}

bool Scene::GetIsFavorite()
{
	return this->isFavorite;
}

bool Scene::SetIsFavorite(bool isFavorite)
{
	this->isFavorite = isFavorite;
	return this->isFavorite;
}

Json::Value Scene::GetData()
{
	std::lock_guard<std::mutex> lock(mtx);
	return data;
}

void Scene::SetData(const Json::Value &data)
{
	std::lock_guard<std::mutex> lock(mtx);
	this->data = data;
}

int Scene::GetPositionDevice(Device *device)
{
	uint32_t deviceAddr = device->GetAddr();
	mtx.lock();
	for (uint32_t i = 0; i < deviceList.size(); i++)
	{
		if (deviceAddr == deviceList[i]->device->GetAddr())
		{
			mtx.unlock();
			return i;
		}
	}
	mtx.unlock();
	return CODE_ERROR;
}

Json::Value Scene::GetDataDeviceInScene(Device *device)
{
	mtx.lock();
	for (uint32_t i = 0; i < deviceList.size(); i++)
	{
		if (deviceList[i]->device->GetId() == device->GetId())
		{
			Json::Value data = deviceList[i]->data;
			mtx.unlock();
			return data;
		}
	}
	mtx.unlock();
	return Json::Value();
}

int Scene::AddDevice(Device *device, Json::Value data)
{
#ifdef CONFIG_ENABLE_BLE
	if (device)
	{
		DeviceInScene *deviceInScene = new DeviceInScene(device, data);
		if (deviceInScene)
		{
			int positionDeviceInScene = GetPositionDevice(device);
			mtx.lock();
			if (positionDeviceInScene != CODE_ERROR)
			{
				deviceList.erase(deviceList.begin() + positionDeviceInScene);
			}
			deviceList.push_back(deviceInScene);
			mtx.unlock();
			return CODE_OK;
		}
	}
#endif
	return CODE_ERROR;
}

int Scene::DelDevice(Device *device)
{
#ifdef CONFIG_ENABLE_BLE
	if (device)
	{
		int positionDevInScene = GetPositionDevice(device);
		if (positionDevInScene > -1)
		{
			mtx.lock();
			delete deviceList[positionDevInScene];
			deviceList.erase(deviceList.begin() + positionDevInScene);
			mtx.unlock();
		}
		return CODE_OK;
	}
#endif
	return CODE_ERROR;
}

int Scene::Do(bool ack, uint16_t groupAddr)
{
#ifdef CONFIG_ENABLE_BLE
	return BleProtocol::getInstance()->CallScene(0xffff, addr,
										TRANSITION_DEFAULT, ack);
#endif
	return CODE_OK;
}

bool Scene::IsDevIn(Device *device)
{
	mtx.lock();
	for (auto &devInScene : deviceList)
	{
		if (devInScene->device->GetId() == device->GetId())
		{
			mtx.unlock();
			return true;
		}
	}
	mtx.unlock();
	return false;
}

void Scene::AddDataToListRsp(Device *device, Json::Value &dataValue)
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

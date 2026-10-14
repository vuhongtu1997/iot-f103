#include "RoomManager.h"
#include "Log.h"

using namespace std;

RoomManager *RoomManager::getInstance()
{
	static RoomManager *roomManager = NULL;
	if (!roomManager)
	{
		roomManager = new RoomManager();
	}
	return roomManager;
}

RoomManager::RoomManager()
{
}

void RoomManager::ForEach(function<void(Room *)> func)
{
	roomListMtx.lock();
	for (auto &[id, room] : roomList)
	{
		func(room);
	}
	roomListMtx.unlock();
}

Room *RoomManager::GetRoomFromId(string id)
{
	roomListMtx.lock();
	if (roomList.find(id) != roomList.end())
	{
		roomListMtx.unlock();
		return roomList[id];
	}
	roomListMtx.unlock();
	return NULL;
}

Room *RoomManager::GetRoomFromAddr(uint16_t addr)
{
	roomListMtx.lock();
	for (const auto &[id, room] : roomList)
	{
		if (room->GetAddr() == addr)
		{
			roomListMtx.unlock();
			return room;
		}
	}
	roomListMtx.unlock();
	return NULL;
}

/**
 * HC phân bổ ID nhóm thiết bị
 * 4095 địa chỉ cho nhóm thiết bị tạo thủ công < từ C000-> CFFF>
 * 11520 nhóm thiết bị tự động < từ D000-> FD00> : 45 phòng tối đa trên 1 Hc, 255 nhóm thiết bị trong một phòng
 * ID bắt đầu của phòng là D000-C000 = 4096
 */
uint16_t RoomManager::GetNextRoomAddr()
{
	uint16_t roomAddrStart = ROOM_ADDR_START;
	uint16_t numGroupInRoom = ROOM_NUM_GROUPS_MAX;
	uint8_t numRoomMax = NUM_ROOMS_MAX;
	bool addrExist = false;
	uint16_t indexMax = roomAddrStart + (numGroupInRoom * (numRoomMax - 1));
	// get max addr
	uint16_t rs = roomAddrStart;
	roomListMtx.lock();
	for (const auto &[id, room] : roomList)
	{
		if (room->GetAddr() >= rs && room->GetAddr() < indexMax)
		{
			rs = (roomAddrStart + numGroupInRoom) + ((room->GetAddr() - roomAddrStart) / numGroupInRoom) * numGroupInRoom;
		}
	}
	roomListMtx.unlock();

	// get index null
	if (rs >= indexMax)
	{
		for (int i = 0; i < numRoomMax; i++)
		{
			addrExist = false;
			roomListMtx.lock();
			for (const auto &[id, room] : roomList)
			{
				if (room->GetAddr() == (roomAddrStart + (numGroupInRoom * i)))
				{
					addrExist = true;
					break;
				}
			}
			roomListMtx.unlock();
			if (!addrExist)
			{
				return (roomAddrStart + (numGroupInRoom * i));
			}
		}
		LOGW("group full");
	}
	return rs;
}

Room *RoomManager::AddRoom(Room *room)
{
	if (room)
	{
		roomListMtx.lock();
		roomList[room->GetId()] = room;
		roomListMtx.unlock();
	}
	return room;
}

void RoomManager::DelRoom(Room *room)
{
	roomListMtx.lock();
	roomList.erase(room->GetId());
	roomListMtx.unlock();
	delete room;
}

void RoomManager::DelAllRoom()
{
	roomListMtx.lock();
	for (auto &[id, room] : roomList)
		delete room;
	roomList.clear();
	roomListMtx.unlock();
}

void RoomManager::DelDevInAllRoom(Device *device)
{
	if (device)
	{
		roomListMtx.lock();
		for (auto &[id, room] : roomList)
		{
			for (auto &devInRoom : room->deviceList)
			{
				if (devInRoom->device->GetId() == device->GetId())
				{
					room->DelDevice(devInRoom->device, devInRoom->epId);
				}
			}
		}
		roomListMtx.unlock();
	}
}

Room *RoomManager::GetRoomDeviceIn(Device *device)
{
	roomListMtx.lock();
	for (auto &[id, room] : roomList)
	{
		if (room->IsDevIn(device))
		{
			roomListMtx.unlock();
			return room;
		}
	}
	roomListMtx.unlock();
	return NULL;
}
Room *RoomManager::GetRoomGroupIn(Group *group)
{
	roomListMtx.lock();
	for (auto &[id, room] : roomList)
	{
		if (room->IsGroupInRoom(group))
		{
			roomListMtx.unlock();
			return room;
		}
	}
	roomListMtx.unlock();
	return NULL;
}
Room *RoomManager::GetRoomSceneIn(Scene *scene)
{
	roomListMtx.lock();
	for (auto &[id, room] : roomList)
	{
		if (room->IsSceneInRoom(scene))
		{
			roomListMtx.unlock();
			return room;
		}
	}
	roomListMtx.unlock();
	return NULL;
}

void RoomManager::PrintRoom()
{
	roomListMtx.lock();
	for (auto &[id, grp] : roomList)
	{
		LOGD("room: %s", id.c_str());
		for (auto &dev : grp->deviceList)
		{
			LOGD("\tdev:%s: %d", dev->device->GetId().c_str(), dev->epId);
		}
	}
	roomListMtx.unlock();
}

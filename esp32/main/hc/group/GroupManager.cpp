#include "GroupManager.h"
#include "Log.h"

using namespace std;

GroupManager *GroupManager::getInstance()
{
	static GroupManager *groupManager = NULL;
	if (!groupManager)
	{
		groupManager = new GroupManager();
	}
	return groupManager;
}

GroupManager::GroupManager()
{
}

void GroupManager::ForEach(function<void(Group *)> func)
{
	groupListMtx.lock();
	for (auto &[id, group] : groupList)
	{
		func(group);
	}
	groupListMtx.unlock();
}

void GroupManager::AddGroup(Group *group)
{
	if (group)
	{
		groupListMtx.lock();
		groupList[group->GetId()] = group;
		groupListMtx.unlock();
	}
}

Group *GroupManager::GetGroupFromId(string id)
{
	groupListMtx.lock();
	if (groupList.find(id) != groupList.end())
	{
		groupListMtx.unlock();
		return groupList[id];
	}
	groupListMtx.unlock();
	return NULL;
}

Group *GroupManager::GetGroupFromAddr(uint16_t addr)
{
	groupListMtx.lock();
	for (const auto &[id, group] : groupList)
	{
		if (group->GetAddr() == addr)
		{
			groupListMtx.unlock();
			return group;
		}
	}
	groupListMtx.unlock();
	return NULL;
}

uint16_t GroupManager::GetNextGroupAddr()
{
	uint16_t groupAddr = 1; // start add of normal group
	groupListMtx.lock();
	for (const auto &[id, group] : groupList)
	{
		if (group->GetAddr() >= groupAddr && group->GetAddr() < 4096)
		{
			groupAddr = group->GetAddr() + 1;
		}
	}
	groupListMtx.unlock();
	return groupAddr;
}

void GroupManager::DelGroup(Group *group)
{
	std::lock_guard<std::mutex> lock(groupListMtx);
	auto it = groupList.find(group->GetId());
	if (it != groupList.end())
	{
		delete it->second;
		groupList.erase(it);
	}
}

void GroupManager::DelAllGroup()
{
	groupListMtx.lock();
	for (auto &[id, group] : groupList)
		delete group;
	groupList.clear();
	groupListMtx.unlock();
}

void GroupManager::PrintGroup()
{
	groupListMtx.lock();
	for (auto &[id, grp] : groupList)
	{
		LOGD("group: %s", id.c_str());
		for (auto &dev : grp->deviceList)
		{
			LOGD("\tdev:%s: 0X%04X", dev->device->GetId().c_str(), dev->epId);
		}
	}
	groupListMtx.unlock();
}

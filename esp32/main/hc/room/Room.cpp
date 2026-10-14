#include "Room.h"
#include "Log.h"

using namespace std;

Room::Room(string id, uint16_t addr, string name) : Group(id, addr, name)
{
	dataConfig = "";
}

Room::~Room()
{
	mtxGroup.lock();
	groupList.clear();
	mtxGroup.unlock();

	mtxScene.lock();
	sceneList.clear();
	mtxScene.unlock();

	mtxRule.lock();
	ruleList.clear();
	mtxRule.unlock();
}

int Room::GetPositionGroup(Group *group)
{
	string id = group->GetId();
	mtxGroup.lock();
	for (uint32_t i = 0; i < groupList.size(); i++)
	{
		if (id == groupList[i]->GetId())
		{
			mtxGroup.unlock();
			return i;
		}
	}
	mtxGroup.unlock();
	return CODE_ERROR;
}

int Room::GetPositionScene(Scene *scene)
{
	string id = scene->GetId();
	mtxScene.lock();
	for (int i = 0; i < sceneList.size(); i++)
	{
		if (id == sceneList[i]->GetId())
		{
			mtxScene.unlock();
			return i;
		}
	}
	mtxScene.unlock();
	return CODE_ERROR;
}

int Room::GetPositionRule(Rule *rule)
{
	string id = rule->GetId();
	mtxRule.lock();
	for (int i = 0; i < ruleList.size(); i++)
	{
		if (id == ruleList[i]->GetId())
		{
			mtxRule.unlock();
			return i;
		}
	}
	mtxRule.unlock();
	return CODE_ERROR;
}

string Room::GetDataConfig()
{
	return this->dataConfig;
}

void Room::SetDataConfig(string dataConfig)
{
	this->dataConfig = dataConfig;
}

int Room::AddGroup(Group *group)
{
	if (GetPositionGroup(group) < 0)
	{
		mtxGroup.lock();
		groupList.push_back(group);
		mtxGroup.unlock();
		return CODE_OK;
	}
	return CODE_ERROR;
}

int Room::DelGroup(Group *group)
{
	int position = GetPositionGroup(group);
	if (position > -1)
	{
		mtxGroup.lock();
		groupList.erase(groupList.begin() + position);
		mtxGroup.unlock();
		return CODE_OK;
	}
	return CODE_ERROR;
}

int Room::AddScene(Scene *scene)
{
	if (GetPositionScene(scene) < 0)
	{
		mtxScene.lock();
		sceneList.push_back(scene);
		mtxScene.unlock();
		return CODE_OK;
	}
	return CODE_ERROR;
}

int Room::DelScene(Scene *scene)
{
	int position = GetPositionScene(scene);
	if (position > -1)
	{
		mtxScene.lock();
		sceneList.erase(sceneList.begin() + position);
		mtxScene.unlock();
		return CODE_OK;
	}
	return CODE_ERROR;
}

int Room::AddRule(Rule *rule)
{
	if (GetPositionRule(rule) < 0)
	{
		mtxScene.lock();
		ruleList.push_back(rule);
		mtxScene.unlock();
		return CODE_OK;
	}
	return CODE_ERROR;
}

int Room::DelRule(Rule *rule)
{
	int position = GetPositionRule(rule);
	if (position > -1)
	{
		mtxRule.lock();
		ruleList.erase(ruleList.begin() + position);
		mtxRule.unlock();
		return CODE_OK;
	}
	return CODE_ERROR;
}

bool Room::IsGroupInRoom(Group *group)
{
	if (group)
	{
		return (GetPositionGroup(group) > -1);
	}
	return false;
}
bool Room::IsSceneInRoom(Scene *scene)
{
	if (scene)
	{
		return (GetPositionScene(scene) > -1);
	}
	return false;
}
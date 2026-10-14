#include "RuleManager.h"
#include "Log.h"
#include "Util.h"
#include "DeviceManager.h"
#include "GroupManager.h"
#include "SceneManager.h"
#include "RuleInputDevice.h"
#include "RuleInputTimer.h"
#include "RuleOutputDelay.h"
#include "RuleOutputDevice.h"
#include "RuleOutputGateway.h"
#include "RuleOutputGroup.h"
#include "RuleOutputScene.h"
#include "BleProtocol.h"
#include "Device.h"
#include "Database.h"

#ifdef ESP_PLATFORM
static void startCheckRuleTask(void *arg)
{
	RuleManager *ruleManager = static_cast<RuleManager *>(arg);
	if (ruleManager)
	{
		ruleManager->CheckRuleTask();
	}

	vTaskDelete(NULL);
}
#endif

RuleManager *RuleManager::getInstance()
{
	static RuleManager *ruleManager = NULL;
	if (!ruleManager)
	{
		ruleManager = new RuleManager();
	}
	return ruleManager;
}

RuleManager::RuleManager()
{
#ifdef ESP_PLATFORM
	if (xTaskCreate(startCheckRuleTask, "checkRuleTask", 4096, this, 5, NULL) != pdPASS)
	{
		LOGE("Create CheckRuleTask failed");
	}
#else
	thread checkRuleThread(bind(&RuleManager::CheckRuleTask, this));
	checkRuleThread.detach();
#endif
}

void RuleManager::ForEach(function<void(Rule *)> func)
{
	ruleListMtx.lock();
	for (auto &[id, rule] : ruleList)
	{
		func(rule);
	}
	ruleListMtx.unlock();
}

Rule *RuleManager::AddRule(Json::Value &ruleValue)
{
	LOGD("AddRule");
	if (ruleValue.isMember("id") && ruleValue["id"].isString() &&
		ruleValue.isMember("name") && ruleValue["name"].isString() &&
		ruleValue.isMember("type") && ruleValue["type"].isInt() &&
		ruleValue.isMember("input") && ruleValue["input"].isObject() &&
		ruleValue.isMember("output") && ruleValue["output"].isArray())
	{
		string id = ruleValue["id"].asString();
		int type = ruleValue["type"].asInt();
		Json::Value inputValue = ruleValue["input"];
		Json::Value outputValues = ruleValue["output"];
		string name = ruleValue["name"].asString();
		int repeat = 255;
		uint16_t addr = 0;
		bool isFullDay = true;
		Rule *rule = NULL;

		if (inputValue.isMember("timer") && inputValue["timer"].isObject())
		{
			Json::Value timeRule = ruleValue["time"];
			if (timeRule.isMember("fullDay") && timeRule["fullDay"].isBool() &&
				timeRule.isMember("repeat") && timeRule["repeat"].isInt())
			{
				isFullDay = timeRule["fullDay"].asBool();
				repeat = timeRule["repeat"].asInt();
				if (isFullDay)
				{
					rule = new Rule(id, (RuleType)type, repeat, name, addr, ruleValue);
				}
				else
				{
					if (timeRule.isMember("start") && timeRule["start"].isString() &&
						timeRule.isMember("end") && timeRule["end"].isString())
					{
						string startRule = timeRule["start"].asString();
						string endRule = timeRule["end"].asString();
						rule = new Rule(id, (RuleType)type, repeat, name, addr, Util::ConvertStrTimeToInt(startRule), Util::ConvertStrTimeToInt(endRule), ruleValue, false);
					}
					else
					{
						LOGW("Time format error");
						return NULL;
					}
				}
			}
		}

		if (rule)
		{
			if (ruleValue.isMember("enable") && ruleValue["enable"].isInt())
				rule->SetStatus(ruleValue["enable"].asInt());

			if (inputValue.isMember("timer") && inputValue["timer"].isArray())
			{
				Json::Value timersJson = inputValue["timer"];
				for (auto &timerJson : timersJson)
				{
					if (timerJson.isMember("time") && timerJson["time"].isString() &&
						timerJson.isMember("repeat") && timerJson["repeat"].isInt())
					{
						string timerTime = timerJson["time"].asString();
						int timerRepeat = timerJson["repeat"].asInt();
						RuleInputTimer *ruleInputTimer = new RuleInputTimer(rule, Util::ConvertStrTimeToInt(timerTime), timerRepeat);
						rule->AddRuleInput(ruleInputTimer);
					}
				}
			}
			
			if (inputValue.isMember("device") && inputValue["device"].isArray())
			{
				Json::Value devicesJson = inputValue["device"];
				for (auto &deviceJson : devicesJson)
				{
					if (deviceJson.isObject() && deviceJson.isMember("id") && deviceJson["id"].isString() &&
						deviceJson.isMember("data") && deviceJson["data"].isObject())
					{
						string deviceId = deviceJson["id"].asString();
						Device *deviceInRule = DeviceManager::getInstance()->GetDeviceFromId(deviceId);
						if (deviceInRule)
						{
							Json::Value dataJson = deviceJson["data"];
							RuleInputDevice *ruleInputDevice = new RuleInputDevice(rule, deviceInRule, dataJson);
							rule->AddRuleInput(ruleInputDevice);
						}
						else
							LOGW("Device not found");
					}
				}
			}

			for (Json::Value::ArrayIndex i = 0; i < outputValues.size(); i++)
			{
				Json::Value outputValue = outputValues[i];
				if (outputValue.isMember("delay"))
				{
					RuleOutputDelay *ruleOutputDelay = new RuleOutputDelay(outputValue["delay"].asInt());
					rule->AddRuleOutput(ruleOutputDelay);
				}
				else if (outputValue.isMember("deviceId"))
				{
					if (outputValue["deviceId"].isString() && outputValue.isMember("data") && outputValue["data"].isObject())
					{
						Json::Value dataValue = outputValue["data"];
						string id = outputValue["deviceId"].asString();
						Device *device = DeviceManager::getInstance()->GetDeviceFromId(id);
						if (device)
						{
							RuleOutputDevice *ruleOutputDevice = new RuleOutputDevice(device, dataValue);
							rule->AddRuleOutput(ruleOutputDevice);
						}
						else
							LOGW("Device not found");
					}
				}
				else if (outputValue.isMember("groupId"))
				{
					if (outputValue["groupId"].isString() && outputValue.isMember("data") && outputValue["data"].isObject())
					{
						Json::Value dataValue = outputValue["data"];
						string id = outputValue["groupId"].asString();
						Group *group = GroupManager::getInstance()->GetGroupFromId(id);
						if (group)
						{
							RuleOutputGroup *ruleOutputGroup = new RuleOutputGroup(group, dataValue);
							rule->AddRuleOutput(ruleOutputGroup);
						}
					}
				}
				else if (outputValue.isMember("sceneId"))
				{
					if (outputValue["sceneId"].isString())
					{
						string id = outputValue["sceneId"].asString();
						Scene *scene = SceneManager::getInstance()->GetSceneFromId(id);
						if (scene)
						{
							RuleOutputScene *ruleOutputScene = new RuleOutputScene(scene);
							rule->AddRuleOutput(ruleOutputScene);
						}
					}
				}
			}
			ruleListMtx.lock();
			ruleList[id] = rule;
			ruleListMtx.unlock();
			string ruleStr = ruleValue.toString();
			Database::getInstance()->RuleAdd(rule, ruleStr, 0);
		}
		else
			LOGW("Rule null");
		return rule;
	}
	else
		LOGE("New rule error, out of memory");
	return NULL;
}

Rule *RuleManager::GetRuleFromId(string id)
{
	ruleListMtx.lock();
	if (ruleList.find(id) != ruleList.end())
	{
		ruleListMtx.unlock();
		return ruleList[id];
	}
	ruleListMtx.unlock();
	return NULL;
}

void RuleManager::DelRule(Rule *rule)
{
	ruleListMtx.lock();
	ruleList.erase(rule->GetId());
	ruleListMtx.unlock();
	delete rule;
}

void RuleManager::DelAllRule()
{
	ruleListMtx.lock();
	for (auto &[id, rule] : ruleList)
		delete rule;
	ruleList.clear();
	ruleListMtx.unlock();
}

void RuleManager::CheckRuleTask()
{
	LOGI("Start CheckRuleTask");
	Rule *rule = NULL;
	while (1)
	{
		ruleNeedToCheckListMtx.lock();
		while (ruleNeedToCheckList.size())
		{
			rule = ruleNeedToCheckList.back();
			ruleNeedToCheckList.pop_back();
			rule->Check();
		}
		ruleNeedToCheckListMtx.unlock();
		SLEEP_MS(100);
	}
}

int RuleManager::CheckRule(Rule *rule)
{
	LOGI("Add rule id %s to check", rule->GetId().c_str());
	int rs = CODE_ERROR;
	ruleNeedToCheckListMtx.lock();
	if (ruleNeedToCheckList.size() < MAX_RULE_IN_QUEUE_TO_CHECK)
	{
		ruleNeedToCheckList.push_back(rule);
		rs = CODE_OK;
	}
	ruleNeedToCheckListMtx.unlock();
	return rs;
}

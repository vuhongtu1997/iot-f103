#include "Rule.h"
#include "TimerSchedule.h"
#include "Util.h"
#include "Log.h"
#include <functional>

#ifdef ESP_PLATFORM
#include "Sntp.h"
#endif

using namespace std;

Rule::Rule(string id, RuleType type, unsigned char repeater, string name, uint16_t addr, Json::Value &ruleData) : Object(id, addr, name)
{
	this->type = type;
	this->repeater = repeater;
	this->startTime = -1;
	this->endTime = -1;
	this->ruleData = ruleData;
	this->isFavorite = false;
	isEnable = true;
	timerRegisterIndex = 0;
}

Rule::Rule(string id, RuleType type, unsigned char repeater, string name, uint16_t addr, int startTime, int endTime, Json::Value &ruleData, bool isRegisterTimer) : Object(id, addr, name)
{
	this->type = type;
	this->repeater = repeater;
	this->startTime = startTime;
	this->endTime = endTime;
	this->ruleData = ruleData;
	this->isFavorite = false;
	isEnable = true;
	if (isRegisterTimer)
	{
		timerRegisterIndex = TimerSchedule::getInstance()->RegisterTimer(startTime, this);
	}
	else
	{
		timerRegisterIndex = 0;
	}
}

Rule::~Rule()
{
	LOGI("~Rule");
	for (auto &ruleInput : ruleInputList)
	{
		delete ruleInput;
	}
	for (auto &ruleOutput : ruleOutputList)
	{
		delete ruleOutput;
	}
	if (timerRegisterIndex > 0)
		TimerSchedule::getInstance()->UnregisterTimer(timerRegisterIndex);
}

Json::Value Rule::GetRuleData()
{
	return ruleData;
}

void Rule::SetRuleData(Json::Value ruleData)
{
	this->ruleData = ruleData;
}

RuleType Rule::GetType()
{
	return type;
}

void Rule::Check()
{
	if (isEnable)
	{
		bool checkRuleInputResult = false;
		int currentTimer = Util::GetCurrentTimeInDay();
		int currentWeekDay = Util::GetCurrentWeekDay();
		if (Util::CheckDayInWeek(currentWeekDay, repeater))
		{
			LOGD("Check repeater day OK");
			LOGD("current: %d", currentTimer);
			LOGD("start: %d", startTime);
			LOGD("end: %d", endTime);
			if ((startTime < 0) ||																						// fullDay
				(Util::HaveTime() && ((startTime <= currentTimer && currentTimer <= endTime) ||							// bắt đầu và kết thúc trong cùng 1 ngày
									 (endTime < startTime && (startTime <= currentTimer || currentTimer <= endTime))))) // bắt đầu và kết thúc trong 2 ngày khác nhau
			{
				LOGD("Check time OK");
				if (type == RULE_TYPE_OR || type == RULE_TYPE_TIME_OR || type == RULE_TYPE_TIME)
				{
					checkRuleInputResult = false;
					for (auto &ruleInput : ruleInputList)
					{
						if (ruleInput->Check())
						{
							checkRuleInputResult = true;
							break;
						}
					}
				}
				else if (type == RULE_TYPE_TIME_AND)
				{
					checkRuleInputResult = true;
					for (auto &ruleInput : ruleInputList)
					{
						if (ruleInput->Check() == false)
						{
							checkRuleInputResult = false;
							break;
						}
					}
				}
			}
		}
		if (checkRuleInputResult)
		{
			RunOutput();
		}
	}
}

void Rule::RunOutput()
{
	LOGI("Do output rule id: %s", id.c_str());
	for (auto &ruleOutput : ruleOutputList)
	{
		ruleOutput->RunOutput();
	}
}

void Rule::AddRuleInput(RuleInput *ruleInput)
{
	ruleInputList.push_back(ruleInput);
}

void Rule::AddRuleOutput(RuleOutput *ruleOutput)
{
	ruleOutputList.push_back(ruleOutput);
}

void Rule::DelAllRuleInput()
{
	for (auto ruleInput : ruleInputList)
		delete ruleInput;
	ruleInputList.clear();
}

void Rule::DelAllRuleOutput()
{
	for (auto ruleOutput : ruleOutputList)
		delete ruleOutput;
	ruleOutputList.clear();
}

void Rule::UpdateData(RuleType type, unsigned char repeater, string name, uint16_t addr, Json::Value &ruleData)
{
	this->type = type;
	this->repeater = repeater;
	this->startTime = -1;
	this->endTime = -1;
	this->ruleData = ruleData;
	this->isFavorite = false;
	isEnable = true;
	if (timerRegisterIndex)
	{
		TimerSchedule::getInstance()->UnregisterTimer(timerRegisterIndex);
	}
	timerRegisterIndex = 0;
}

void Rule::UpdateData(RuleType type, unsigned char repeater, string name, uint16_t addr, int startTime, int endTime, Json::Value &ruleData)
{
	this->type = type;
	this->repeater = repeater;
	this->startTime = startTime;
	this->endTime = endTime;
	this->ruleData = ruleData;
	this->isFavorite = false;
	isEnable = true;
	if (timerRegisterIndex)
	{
		TimerSchedule::getInstance()->UnregisterTimer(timerRegisterIndex);
	}
	timerRegisterIndex = 0;
}

void Rule::UpdateData(Json::Value &ruleData)
{
	this->ruleData = ruleData;
}

bool Rule::GetStatus()
{
	return this->isEnable;
}

void Rule::SetStatus(bool enable)
{
	this->isEnable = enable;
}

bool Rule::GetIsFavorite()
{
	return this->isFavorite;
}

void Rule::SetIsFavorite(bool isFavorite)
{
	this->isFavorite = isFavorite;
}

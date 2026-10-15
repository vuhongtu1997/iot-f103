#include "RuleInputTimer.h"
#include "Util.h"
#include "TimerSchedule.h"
#include "RuleManager.h"
#include "Log.h"

RuleInputTimer::RuleInputTimer(Rule *rule, int timer, int repeat)
{
	this->rule = rule;
	this->timer = timer;
	this->repeat = repeat;
	timerRegisterIndex = TimerSchedule::getInstance()->RegisterTimer(timer, bind(&RuleManager::CheckRule, RuleManager::getInstance(), rule));

	if (this->repeat == 0)
		this->isRunOneTime = true;
	else
		this->isRunOneTime = false;
}

RuleInputTimer::~RuleInputTimer()
{
	LOGI("~RuleInputTimer");
	if (timerRegisterIndex > 0)
		TimerSchedule::getInstance()->UnregisterTimer(timerRegisterIndex);
}

bool RuleInputTimer::Check()
{
	if (!Util::HaveTime())
	{
		LOGW("System time has not been synchronized");
		return false;
	}
	int currentWeekDay = Util::GetCurrentWeekDay();
	LOGD("currentWeekDay : %d", currentWeekDay);
	LOGD("repeat : 0x%02X", repeat);
	LOGD("runOneTime: %s", this->isRunOneTime ? "true" : "false");

	bool rs = (Util::CheckDayInWeek(currentWeekDay, repeat) | (this->isRunOneTime)) && timer == Util::GetCurrentTimeInDay();
	if (rs)
		this->isRunOneTime = false;

	return rs;
}

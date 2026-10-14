#include "RuleOutputDelay.h"
#include "Log.h"

RuleOutputDelay::RuleOutputDelay(int delayTime)
{
	this->delayTime = delayTime;
}

RuleOutputDelay::~RuleOutputDelay()
{
	LOGI("~RuleOutputDelay");
}

void RuleOutputDelay::RunOutput()
{
	sleep(delayTime);
}

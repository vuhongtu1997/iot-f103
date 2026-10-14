#include "RuleOutputGroup.h"
#include "Log.h"

RuleOutputGroup::RuleOutputGroup(Group *group, Json::Value &data)
{
	this->group = group;
	this->data = data;
}

RuleOutputGroup::~RuleOutputGroup()
{
	LOGI("~RuleOutputGroup");
}

void RuleOutputGroup::RunOutput()
{
	if (group)
	{
		group->Do(data, true);
	}
}

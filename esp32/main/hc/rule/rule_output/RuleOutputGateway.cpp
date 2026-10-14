#include "RuleOutputGateway.h"
#include "Log.h"
#include "Gateway.h"

RuleOutputGateway::RuleOutputGateway(Json::Value &data)
{
	this->data = data;
}

RuleOutputGateway::~RuleOutputGateway()
{
	LOGI("~RuleOutputGateway");
}

void RuleOutputGateway::RunOutput()
{
	Gateway::getInstance()->Do(data);
}

#include "ActionCallback.h"

ActionCallback::ActionCallback(ActionCallbackFuncType1 actionCallbackFuncType1, string topic)
{
	this->type = 1;
	this->topic = topic;
	this->actionCallbackFuncType1 = actionCallbackFuncType1;
}

ActionCallback::ActionCallback(ActionCallbackFuncType2 actionCallbackFuncType2, string topic)
{
	this->type = 2;
	this->topic = topic;
	this->actionCallbackFuncType2 = actionCallbackFuncType2;
}

ActionCallback::ActionCallback(ActionCallbackFuncType3 actionCallbackFuncType3, string topic)
{
	this->type = 3;
	this->topic = topic;
	this->actionCallbackFuncType3 = actionCallbackFuncType3;
}

ActionCallback::ActionCallback(ActionCallbackFuncType4 actionCallbackFuncType4, string topic)
{
	this->type = 4;
	this->topic = topic;
	this->actionCallbackFuncType4 = actionCallbackFuncType4;
}

int ActionCallback::getType()
{
	return type;
}

void ActionCallback::setTopic(string topic)
{
	this->topic = topic;
}

string ActionCallback::getTopic()
{
	return topic;
}

#pragma once

#include <string>
#include <vector>
#include <functional>

using namespace std;

typedef void (*ActionCallbackFuncType1)(string &topic, string &payload);
typedef void (*ActionCallbackFuncType2)(string &topic, char *payload, int payloadLen);
typedef function<void(string &topic, string &payload)> ActionCallbackFuncType3;
typedef function<void(string &topic, char *payload, int payloadLen)> ActionCallbackFuncType4;

class ActionCallback
{
private:
	int type;
	string topic;

public:
	ActionCallbackFuncType1 actionCallbackFuncType1;
	ActionCallbackFuncType2 actionCallbackFuncType2;
	ActionCallbackFuncType3 actionCallbackFuncType3;
	ActionCallbackFuncType4 actionCallbackFuncType4;
	ActionCallback(ActionCallbackFuncType1 actionCallbackFuncType1, string topic);
	ActionCallback(ActionCallbackFuncType2 actionCallbackFuncType2, string topic);
	ActionCallback(ActionCallbackFuncType3 actionCallbackFuncType3, string topic);
	ActionCallback(ActionCallbackFuncType4 actionCallbackFuncType4, string topic);
	int getType();
	void setTopic(string topic);
	string getTopic();
};

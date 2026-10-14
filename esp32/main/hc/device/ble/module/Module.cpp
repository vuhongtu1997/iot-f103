#include "Module.h"
#include "Device.h"
#include "Log.h"
#include <thread>

Module::Module(Device *device, uint16_t addr, uint32_t index)
{
	this->device = device;
	this->addr = addr;
	this->index = index;
}

Module::~Module() {}

bool Module::CheckAddr(uint16_t addr) { return this->addr == addr; }

void Module::CheckTrigger(Json::Value &data)
{
	LOGI("CheckTrigger %s", data.toString().c_str());
#if 1 // Rule feature disabled
	if (!data.isNull())
	{
		// LOGI("size of deviceRuleInputList: %d", device->deviceRuleInputList.size());
		for (auto &ruleInputDevice : device->deviceRuleInputList)
		{
			Json::Value *ruleValue = ruleInputDevice->GetData();
			LOGI("data: %s, ruleValue: %s", data.toString().c_str(), ruleValue->toString().c_str());
			if (data.isObject() && ruleValue->isArray())
			{
				auto keys = data.getMemberNames();
				string firstKey = keys.empty() ? "" : keys[0];
				LOGD("ruleValue[0]: %s", (*ruleValue)[0].asString().c_str());
				if ((*ruleValue)[0].asString() == firstKey)
				{
					bool rs = false;
					if (CheckData(*ruleInputDevice->GetData(), rs))
						ruleInputDevice->Trigger(rs);
				}
			}
			else
			{
				LOGW("Is not object");
			}
		}
	}
#endif
}

bool Module::CheckData(Json::Value &dataValue, bool &rs)
{
	LOGW("CheckData not implement");
	return false;
}

#include "TimerSchedule.h"
#include "Util.h"
#include "Led.h"
#include "Log.h"
#include "ErrorCode.h"
#include <unistd.h>

using namespace std;

TimerSchedule *TimerSchedule::getInstance()
{
	static TimerSchedule *timerSchedule = NULL;
	if (!timerSchedule)
	{
		timerSchedule = new TimerSchedule();
	}
	return timerSchedule;
}

Timer::Timer(int index, int time, TimerCallbackFunc timerCallbackFunc)
{
	this->index = index;
	this->time = time;
	this->timerCallbackFunc = timerCallbackFunc;
	this->rule = NULL;
}

Timer::Timer(int index, int time, Rule *rule)
{
	this->index = index;
	this->time = time;
	this->rule = rule;
}

int Timer::GetIndex()
{
	return index;
}

bool Timer::IsAtTime(int time)
{
	if (this->time == time)
		return true;
	return false;
}

static void TimerDoThread(void *data)
{
	LOGI("TimerDoThread Start");
	TimerSchedule *timerSchedule = (TimerSchedule *)data;
	Timer *timer = NULL;
	while (1)
	{
		if (xQueueReceive(timerSchedule->queue, &timer, (TickType_t)5))
		{
			if (timer)
			{
				if (timer->rule)
					timer->rule->Check();
				else
					timer->timerCallbackFunc();
			}
		}
		else
			vTaskDelay(pdMS_TO_TICKS(200));
	}
	vTaskDelete(NULL);
}

TimerSchedule::TimerSchedule()
{
	index = 0;
}

TimerSchedule::~TimerSchedule()
{
}

static void TimerThread(void *data)
{
	LOGI("Start Timer Thread");
	TimerSchedule *timerSchedule = (TimerSchedule *)data;
	int currentTimer, oldTimer = 0;
	while (1)
	{
		currentTimer = Util::GetCurrentTimeInDay();
		if (currentTimer != oldTimer)
		{
			// LOGI("Free memory: %d bytes, internal: %d bytes", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
			timerSchedule->mtx.lock();
			for (auto &timer : timerSchedule->timerList)
			{
				if (timer->IsAtTime(currentTimer))
				{
					xQueueSend(timerSchedule->queue, (void *)&timer, (TickType_t)0);
				}
			}
			timerSchedule->mtx.unlock();
			oldTimer = currentTimer;
		}
		else
			vTaskDelay(pdMS_TO_TICKS(500));
	}
	vTaskDelete(NULL);
}

void TimerSchedule::Init()
{
	LOGI("Start Timer init");
	queue = xQueueCreate(10, sizeof(Timer *));
	LOGI("Free memory: %d bytes, internal: %d bytes", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
	if (xTaskCreate(TimerThread, "TimerThread", 2048, this, 10, NULL) != pdPASS)
	{
		LOGE("Failed to create task");
		SetLedService(false);
	}
	vTaskDelay(10);

	if (xTaskCreate(TimerDoThread, "TimerDoThread", 5120, this, 5, NULL) != pdPASS)
	{
		LOGE("Failed to create task");
		SetLedService(false);
	}
	vTaskDelay(10);
}

int TimerSchedule::RegisterTimer(string timerStr, TimerCallbackFunc timerCallbackFunc)
{
	int time = Util::ConvertStrTimeToInt(timerStr);
	return RegisterTimer(time, timerCallbackFunc);
}

int TimerSchedule::RegisterTimer(int time, TimerCallbackFunc timerCallbackFunc)
{
	Timer *timer = new Timer(++index, time, timerCallbackFunc);
	if (timer)
	{
		mtx.lock();
		timerList.push_back(timer);
		mtx.unlock();
	}
	return index;
}

int TimerSchedule::RegisterTimer(string timerStr, Rule *rule)
{
	int time = Util::ConvertStrTimeToInt(timerStr);
	return RegisterTimer(time, rule);
}

int TimerSchedule::RegisterTimer(int time, Rule *rule)
{
	Timer *timer = new Timer(++index, time, rule);
	if (timer)
	{
		mtx.lock();
		timerList.push_back(timer);
		mtx.unlock();
	}
	return index;
}

int TimerSchedule::UnregisterTimer(int index)
{
	std::lock_guard<std::mutex> lock(mtx);

	for (auto it = timerList.begin(); it != timerList.end();)
	{
		Timer *timer = *it;
		if (timer->GetIndex() == index)
		{
			delete timer;
			it = timerList.erase(it);
		}
		else
		{
			++it;
		}
	}

	return CODE_OK;
}

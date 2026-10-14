#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_sntp.h"
#include "Ds1307.h"
#include "Log.h"

#define SNTP_WAIT_UPDATE_TIME 5
#define RTC_SAVE_INTERVAL_MS (60 * 60 * 1000)

static bool _have_ntp_time = false;
static bool _rtc_saved_first_time = false;
static bool _rtc_save_task_started = false;

bool have_ntp_time(void)
{
	return _have_ntp_time;
}

static void rtc_save_task(void *arg)
{
	while (true)
	{
		vTaskDelay(pdMS_TO_TICKS(RTC_SAVE_INTERVAL_MS));

		if (_have_ntp_time)
		{
			ds1307_write_system_time();
		}
	}
}

void time_sync_notification_cb(struct timeval *tv)
{
	/*set GMT +7 time*/
	setenv("TZ", "CST-7", 1);
	tzset();

	LOGI("Notification of a time synchronization event");
	_have_ntp_time = true;

	if (!_rtc_saved_first_time)
	{
		_rtc_saved_first_time = ds1307_write_system_time();
	}
}

void initialize_sntp(bool wait_network)
{
	int retry = 0;

	setenv("TZ", "CST-7", 1);
	tzset();

	ds1307_init();
	ds1307_read_system_time();

	if (!wait_network)
	{
		LOGI("Skip SNTP because network is not ready");
		return;
	}

	LOGI("Initializing SNTP");
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, "pool.ntp.org");
	sntp_set_time_sync_notification_cb(time_sync_notification_cb);
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
	sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
#endif

	sntp_init();

	if (!_rtc_save_task_started)
	{
		xTaskCreate(rtc_save_task, "rtc_save_task", 4096, NULL, 5, NULL);
		_rtc_save_task_started = true;
	}

	while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < SNTP_WAIT_UPDATE_TIME)
	{
		LOGI("Waiting for system time to be set... (retry %d)", retry);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

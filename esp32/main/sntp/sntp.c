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
#include "Log.h"

#define SNTP_WAIT_UPDATE_TIME 5

static bool _have_ntp_time = false;

bool have_ntp_time(void)
{
	return _have_ntp_time;
}

void time_sync_notification_cb(struct timeval *tv)
{
	/*set GMT +7 time*/
	setenv("TZ", "CST-7", 1);
	tzset();

	LOGI("Notification of a time synchronization event");
	_have_ntp_time = true;
}

void initialize_sntp(bool wait_network)
{
	int retry = 0;

	setenv("TZ", "CST-7", 1);
	tzset();

	if (!wait_network)
	{
		LOGI("Starting SNTP; synchronization will follow WiFi provisioning");
	}

	LOGI("Initializing SNTP");
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, "pool.ntp.org");
	sntp_set_time_sync_notification_cb(time_sync_notification_cb);
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
	sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
#endif

	sntp_init();

	while (wait_network && sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < SNTP_WAIT_UPDATE_TIME)
	{
		LOGI("Waiting for system time to be set... (retry %d)", retry);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

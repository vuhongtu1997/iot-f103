#include "Log.h"
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <time.h>

static vprintf_like_t s_log_print_func = &vprintf;

log_level_t log_level = XLOG_INFO;

static uint8_t slog_get_usec()
{
	struct timeval tv;
	if (gettimeofday(&tv, NULL) < 0)
		return 0;
	return (uint8_t)(tv.tv_usec / 10000);
}

static char time_str[16];
char *timestr()
{
	time_t t = time(NULL);
	struct tm *timeinfo;
	timeinfo = localtime(&t);
	sprintf(time_str, "%02d:%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, slog_get_usec());
	return time_str;
}

void log_set_level(log_level_t log_level_)
{
	log_level = log_level_;
}

void log_set_vprintf(vprintf_like_t func)
{
	s_log_print_func = func;
}

void log_write(const char *format, ...)
{
	va_list list;
	va_start(list, format);
	(*s_log_print_func)(format, list);
	va_end(list);
}

char *log_cut_str(char *full_path, uint8_t len)
{
	uint8_t k;
	char *ptr;
	k = strlen(full_path);

	if (k <= len)
		return full_path;

	ptr = full_path + (k - len);
	return ptr;
}

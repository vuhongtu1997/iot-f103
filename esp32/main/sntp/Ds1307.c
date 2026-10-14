#include "Ds1307.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "Log.h"
#include "define.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#define DS1307_I2C_PORT I2C_NUM_0
#define DS1307_I2C_ADDR 0x68
#define DS1307_I2C_FREQ_HZ 100000
#define DS1307_TIMEOUT_MS 1000

#define DS1307_REG_SECONDS 0x00
#define DS1307_REG_COUNT 7

static bool _ds1307_initialized = false;
static bool _ds1307_have_time = false;

static uint8_t bin_to_bcd(int value)
{
	return (uint8_t)(((value / 10) << 4) | (value % 10));
}

static int bcd_to_bin(uint8_t value)
{
	return ((value >> 4) * 10) + (value & 0x0f);
}

static bool ds1307_write_regs(uint8_t reg, const uint8_t *data, size_t len)
{
	uint8_t buffer[1 + DS1307_REG_COUNT];
	if (len > DS1307_REG_COUNT)
	{
		return false;
	}

	buffer[0] = reg;
	memcpy(&buffer[1], data, len);

	esp_err_t err = i2c_master_write_to_device(DS1307_I2C_PORT, DS1307_I2C_ADDR,
											   buffer, len + 1,
											   pdMS_TO_TICKS(DS1307_TIMEOUT_MS));
	if (err != ESP_OK)
	{
		LOGE("DS1307 write error: %s", esp_err_to_name(err));
		return false;
	}

	return true;
}

static bool ds1307_read_regs(uint8_t reg, uint8_t *data, size_t len)
{
	esp_err_t err = i2c_master_write_read_device(DS1307_I2C_PORT,
												 DS1307_I2C_ADDR, &reg, 1,
												 data, len,
												 pdMS_TO_TICKS(DS1307_TIMEOUT_MS));
	if (err != ESP_OK)
	{
		LOGE("DS1307 read error: %s", esp_err_to_name(err));
		return false;
	}

	return true;
}

bool ds1307_init(void)
{
	if (_ds1307_initialized)
	{
		return true;
	}

	i2c_config_t conf = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = CONFIG_SDA_GPIO,
		.scl_io_num = CONFIG_SCL_GPIO,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master = {
			.clk_speed = DS1307_I2C_FREQ_HZ,
		},
		.clk_flags = 0,
	};

	esp_err_t err = i2c_param_config(DS1307_I2C_PORT, &conf);
	if (err != ESP_OK)
	{
		LOGE("DS1307 i2c_param_config error: %s", esp_err_to_name(err));
		return false;
	}

	err = i2c_driver_install(DS1307_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
	if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
	{
		LOGE("DS1307 i2c_driver_install error: %s", esp_err_to_name(err));
		return false;
	}

	_ds1307_initialized = true;
	LOGI("DS1307 initialized on SDA=%d SCL=%d", CONFIG_SDA_GPIO, CONFIG_SCL_GPIO);
	return true;
}

bool ds1307_read_system_time(void)
{
	if (!ds1307_init())
	{
		return false;
	}

	uint8_t regs[DS1307_REG_COUNT] = {0};
	if (!ds1307_read_regs(DS1307_REG_SECONDS, regs, sizeof(regs)))
	{
		_ds1307_have_time = false;
		return false;
	}

	if (regs[0] & 0x80)
	{
		LOGE("DS1307 oscillator is stopped");
		_ds1307_have_time = false;
		return false;
	}

	struct tm tm_time = {0};
	tm_time.tm_sec = bcd_to_bin(regs[0] & 0x7f);
	tm_time.tm_min = bcd_to_bin(regs[1] & 0x7f);

	if (regs[2] & 0x40)
	{
		int hour = bcd_to_bin(regs[2] & 0x1f);
		bool is_pm = (regs[2] & 0x20) != 0;
		if (is_pm && hour < 12)
		{
			hour += 12;
		}
		if (!is_pm && hour == 12)
		{
			hour = 0;
		}
		tm_time.tm_hour = hour;
	}
	else
	{
		tm_time.tm_hour = bcd_to_bin(regs[2] & 0x3f);
	}

	tm_time.tm_wday = bcd_to_bin(regs[3] & 0x07) - 1;
	tm_time.tm_mday = bcd_to_bin(regs[4] & 0x3f);
	tm_time.tm_mon = bcd_to_bin(regs[5] & 0x1f) - 1;
	tm_time.tm_year = bcd_to_bin(regs[6]) + 100;
	tm_time.tm_isdst = -1;

	if (tm_time.tm_year < 124 || tm_time.tm_mon < 0 || tm_time.tm_mon > 11 ||
		tm_time.tm_mday < 1 || tm_time.tm_mday > 31 ||
		tm_time.tm_hour > 23 || tm_time.tm_min > 59 || tm_time.tm_sec > 59)
	{
		LOGE("DS1307 time is invalid");
		_ds1307_have_time = false;
		return false;
	}

	setenv("TZ", "CST-7", 1);
	tzset();

	time_t rtc_time = mktime(&tm_time);
	if (rtc_time <= 0)
	{
		LOGE("DS1307 mktime failed");
		_ds1307_have_time = false;
		return false;
	}

	struct timeval tv = {
		.tv_sec = rtc_time,
		.tv_usec = 0,
	};
	settimeofday(&tv, NULL);

	_ds1307_have_time = true;
	LOGI("System time restored from DS1307");
	return true;
}

bool ds1307_write_system_time(void)
{
	if (!ds1307_init())
	{
		return false;
	}

	time_t now = time(NULL);
	if (now <= 0)
	{
		LOGE("System time is invalid, skip writing DS1307");
		return false;
	}

	setenv("TZ", "CST-7", 1);
	tzset();

	struct tm tm_time = {0};
	if (localtime_r(&now, &tm_time) == NULL)
	{
		LOGE("localtime_r failed, skip writing DS1307");
		return false;
	}

	uint8_t regs[DS1307_REG_COUNT] = {
		bin_to_bcd(tm_time.tm_sec) & 0x7f,
		bin_to_bcd(tm_time.tm_min),
		bin_to_bcd(tm_time.tm_hour),
		bin_to_bcd(tm_time.tm_wday + 1),
		bin_to_bcd(tm_time.tm_mday),
		bin_to_bcd(tm_time.tm_mon + 1),
		bin_to_bcd((tm_time.tm_year + 1900) % 100),
	};

	if (!ds1307_write_regs(DS1307_REG_SECONDS, regs, sizeof(regs)))
	{
		return false;
	}

	_ds1307_have_time = true;
	LOGI("System time saved to DS1307");
	return true;
}

bool ds1307_have_time(void)
{
	return _ds1307_have_time;
}

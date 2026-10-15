#include "ButtonSignal.h"
#include "Gateway.h"
#include "Log.h"
#include "Wifi.h"
#include "Led.h"

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/task.h"

ButtonSignal *ButtonSignal::getInstance()
{
	static ButtonSignal *buttonSignal = NULL;
	if (!buttonSignal)
	{
		buttonSignal = new ButtonSignal();
	}
	return buttonSignal;
}

static bool button_released_after_config_entry = false;
static TaskHandle_t button_task_handle = NULL;

static void FactoryResetTask(void *arg)
{
	ButtonSignal *buttonSignal = static_cast<ButtonSignal *>(arg);
	if (!buttonSignal)
	{
		vTaskDelete(NULL);
		return;
	}

	LOGW("Factory reset sequence confirmed");
	BlinkLedInternetTimes(5, 150, 150);
	Gateway::getInstance()->ResetFactory();
	vTaskDelay(pdMS_TO_TICKS(300));
	esp_restart();
}

static bool is_button_pressed_level()
{
	return gpio_get_level(GPIO_INPUT_IO_0) == 0;
}

static void ButtonSignalHandler(void *arg)
{
	ButtonSignal *buttonSignal = (ButtonSignal *)arg;

	while (1)
	{
		bool pressedNow = is_button_pressed_level();
		if (pressedNow != buttonSignal->isPressed)
		{
			if (pressedNow)
				buttonSignal->OnPress();
			else
				buttonSignal->OnRelease();
		}

		if (buttonSignal->isConfigMode)
		{
			if (!pressedNow)
			{
				if (!button_released_after_config_entry)
				{
					button_released_after_config_entry = true;
					LOGI("Button released - press again to exit config mode");
				}
			}
			else if (button_released_after_config_entry)
			{
				LOGI("Button pressed again - exiting config mode");
				buttonSignal->isConfigMode = false;
				buttonSignal->isConfigHcMode = false;
				button_released_after_config_entry = false;
				ExitLedConfigMode();
				esp_restart();
			}

			vTaskDelay(pdMS_TO_TICKS(buttonSignal->isConfigHcMode ? 50 : 100));
		}
		else
		{
			buttonSignal->PollFactoryResetSequence();
			vTaskDelay(pdMS_TO_TICKS(100));
		}
	}
}

ButtonSignal::ButtonSignal()
{
	isPressed = false;
	isConfigMode = false;
	isConfigHcMode = false;
	isFactoryResetting = false;
	factoryResetPressCount = 0;
	factoryResetWindowStartUs = 0;
}

ButtonSignal::~ButtonSignal()
{
}

void ButtonSignal::Init()
{
	LOGI("Free memory: %d bytes, internal: %d bytes", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
	if (button_task_handle)
	{
		return;
	}

	if (xTaskCreate(ButtonSignalHandler, "Button", 4096, (void *)this, 10,
					&button_task_handle) != pdPASS)
	{
		LOGE("Failed to create task");
		SetLedService(false);
	}
}

void ButtonSignal::OnPress()
{
	LOGI("OnPress");
	isPressed = true;
}

void ButtonSignal::OnRelease()
{
	LOGI("OnRelease");
	isPressed = false;

	if (isConfigMode || isFactoryResetting)
	{
		return;
	}

	const int64_t nowUs = esp_timer_get_time();
	if (factoryResetPressCount == 0)
	{
		factoryResetWindowStartUs = nowUs;
	}

	factoryResetPressCount++;
	LOGI("Factory reset click count: %d", factoryResetPressCount);

	if ((nowUs - factoryResetWindowStartUs) > 10000000LL)
	{
		LOGW("Factory reset window exceeded during click sequence");
		factoryResetPressCount = 0;
		factoryResetWindowStartUs = 0;
		BlinkBothLedsTimes(2, 150, 150);
		return;
	}

	if (factoryResetPressCount >= 5)
	{
		StartFactoryReset();
	}
}

bool ButtonSignal::GetStatus()
{
	return this->isPressed;
}

bool ButtonSignal::CheckBootConfigRequest()
{
	LOGI("Waiting for button press to enter config mode...");
	LOGI("Hold button for 5 seconds to enter configuration mode");

	uint32_t start_time = esp_timer_get_time();
	while ((esp_timer_get_time() - start_time) < 2000000)
	{
		if (is_button_pressed_level())
		{
			LOGI("Button pressed - holding for 5 seconds...");
			bool button_held = true;

			for (int countdown = 5; countdown > 0; countdown--)
			{
				LOGI("Hold button for %d more seconds...", countdown);
				for (int i = 0; i < 10; i++)
				{
					if (!is_button_pressed_level())
					{
						button_held = false;
						LOGI("Button released too early");
						return false;
					}
					vTaskDelay(pdMS_TO_TICKS(100));
				}

				if (!button_held)
				{
					return false;
				}
			}

			if (button_held)
			{
				LOGI("Button held for 5 seconds - entering configuration mode");
				return true;
			}
		}
		vTaskDelay(pdMS_TO_TICKS(500));
	}
	return false;
}

void ButtonSignal::EnterConfigMode()
{
	LOGI("Button pressed, entering config mode");
	isConfigMode = true;
	isConfigHcMode = false;
	button_released_after_config_entry = false;

	EnterLedConfigMode();
}

void ButtonSignal::SetConfigHcMode(bool enabled)
{
	isConfigHcMode = enabled;
	if (isConfigMode)
	{
		SetLedConfigBusy(enabled);
	}
}

void ButtonSignal::CompleteConfigMode()
{
	if (!isConfigMode)
		return;

	LOGI("Cloud connected - completing config mode");
	isConfigMode = false;
	isConfigHcMode = false;
	button_released_after_config_entry = false;
	ExitLedConfigMode();
}

bool ButtonSignal::IsConfigMode() const
{
	return isConfigMode;
}

void ButtonSignal::PollFactoryResetSequence()
{
	if (isConfigMode || isFactoryResetting || factoryResetPressCount <= 0)
	{
		return;
	}

	const int64_t nowUs = esp_timer_get_time();
	if ((nowUs - factoryResetWindowStartUs) <= 10000000LL)
	{
		return;
	}

	LOGW("Factory reset sequence timeout after %d clicks",
		 factoryResetPressCount);
	factoryResetPressCount = 0;
	factoryResetWindowStartUs = 0;
	BlinkBothLedsTimes(2, 150, 150);
}

void ButtonSignal::StartFactoryReset()
{
	if (isFactoryResetting)
	{
		return;
	}

	isFactoryResetting = true;
	factoryResetPressCount = 0;
	factoryResetWindowStartUs = 0;

	if (xTaskCreate(FactoryResetTask, "FactoryReset", 6144, this, 11, NULL) !=
		pdPASS)
	{
		LOGE("Failed to create factory reset task");
		isFactoryResetting = false;
	}
}

void gpio_init(void)
{
	gpio_config_t io_conf = {};
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	gpio_config(&io_conf);
}

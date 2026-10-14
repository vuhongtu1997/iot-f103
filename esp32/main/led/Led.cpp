#include "Led.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static bool statusLedService = false;
static bool statusLedInternet = false;
static led_mode_t modeLedService = LED_OFF;
static led_mode_t modeLedInternet = LED_OFF;

// Mutex bảo vệ LED
static SemaphoreHandle_t led_mutex = NULL;

// esp_timer cho toggle BLINK/FLASH
static esp_timer_handle_t led_timer = NULL;

// Counter đơn giản cho toggle
static uint8_t blink_counter = 0;
static bool toogle_service_flash = false;
static bool toogle_service_blink = false;
static bool toogle_internet_flash = false;
static bool toogle_internet_blink = false;
static bool config_mode_active = false;
static bool connection_state_active = false;
static bool internet_connected = false;
static bool server_connected = false;
static bool internet_connection_led_on = false;
static bool server_connection_led_on = false;
static uint8_t internet_connection_tick = 0;
static uint8_t server_connection_tick = 0;
static bool manual_override_active = false;

static inline int led_level_from_status(bool on)
{
    return on ? 0 : 1;
}

static inline void set_led_gpio_locked(bool serviceOn, bool internetOn)
{
    gpio_set_level(LED_GPIO_SERVICE, led_level_from_status(serviceOn));
    gpio_set_level(LED_GPIO_INTERNET, led_level_from_status(internetOn));
}
// LEDC init
static void led_init_channels()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LED_GPIO_SERVICE) | (1ULL << LED_GPIO_INTERNET);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(LED_GPIO_SERVICE, 0);
    gpio_set_level(LED_GPIO_INTERNET, 0);
}

// Mutex lock/unlock
static void led_lock()
{
    if (led_mutex)
        xSemaphoreTake(led_mutex, portMAX_DELAY);
}
static void led_unlock()
{
    if (led_mutex)
        xSemaphoreGive(led_mutex);
}

// Timer callback
static void led_timer_callback(void *arg)
{
    blink_counter++;

    led_lock();
    if (manual_override_active)
    {
        led_unlock();
        return;
    }
    if (config_mode_active || connection_state_active)
    {
        const uint8_t toggle_ticks = config_mode_active ? 5 : 20;

        if (internet_connected)
        {
            internet_connection_led_on = true;
            internet_connection_tick = 0;
        }
        else if (++internet_connection_tick >= toggle_ticks)
        {
            internet_connection_tick = 0;
            internet_connection_led_on = !internet_connection_led_on;
        }

        if (server_connected)
        {
            server_connection_led_on = true;
            server_connection_tick = 0;
        }
        else if (++server_connection_tick >= toggle_ticks)
        {
            server_connection_tick = 0;
            server_connection_led_on = !server_connection_led_on;
        }

        gpio_set_level(LED_GPIO_INTERNET,
                       led_level_from_status(internet_connection_led_on));
        gpio_set_level(LED_GPIO_SERVICE,
                       led_level_from_status(server_connection_led_on));
        led_unlock();
        return;
    }

    // LED Service
    if (modeLedService == LED_BLINK)
    {
        if ((blink_counter % 10) == 0)
            toogle_service_blink = !toogle_service_blink;
        gpio_set_level(LED_GPIO_SERVICE, toogle_service_blink ? 1 : 0);
    }
    else if (modeLedService == LED_FLASH)
    {
        if ((blink_counter % 5) == 0)
            toogle_service_flash = !toogle_service_flash;
        gpio_set_level(LED_GPIO_SERVICE, toogle_service_flash ? 1 : 0);
    }
    else if (modeLedService == LED_ON)
        gpio_set_level(LED_GPIO_SERVICE, 1);
    else
        gpio_set_level(LED_GPIO_SERVICE, 0);

    // LED Internet
    if (modeLedInternet == LED_BLINK)
    {
        if ((blink_counter % 10) == 0)
            toogle_internet_blink = !toogle_internet_blink;
        gpio_set_level(LED_GPIO_INTERNET, toogle_internet_blink ? 1 : 0);
    }
    else if (modeLedInternet == LED_FLASH)
    {
        if ((blink_counter % 5) == 0)
            toogle_internet_flash = !toogle_internet_flash;
        gpio_set_level(LED_GPIO_INTERNET, toogle_internet_flash ? 1 : 0);
    }
    else if (modeLedInternet == LED_ON)
        gpio_set_level(LED_GPIO_INTERNET, 1);
    else
        gpio_set_level(LED_GPIO_INTERNET, 0);
    led_unlock();
}

// ----------------- Init LED module -----------------
void Led_init()
{
    if (!led_mutex)
        led_mutex = xSemaphoreCreateMutex();
    led_init_channels();

    esp_timer_create_args_t timer_args = {
        .callback = &led_timer_callback,
        .name = "led_timer"};
    esp_timer_create(&timer_args, &led_timer);
    esp_timer_start_periodic(led_timer, 50 * 1000);
}

// ----------------- LED control -----------------
void SetLedService(bool on)
{
    modeLedService = !on ? LED_ON : LED_OFF;
    statusLedService = on;
}
void SetLedInternet(bool on)
{
    modeLedInternet = !on ? LED_ON : LED_OFF;
    statusLedInternet = on;
}
void BlinkLedService() { modeLedService = LED_BLINK; }
void FlashLedService() { modeLedService = LED_FLASH; }
void BlinkLedInternet() { modeLedInternet = LED_BLINK; }
void FlashLedInternet() { modeLedInternet = LED_FLASH; }

// ----------------- Lấy trạng thái -----------------
bool GetStatusLedService() { return statusLedService; }
bool GetStatusLedInternet() { return statusLedInternet; }
led_mode_t GetModeLedService() { return modeLedService; }
led_mode_t GetModeLedInternet() { return modeLedInternet; }

void EnterLedConfigMode()
{
    led_lock();
    config_mode_active = true;
    connection_state_active = true;
    internet_connected = false;
    server_connected = false;
    internet_connection_led_on = true;
    server_connection_led_on = true;
    internet_connection_tick = 0;
    server_connection_tick = 0;
    led_unlock();
}

void SetLedConfigBusy(bool busy)
{
    (void)busy;
}

void SetLedConfigWifiConnected(bool connected)
{
    SetLedInternetConnected(connected);
}

void SetLedConfigServerConnected(bool connected)
{
    SetLedServerConnected(connected);
}

void SetLedInternetConnected(bool connected)
{
    led_lock();
    connection_state_active = true;
    internet_connected = connected;
    internet_connection_led_on = connected;
    internet_connection_tick = 0;
    led_unlock();
}

void SetLedServerConnected(bool connected)
{
    led_lock();
    connection_state_active = true;
    server_connected = connected;
    server_connection_led_on = connected;
    server_connection_tick = 0;
    led_unlock();
}

void ExitLedConfigMode()
{
    led_lock();
    config_mode_active = false;
    internet_connection_tick = 0;
    server_connection_tick = 0;
    led_unlock();
}

bool IsLedConfigMode() { return config_mode_active; }

static void blink_manual_pattern(uint8_t times, bool serviceOn, bool internetOn,
                                 uint32_t onMs, uint32_t offMs)
{
    if (times == 0)
        return;

    for (uint8_t i = 0; i < times; ++i)
    {
        led_lock();
        manual_override_active = true;
        set_led_gpio_locked(serviceOn, internetOn);
        led_unlock();
        vTaskDelay(pdMS_TO_TICKS(onMs));

        led_lock();
        set_led_gpio_locked(false, false);
        led_unlock();
        vTaskDelay(pdMS_TO_TICKS(offMs));
    }

    led_lock();
    manual_override_active = false;
    led_unlock();
}

void BlinkLedInternetTimes(uint8_t times, uint32_t onMs, uint32_t offMs)
{
    blink_manual_pattern(times, false, true, onMs, offMs);
}

void BlinkBothLedsTimes(uint8_t times, uint32_t onMs, uint32_t offMs)
{
    blink_manual_pattern(times, true, true, onMs, offMs);
}

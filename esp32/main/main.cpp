
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "sqlite3.h"
#include <new>

#include <driver/gpio.h>
#include <driver/uart.h>

#include "Wifi.h"
#include "Sntp.h"
#include "ButtonSignal.h"
// #include "TimerSchedule.h"
#include "Database.h"
#include "Led.h"
#include "Log.h"

#ifdef CONFIG_ENABLE_BLE
#include "BleProtocol.h"
#endif

extern void gpio_init(void);

void *operator new(size_t size)
{
  // Ưu tiên cấp phát từ PSRAM
  void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  if (!ptr)
  {
    // Nếu PSRAM hết hoặc chưa sẵn sàng, thử cấp phát từ RAM nội bộ
    ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }

  if (!ptr)
  {
    // Lưu ý: Throw exception khi hệ thống chưa khởi tạo xong (trước app_main)
    // có thể gây crash do TLS chưa sẵn sàng.
    throw std::bad_alloc();
  }
  return ptr;
}

void operator delete(void *ptr) noexcept { heap_caps_free(ptr); }

extern "C" void app_main(void)
{
  vTaskDelay(pdMS_TO_TICKS(100));

  esp_log_level_set("*", ESP_LOG_DEBUG);
  log_set_level(XLOG_DEBUG);

  srand(time(NULL));
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
    if (err != ESP_OK)
    {
      LOGE("NVS Init error %s", esp_err_to_name(err));
    }
  }

  Led_init();
  gpio_init();
  ButtonSignal *buttonSignal = ButtonSignal::getInstance();
  bool bootConfigMode = buttonSignal->CheckBootConfigRequest();

  Wifi::Init(bootConfigMode);
  Sntp::init(!bootConfigMode && Wifi::WaitConnecting());
  buttonSignal->Init();
  if (bootConfigMode)
  {
    buttonSignal->EnterConfigMode();
  }

  Database::getInstance()->Init();
  if (!bootConfigMode)
  {
    for (int i = 0; i < 2; i++)
    {
      SetLedInternet(true);
      SetLedService(true);
      usleep(200000);
      SetLedInternet(false);
      SetLedService(false);
      usleep(200000);
    }

    SetLedService(true);
    SetLedInternet(false);
  }

  SetLedInternetConnected(Wifi::IsNetworkConnected());
  SetLedServerConnected(false);

  // TimerSchedule::getInstance()->Init();

#ifdef CONFIG_ENABLE_BLE
  BleProtocol::getInstance()->Init();
#endif

  Gateway::getInstance()->Init();

#ifdef CONFIG_ENABLE_BLE
  BleProtocol::getInstance()->InitKey();
#endif

  while (true)
  {
    vTaskDelay(pdMS_TO_TICKS(60000));
  }
}

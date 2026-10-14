#include "Ethernet.h"

#include "Define.h"
#include "Led.h"
#include "Log.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include "driver/gpio.h"
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_netif_glue.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lwip/inet.h"

namespace
{
  constexpr EventBits_t ETH_CONNECTED_BIT = BIT0;
  constexpr int ETH_ROUTE_PRIORITY = 150;

  EventGroupHandle_t ethEventGroup = nullptr;
  esp_eth_handle_t ethHandle = nullptr;
  esp_eth_mac_t *ethMac = nullptr;
  esp_eth_phy_t *ethPhy = nullptr;
  esp_eth_netif_glue_handle_t ethGlue = nullptr;
  esp_netif_t *ethNetif = nullptr;
  esp_event_handler_instance_t ethEventInstance = nullptr;
  esp_event_handler_instance_t gotIpEventInstance = nullptr;
  esp_event_handler_instance_t lostIpEventInstance = nullptr;
  bool started = false;

  void clearConnected()
  {
    if (ethEventGroup)
    {
      xEventGroupClearBits(ethEventGroup, ETH_CONNECTED_BIT);
    }
  }

  void ethEventHandler(void *, esp_event_base_t, int32_t eventId, void *)
  {
    switch (eventId)
    {
    case ETHERNET_EVENT_CONNECTED:
      LOGI("Ethernet link up");
      break;
    case ETHERNET_EVENT_DISCONNECTED:
      clearConnected();
      LOGW("Ethernet link down");
      break;
    case ETHERNET_EVENT_START:
      LOGI("Ethernet started");
      break;
    case ETHERNET_EVENT_STOP:
      clearConnected();
      LOGI("Ethernet stopped");
      break;
    default:
      break;
    }
  }

  void ipEventHandler(void *, esp_event_base_t, int32_t eventId, void *eventData)
  {
    if (eventId == IP_EVENT_ETH_GOT_IP)
    {
      auto *event = static_cast<ip_event_got_ip_t *>(eventData);
      xEventGroupSetBits(ethEventGroup, ETH_CONNECTED_BIT);
      SetLedInternetConnected(true);
      esp_netif_dns_info_t dns = {};
      IP4_ADDR(&dns.ip.u_addr.ip4, 8, 8, 8, 8);
      // IP4_ADDR(&dns.ip.u_addr.ip4, 1, 1, 1, 1);
      dns.ip.type = ESP_IPADDR_TYPE_V4;

      esp_err_t dnsErr =
          ethNetif ? esp_netif_set_dns_info(ethNetif, ESP_NETIF_DNS_MAIN, &dns)
                   : ESP_ERR_INVALID_STATE;
      if (dnsErr == ESP_OK)
      {
        LOGI("Ethernet DNS overridden to 8.8.8.8");
      }
      else
      {
        LOGW("Ethernet DNS override failed: %s", esp_err_to_name(dnsErr));
      }

      LOGI("Ethernet got IP: " IPSTR, IP2STR(&event->ip_info.ip));
      return;
    }

    if (eventId == IP_EVENT_ETH_LOST_IP)
    {
      clearConnected();
      SetLedInternetConnected(false);
      LOGW("Ethernet lost IP");
    }
  }

  void cleanup()
  {
    if (ethHandle && started)
    {
      esp_eth_stop(ethHandle);
    }
    started = false;

    if (ethEventInstance)
    {
      esp_event_handler_instance_unregister(ETH_EVENT, ESP_EVENT_ANY_ID,
                                            ethEventInstance);
      ethEventInstance = nullptr;
    }
    if (gotIpEventInstance)
    {
      esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                            gotIpEventInstance);
      gotIpEventInstance = nullptr;
    }
    if (lostIpEventInstance)
    {
      esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_ETH_LOST_IP,
                                            lostIpEventInstance);
      lostIpEventInstance = nullptr;
    }
    if (ethGlue)
    {
      esp_eth_del_netif_glue(ethGlue);
      ethGlue = nullptr;
    }
    if (ethHandle)
    {
      esp_eth_driver_uninstall(ethHandle);
      ethHandle = nullptr;
    }
    if (ethPhy)
    {
      ethPhy->del(ethPhy);
      ethPhy = nullptr;
    }
    if (ethMac)
    {
      ethMac->del(ethMac);
      ethMac = nullptr;
    }
    if (ethNetif)
    {
      esp_netif_destroy(ethNetif);
      ethNetif = nullptr;
    }
    clearConnected();
  }
} // namespace

bool Ethernet::Start()
{
  if (started)
  {
    return true;
  }

  if (!ethEventGroup)
  {
    ethEventGroup = xEventGroupCreate();
    if (!ethEventGroup)
    {
      LOGE("Cannot create Ethernet event group");
      return false;
    }
  }
  clearConnected();

  gpio_config_t resetConfig = {};
  resetConfig.pin_bit_mask = 1ULL << ETH_MCU_RESET;
  resetConfig.mode = GPIO_MODE_OUTPUT;
  resetConfig.pull_up_en = GPIO_PULLUP_DISABLE;
  resetConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
  resetConfig.intr_type = GPIO_INTR_DISABLE;
  if (gpio_config(&resetConfig) != ESP_OK)
  {
    LOGE("Cannot configure Ethernet reset GPIO");
    return false;
  }

  gpio_set_level(ETH_MCU_RESET, 0);
  vTaskDelay(pdMS_TO_TICKS(100));
  gpio_set_level(ETH_MCU_RESET, 1);
  vTaskDelay(pdMS_TO_TICKS(100));

  esp_netif_inherent_config_t baseConfig = ESP_NETIF_INHERENT_DEFAULT_ETH();
  baseConfig.route_prio = ETH_ROUTE_PRIORITY;
  esp_netif_config_t netifConfig = {
      .base = &baseConfig,
      .driver = nullptr,
      .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH};
  ethNetif = esp_netif_new(&netifConfig);
  if (!ethNetif)
  {
    LOGE("Cannot create Ethernet netif");
    cleanup();
    return false;
  }

  eth_mac_config_t macConfig = ETH_MAC_DEFAULT_CONFIG();
  eth_phy_config_t phyConfig = ETH_PHY_DEFAULT_CONFIG();
  eth_esp32_emac_config_t emacConfig = ETH_ESP32_EMAC_DEFAULT_CONFIG();
  emacConfig.smi_gpio.mdc_num = ETH_MDC_GPIO;
  emacConfig.smi_gpio.mdio_num = ETH_MDIO_GPIO;
  phyConfig.phy_addr = ETH_PHY_ADDR;
  phyConfig.reset_gpio_num = ETH_RESET_PIN;

  ethMac = esp_eth_mac_new_esp32(&emacConfig, &macConfig);
  ethPhy = esp_eth_phy_new_lan87xx(&phyConfig);
  if (!ethMac || !ethPhy)
  {
    LOGE("Cannot create Ethernet MAC/PHY");
    cleanup();
    return false;
  }

  esp_eth_config_t ethConfig = ETH_DEFAULT_CONFIG(ethMac, ethPhy);
  esp_err_t err = esp_eth_driver_install(&ethConfig, &ethHandle);
  if (err != ESP_OK)
  {
    LOGE("Ethernet driver install failed: %s", esp_err_to_name(err));
    cleanup();
    return false;
  }

  ethGlue = esp_eth_new_netif_glue(ethHandle);
  if (!ethGlue)
  {
    LOGE("Cannot create Ethernet netif glue");
    cleanup();
    return false;
  }

  err = esp_netif_attach(ethNetif, ethGlue);
  if (err != ESP_OK)
  {
    LOGE("Ethernet netif attach failed: %s", esp_err_to_name(err));
    cleanup();
    return false;
  }

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      ETH_EVENT, ESP_EVENT_ANY_ID, ethEventHandler, nullptr, &ethEventInstance));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_ETH_GOT_IP, ipEventHandler, nullptr,
      &gotIpEventInstance));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_ETH_LOST_IP, ipEventHandler, nullptr,
      &lostIpEventInstance));

  err = esp_eth_start(ethHandle);
  if (err != ESP_OK)
  {
    LOGE("Ethernet start failed: %s", esp_err_to_name(err));
    cleanup();
    return false;
  }

  started = true;
  LOGI("Ethernet initialized: LAN87xx, MDC=%d, MDIO=%d, PHY=%d",
       ETH_MDC_GPIO, ETH_MDIO_GPIO, ETH_PHY_ADDR);
  return true;
}

void Ethernet::Stop()
{
  cleanup();
}

bool Ethernet::IsStarted()
{
  return started;
}

bool Ethernet::WaitForIp(unsigned int timeoutMs)
{
  if (!started || !ethEventGroup)
  {
    return false;
  }

  EventBits_t bits = xEventGroupWaitBits(
      ethEventGroup, ETH_CONNECTED_BIT, pdFALSE, pdTRUE,
      pdMS_TO_TICKS(timeoutMs));
  return (bits & ETH_CONNECTED_BIT) != 0;
}

bool Ethernet::IsConnected()
{
  return ethEventGroup &&
         (xEventGroupGetBits(ethEventGroup) & ETH_CONNECTED_BIT) != 0;
}

std::string Ethernet::GetIP()
{
  if (!ethNetif)
  {
    return "";
  }

  esp_netif_ip_info_t ipInfo = {};
  if (esp_netif_get_ip_info(ethNetif, &ipInfo) != ESP_OK ||
      ipInfo.ip.addr == 0)
  {
    return "";
  }

  char ip[16] = {};
  snprintf(ip, sizeof(ip), IPSTR, IP2STR(&ipInfo.ip));
  return ip;
}

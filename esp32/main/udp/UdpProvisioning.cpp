#include "UdpProvisioning.h"
#include "Wifi.h"
#include "json.h"
#include "Log.h"
#include "ErrorCode.h"
#include "Util.h"
#include "Define.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <cerrno>
#include <cstring>
#include <string>

namespace
{
  TaskHandle_t task = nullptr;
  constexpr uint16_t port = 8181;

  void reply(int fd, const sockaddr_in &peer, const Json::Value &value)
  {
    const std::string text = value.toString();
    if (sendto(fd, text.data(), text.size(), 0,
               reinterpret_cast<const sockaddr *>(&peer), sizeof peer) < 0)
      LOGW("UDP provisioning reply failed: %d", errno);
  }

  void handle(int fd, const sockaddr_in &peer, const char *packet, size_t size)
  {
    Json::Value request;
    if (!request.parse(packet, size) || !request.isObject() ||
        !request["cmd"].isString() || !request["rqi"].isString() ||
        !request["data"].isObject())
      return;
    const std::string cmd = request["cmd"].asString();
    const Json::Value &data = request["data"];
    Json::Value response;
    response["rqi"] = request["rqi"];
    response["cmd"] = cmd + "Rsp";
    response["data"]["code"] = CODE_OK;
    response["data"]["mac"] = Wifi::GetMacAddress();

    if (cmd == "SCAN_HC" || cmd == "aiHubBroadCast" || cmd == "scanIpHc")
    {
      response["data"]["ip"] = Wifi::GetIP();
      response["data"]["name"] = "RD_MH_" + Wifi::GetMacAddress().substr(6);
      response["data"]["setup_active"] = Wifi::IsProvisioningActive();
      response["data"]["type"] = MODEL;
      response["data"]["ver"] = STR(VERSION);
      if (cmd == "SCAN_HC")
      {
        response["CMD"] = "HC_RESPONSE";
        response["IP"] = Wifi::GetIP();
        response["MAC"] = Wifi::GetMacAddress();
        response["HOSTNAME"] = response["data"]["name"];
        response["TYPE"] = 2;
        response["VERSION"] = STR(VERSION);
        response["TLS"] = false;
        response["MQTT_PORT"] = 1883;
      }
      if (cmd == "aiHubBroadCast")
        response["cmd"] = "getHcInfoRsp";
    }
    else if (cmd == "HC_SCAN_WIFI" || cmd == "setupHC")
    {
      // Accept credential changes only from the setup AP subnet while it is active.
      if (!Wifi::IsProvisioningPeer(peer.sin_addr.s_addr))
      {
        response["data"]["code"] = CODE_ERROR;
        response["data"]["error"] = "connect_to_setup_ap";
      }
      else if (cmd == "HC_SCAN_WIFI")
      {
        Json::Value networks;
        response["CMD"] = "HC_SCAN_WIFI_RESPONSE";
        response["FROM"]["TYPE"] = 2;
        response["TO"]["TYPE"] = 0;
        response["CONNECTION_TYPE"] = 0;
        response["TIME"] = Util::GetCurrentTimeStr();
        response["REQUEST_ID"] = data.isMember("REQUEST_ID") ? data["REQUEST_ID"] : request["rqi"];
        if (!Wifi::ScanWifi(networks))
          response["data"]["code"] = CODE_ERROR;
        response["DATA"] = networks;
      }
      else if (!data["ssid"].isString() || !data["pass"].isString())
      {
        response["data"]["code"] = CODE_FORMAT_ERROR;
      }
      else
      {
        const std::string ssid = data["ssid"].asString();
        const std::string password = data["pass"].asString();
        if (ssid.empty() || ssid.size() > 32 || ssid.find('\0') != std::string::npos ||
            password.find('\0') != std::string::npos ||
            (!password.empty() && (password.size() < 8 || password.size() > 63)))
        {
          response["data"]["code"] = CODE_FORMAT_ERROR;
        }
        else
        {
          response["data"]["status"] = "connecting";
          reply(fd, peer, response);
          const bool connected = Wifi::SetStaCredentials(ssid, password) && Wifi::WaitStaConnecting();
          response["data"]["code"] = connected ? CODE_OK : CODE_ERROR;
          response["data"]["status"] = connected ? "connected" : "connection_failed";
          response["data"]["ip"] = Wifi::GetIP();
        }
      }
    }
    else
    {
      response["data"]["code"] = CODE_FORMAT_ERROR;
      response["data"]["error"] = "unknown_command";
    }
    reply(fd, peer, response);
  }

  void run(void *)
  {
    for (;;)
    {
      int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      if (fd < 0)
      {
        vTaskDelay(pdMS_TO_TICKS(1000));
        continue;
      }
      sockaddr_in local{};
      local.sin_family = AF_INET;
      local.sin_port = htons(port);
      local.sin_addr.s_addr = htonl(INADDR_ANY);
      if (bind(fd, reinterpret_cast<sockaddr *>(&local), sizeof local) != 0)
      {
        close(fd);
        vTaskDelay(pdMS_TO_TICKS(1000));
        continue;
      }
      LOGI("UDP provisioning listening on port %u", port);
      for (;;)
      {
        char packet[1025];
        sockaddr_in peer{};
        socklen_t peerSize = sizeof peer;
        int size = recvfrom(fd, packet, sizeof packet, 0,
                            reinterpret_cast<sockaddr *>(&peer), &peerSize);
        if (size < 0)
          break;
        // Reject oversized datagrams instead of parsing a truncated command.
        if (size > 0 && size <= 1024)
          handle(fd, peer, packet, size);
      }
      close(fd);
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}
void UdpProvisioning::Init()
{
  if (!task && xTaskCreate(run, "UdpProvision", 8192, nullptr, 5, &task) != pdPASS)
    LOGE("Failed to start UDP provisioning task");
}

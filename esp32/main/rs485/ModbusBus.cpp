#include "ModbusBus.h"

#include "protocol.h"
#include "define.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "Database.h"
#include "DeviceManager.h"
#include "DeviceModbus.h"
#include "DeviceModbusGas.h"
#include "DeviceModbusTemperatureHumidity.h"
#include "DeviceModbusCH3.h"
#include "Gateway.h"
#include "Log.h"

namespace
{
  constexpr uart_port_t BUS_UART = UART_NUM_1;
  constexpr size_t BUS_RX_BUFFER_SIZE = 1024;
  constexpr uint32_t POLL_INTERVAL_MS = 5000;

  ModbusNodeInfo registeredNodes[MAX_NODES]{};
  ModbusNodeInfo scanNodes[MAX_NODES]{};
  ModbusNodeInfo previousNodes[MAX_NODES]{};
  size_t registeredNodeCount = 0;
  QueueHandle_t uartEvents = nullptr;
  std::mutex busMutex;
  bool initialized = false;

  bool UidIsBlank(const uint8_t uid[12])
  {
    uint8_t any = 0;
    for (size_t i = 0; i < 12; ++i)
      any |= uid[i];
    return any == 0;
  }

  std::string NodeUidHex(const ModbusNodeInfo &node)
  {
    if (UidIsBlank(node.uid))
      return "";
    char uid[25];
    for (size_t i = 0; i < sizeof(node.uid); ++i)
      snprintf(uid + i * 2, sizeof(uid) - i * 2, "%02x", node.uid[i]);
    return uid;
  }

  std::string NodeDeviceId(const ModbusNodeInfo &node)
  {
    const std::string uid = NodeUidHex(node);
    return std::string("rs485-") +
           (uid.empty() ? "pending-" + std::to_string(node.address) : uid);
  }

  DeviceModbus *CreateSensorDevice(const ModbusNodeInfo &node)
  {
    const std::string parentId = NodeDeviceId(node);
    const std::string childId =
        DeviceModbus::GetSensorDeviceId(parentId, node.sensorType);
    const char *type = DeviceModbus::GetSensorDeviceType(node.sensorType);
    if (childId.empty() || !type)
      return nullptr;

    const std::string uid = NodeUidHex(node);
    const std::string name =
        std::string(DeviceModbus::GetSensorName(node.sensorType)) + " sensor " +
        std::to_string(node.address);
    Json::Value data = Json::objectValue;
    data["uid"] = uid;
    data["parent_id"] = parentId;
    data["address"] = node.address;
    data["sensor_type"] = node.sensorType;
    data["sensor_type_name"] = DeviceModbus::GetSensorName(node.sensorType);
    data["version"] = static_cast<Json::UInt64>(node.version);
    data["stt"] = 0;

    DeviceModbus *sensor = nullptr;
    switch (node.sensorType)
    {
    case MODBUS_SENSOR_GAS:
      sensor = new DeviceModbusGas(childId, name, uid, node.address,
                                   node.version, node.sensorType, &data);
      break;
    case MODBUS_SENSOR_TEMPERATURE_HUMIDITY:
      sensor = new DeviceModbusTemperatureHumidity(
          childId, name, uid, node.address, node.version, node.sensorType, &data);
      break;
    case MODBUS_SENSOR_CH3:
      sensor = new DeviceModbusCH3(childId, name, uid, node.address,
                                   node.version, node.sensorType, &data);
      break;
    default:
      break;
    }
    return sensor;
  }

  bool PersistRegistry(const ModbusNodeInfo *previousNodes,
                       size_t previousNodeCount)
  {
    Database *database = Database::getInstance();
    DeviceManager *manager = DeviceManager::getInstance();
    for (size_t i = 0; i < registeredNodeCount; ++i)
    {
      const std::string id = NodeDeviceId(registeredNodes[i]);
      Device *existing = manager->GetDeviceFromId(id);
      if (existing && existing->GetProtocol() != MODBUS_DEVICE)
      {
        LOGE("Device ID %s is already used by another protocol", id.c_str());
        return false;
      }

      const std::string childId =
          DeviceModbus::GetSensorDeviceId(id, registeredNodes[i].sensorType);
      if (!childId.empty())
      {
        existing = manager->GetDeviceFromId(childId);
        if (existing &&
            (existing->GetProtocol() != MODBUS_DEVICE ||
             existing->GetType() !=
                 DeviceModbus::GetSensorDeviceType(registeredNodes[i].sensorType)))
        {
          LOGE("Device ID %s is already used by another device type",
               childId.c_str());
          return false;
        }
      }
    }

    std::set<std::string> desiredChildIds;
    for (size_t i = 0; i < registeredNodeCount; ++i)
    {
      const std::string childId = DeviceModbus::GetSensorDeviceId(
          NodeDeviceId(registeredNodes[i]), registeredNodes[i].sensorType);
      if (!childId.empty())
        desiredChildIds.insert(childId);
    }
    std::vector<std::string> staleChildIds;
    for (DeviceModbus *sensor : manager->GetListDeviceModbusSensors())
    {
      if (!desiredChildIds.count(sensor->GetId()))
        staleChildIds.push_back(sensor->GetId());
    }

    if (!database->EnsureReady() ||
        database->SqliteBenginTransaction() != SQLITE_OK)
    {
      LOGE("Cannot start SQLite transaction for Modbus registry");
      return false;
    }

    for (size_t i = 0; i < registeredNodeCount; ++i)
    {
      const ModbusNodeInfo &node = registeredNodes[i];
      const std::string id = NodeDeviceId(node);
      const std::string uid = NodeUidHex(node);
      const std::string name =
          "RS485 node " + std::to_string(node.address);
      Json::Value data = Json::objectValue;
      data["uid"] = uid;
      data["version"] = static_cast<Json::UInt64>(node.version);
      data["mode"] = node.mode;
      data["sensor_type"] = node.sensorType;
      data["sensor_type_name"] = DeviceModbus::GetSensorName(node.sensorType);
      data["stt"] = 0;

      DeviceModbus record(id, name, uid, node.address, node.version,
                          node.mode, node.sensorType, &data);
      if (database->DeviceAdd(&record) != SQLITE_OK)
      {
        LOGE("Cannot save Modbus device %s to SQLite", id.c_str());
        database->SqliteRollbackTransaction();
        return false;
      }
    }

    for (size_t i = 0; i < registeredNodeCount; ++i)
    {
      std::unique_ptr<DeviceModbus> sensor(CreateSensorDevice(registeredNodes[i]));
      if (!sensor)
        continue;
      if (database->DeviceAdd(sensor.get()) != SQLITE_OK)
      {
        LOGE("Cannot save Modbus child device %s to SQLite",
             sensor->GetId().c_str());
        database->SqliteRollbackTransaction();
        return false;
      }
    }

    for (const std::string &childId : staleChildIds)
    {
      if (database->DeviceDel(childId) != SQLITE_OK)
      {
        LOGE("Cannot remove stale Modbus child device %s from SQLite",
             childId.c_str());
        database->SqliteRollbackTransaction();
        return false;
      }
    }

    for (size_t i = 0; i < previousNodeCount; ++i)
    {
      const std::string oldId = NodeDeviceId(previousNodes[i]);
      bool stillRegistered = false;
      for (size_t j = 0; j < registeredNodeCount; ++j)
        stillRegistered = stillRegistered || NodeDeviceId(registeredNodes[j]) == oldId;
      if (stillRegistered)
        continue;

      if (database->DeviceDel(oldId) != SQLITE_OK)
      {
        LOGE("Cannot remove stale Modbus reservation %s from SQLite",
             oldId.c_str());
        database->SqliteRollbackTransaction();
        return false;
      }
    }

    if (database->SqliteEndTransaction() != SQLITE_OK)
    {
      LOGE("Cannot commit Modbus registry transaction");
      return false;
    }

    for (size_t i = 0; i < registeredNodeCount; ++i)
    {
      const ModbusNodeInfo &node = registeredNodes[i];
      const std::string id = NodeDeviceId(node);
      const std::string uid = NodeUidHex(node);
      const std::string name = "RS485 node " + std::to_string(node.address);
      Json::Value data = Json::objectValue;
      data["uid"] = uid;
      data["version"] = static_cast<Json::UInt64>(node.version);
      data["mode"] = node.mode;
      data["sensor_type"] = node.sensorType;
      data["sensor_type_name"] = DeviceModbus::GetSensorName(node.sensorType);
      data["stt"] = 0;
      std::string deviceType = DeviceModbus::TYPE;
      std::string deviceName = name;
      Device *existing = manager->GetDeviceFromId(id);
      if (existing)
      {
        existing->SetData(data);
        existing->SetAddr(node.address);
        existing->SetName(name);
        existing->lastOnlineState = false;
      }
      else if (!manager->AddDeviceNoResetWithId(
                   id, uid, deviceType, deviceName, node.address,
                   static_cast<uint16_t>(node.version & 0xffff), &data))
      {
        LOGE("Cannot register Modbus device %s in DeviceManager", id.c_str());
        return false;
      }
    }

    for (size_t i = 0; i < registeredNodeCount; ++i)
    {
      std::unique_ptr<DeviceModbus> sensor(CreateSensorDevice(registeredNodes[i]));
      if (!sensor)
        continue;
      std::string id = sensor->GetId();
      std::string uid = sensor->GetMac();
      std::string type = sensor->GetType();
      std::string name = sensor->GetName();
      Json::Value data = sensor->GetData();
      if (!manager->AddDeviceNoResetWithId(
              id, uid, type, name, sensor->GetAddr(), sensor->GetVersion(),
              &data))
      {
        LOGE("Cannot register Modbus child device %s in DeviceManager",
             id.c_str());
        return false;
      }
    }

    for (const std::string &childId : staleChildIds)
    {
      Device *oldDevice = manager->GetDeviceFromId(childId);
      if (oldDevice)
        manager->DelDevice(oldDevice);
    }

    for (size_t i = 0; i < previousNodeCount; ++i)
    {
      const std::string oldId = NodeDeviceId(previousNodes[i]);
      bool stillRegistered = false;
      for (size_t j = 0; j < registeredNodeCount; ++j)
        stillRegistered = stillRegistered ||
                          NodeDeviceId(registeredNodes[j]) == oldId;
      if (!stillRegistered)
      {
        Device *oldDevice = manager->GetDeviceFromId(oldId);
        if (oldDevice)
          manager->DelDevice(oldDevice);
      }
    }

    return true;
  }

  bool LoadRegistry()
  {
    memset(registeredNodes, 0, sizeof(registeredNodes));
    registeredNodeCount = 0;
    const std::vector<DeviceModbus *> devices =
        DeviceManager::getInstance()->GetListDeviceModbus();
    if (devices.size() > MAX_NODES)
    {
      LOGE("SQLite contains more Modbus devices than the bus address range");
      return false;
    }

    for (DeviceModbus *device : devices)
    {
      if (!device || registeredNodeCount == MAX_NODES)
        return false;
      ModbusNodeInfo &node = registeredNodes[registeredNodeCount];
      node.address = static_cast<uint8_t>(device->GetAddr());
      node.version = device->GetFirmwareVersion();
      node.mode = device->GetMode();
      node.sensorType = device->GetSensorType();
      const std::string storedUid = device->GetUid();
      if (!storedUid.empty() && !device->CopyUid(node.uid))
      {
        LOGE("SQLite contains invalid Modbus UID for %s",
             device->GetId().c_str());
        return false;
      }
      ++registeredNodeCount;
    }

    for (size_t i = 0; i < registeredNodeCount; ++i)
    {
      if (registeredNodes[i].address == 0 ||
          registeredNodes[i].address > MAX_NODES)
      {
        LOGE("Modbus registry contains invalid address");
        return false;
      }
      if (UidIsBlank(registeredNodes[i].uid) &&
          NodeDeviceId(registeredNodes[i]) !=
              "rs485-pending-" +
                  std::to_string(registeredNodes[i].address))
      {
        LOGE("SQLite Modbus reservation has no UID and is not pending");
        return false;
      }
      for (size_t j = 0; j < i; ++j)
      {
        if (registeredNodes[i].address == registeredNodes[j].address ||
            (!UidIsBlank(registeredNodes[i].uid) &&
             !UidIsBlank(registeredNodes[j].uid) &&
             constant_equal(registeredNodes[i].uid, registeredNodes[j].uid,
                            sizeof(registeredNodes[i].uid))))
        {
          LOGE("Modbus registry contains duplicate address or UID");
          return false;
        }
      }
    }
    return true;
  }

  int BusExchange(uint8_t address, uint8_t function, const uint8_t *payload,
                  size_t payloadLength, uint8_t *response,
                  size_t responseCapacity, int timeoutMs, int *activity)
  {
    if (!initialized || payloadLength + 4 > FRAME_MAX ||
        (payloadLength && !payload) || !response)
      return 0;

    uint8_t tx[FRAME_MAX];
    uint8_t rx[FRAME_MAX];
    tx[0] = address;
    tx[1] = function;
    if (payloadLength)
      memcpy(tx + 2, payload, payloadLength);
    put16(tx + 2 + payloadLength, crc16(tx, 2 + payloadLength));

    vTaskDelay(pdMS_TO_TICKS(4));
    if (uart_flush_input(BUS_UART) != ESP_OK)
      return 0;
    xQueueReset(uartEvents);
    int written = uart_write_bytes(BUS_UART, tx, payloadLength + 4);
    if (written != static_cast<int>(payloadLength + 4) ||
        uart_wait_tx_done(BUS_UART, pdMS_TO_TICKS(100)) != ESP_OK)
      return 0;

    const int64_t start = esp_timer_get_time();
    int64_t lastByte = start;
    size_t received = 0;
    bool seen = false;
    bool badFrame = false;
    while (esp_timer_get_time() - start < static_cast<int64_t>(timeoutMs) * 1000)
    {
      uint8_t chunk[64];
      int count = uart_read_bytes(BUS_UART, chunk, sizeof(chunk), pdMS_TO_TICKS(1));
      if (count > 0)
      {
        seen = true;
        lastByte = esp_timer_get_time();
        if (received + static_cast<size_t>(count) > sizeof(rx))
          badFrame = true;
        else
        {
          memcpy(rx + received, chunk, static_cast<size_t>(count));
          received += static_cast<size_t>(count);
        }
      }
      uart_event_t event;
      while (xQueueReceive(uartEvents, &event, 0) == pdTRUE)
      {
        if (event.type == UART_FRAME_ERR || event.type == UART_PARITY_ERR ||
            event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL)
        {
          seen = true;
          badFrame = true;
        }
      }
      if (received && esp_timer_get_time() - lastByte > 4000)
        break;
    }

    if (activity)
      *activity = seen;
    const bool matchingFunction =
        received >= 2 &&
        (rx[1] == function || rx[1] == static_cast<uint8_t>(function | 0x80));
    if (badFrame || received < 4 || received > responseCapacity ||
        rx[0] != address || !matchingFunction || crc16(rx, received) != 0)
      return 0;
    memcpy(response, rx, received);
    return static_cast<int>(received);
  }

  int ProbeNode(uint8_t address, ModbusNodeInfo *node, int *activity)
  {
    const uint8_t request[4] = {0, 0, 0, 12};
    uint8_t response[FRAME_MAX];
    int seen = 0;
    int length = BusExchange(address, 3, request, sizeof(request), response,
                             sizeof(response), 60, &seen);
    if (activity)
      *activity = seen;
    if (!length)
      return seen ? -1 : 0;
    if (length != 29 || response[2] != 24 || response[13] != 0 ||
        response[14] != address)
      return -1;

    memset(node, 0, sizeof(*node));
    node->address = address;
    node->mode = response[12];
    node->version = (static_cast<uint32_t>(response[7]) << 24) |
                    (static_cast<uint32_t>(response[8]) << 16) |
                    (static_cast<uint32_t>(response[9]) << 8) |
                    response[10];
    memcpy(node->uid, response + 15, sizeof(node->uid));
    if (UidIsBlank(node->uid))
      return -1;

    const uint8_t sensorTypeRequest[4] = {
        0, MODBUS_SENSOR_TYPE_REGISTER, 0, 1};
    uint8_t sensorTypeResponse[FRAME_MAX];
    const int sensorTypeLength =
        BusExchange(address, 3, sensorTypeRequest, sizeof(sensorTypeRequest),
                    sensorTypeResponse, sizeof(sensorTypeResponse), 60, nullptr);
    if (sensorTypeLength == 7 && sensorTypeResponse[2] == 2)
    {
      const uint16_t sensorType =
          static_cast<uint16_t>(sensorTypeResponse[3] << 8) |
          sensorTypeResponse[4];
      if (sensorType > MODBUS_SENSOR_CH3)
        return -1;
      node->sensorType = static_cast<uint8_t>(sensorType);
    }
    else if (sensorTypeLength != 5 || sensorTypeResponse[1] != 0x83 ||
             sensorTypeResponse[2] != 2)
      return -1;
    return 1;
  }

  void PollTask(void *)
  {
    bool initialScan = true;
    for (;;)
    {
      if (initialScan)
      {
        initialScan = false;
        if (!ModbusDiscoverNodes())
          LOGW("Initial Modbus scan failed; retained registry is unchanged");
      }

      std::vector<ModbusNodeInfo> nodes;
      ModbusGetNodes(nodes);
      for (const ModbusNodeInfo &node : nodes)
      {
        uint16_t registers[12]{};
        const bool online = ModbusReadNode(node, registers);
        Gateway::getInstance()->PublishModbusTelemetry(node, registers, online);
      }
      vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
  }
} // namespace

bool ModbusBusInit()
{
  std::lock_guard<std::mutex> lock(busMutex);
  if (initialized)
    return true;

  uart_config_t config = {
      .baud_rate = BUS_BAUD,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };
  esp_err_t err = uart_driver_install(BUS_UART, BUS_RX_BUFFER_SIZE, 0, 32,
                                      &uartEvents, 0);
  if (err == ESP_OK)
    err = uart_param_config(BUS_UART, &config);
  if (err == ESP_OK)
    err = uart_set_pin(BUS_UART, RS485_TX_PIN, RS485_RX_PIN, RS485_DE_PIN,
                       UART_PIN_NO_CHANGE);
  if (err == ESP_OK)
    err = uart_set_mode(BUS_UART, UART_MODE_RS485_HALF_DUPLEX);
  if (err != ESP_OK)
  {
    LOGE("RS485 UART initialization failed: %s", esp_err_to_name(err));
    if (uartEvents)
    {
      uart_driver_delete(BUS_UART);
      uartEvents = nullptr;
    }
    return false;
  }

  if (!LoadRegistry())
  {
    uart_driver_delete(BUS_UART);
    uartEvents = nullptr;
    return false;
  }
  initialized = true;

  if (xTaskCreate(PollTask, "modbus_poll", 4096, nullptr, 5, nullptr) != pdPASS)
  {
    initialized = false;
    uart_driver_delete(BUS_UART);
    uartEvents = nullptr;
    LOGE("Cannot create RS485 polling task");
    return false;
  }
  LOGI("RS485 Modbus RTU initialized: UART1 TX=%d RX=%d DE=%d, %u nodes",
       RS485_TX_PIN, RS485_RX_PIN, RS485_DE_PIN,
       static_cast<unsigned>(registeredNodeCount));
  return true;
}

bool ModbusDiscoverNodes()
{
  std::lock_guard<std::mutex> lock(busMutex);
  if (!initialized)
    return false;

  memset(scanNodes, 0, sizeof(scanNodes));
  size_t count = registeredNodeCount;
  memcpy(scanNodes, registeredNodes, count * sizeof(scanNodes[0]));

  for (unsigned address = 1; address <= MAX_NODES; ++address)
  {
    ModbusNodeInfo discovered{};
    int result = ProbeNode(static_cast<uint8_t>(address), &discovered, nullptr);
    if (result < 0)
      return false;
    if (result == 0)
      continue;

    size_t slot = count;
    for (size_t i = 0; i < count; ++i)
    {
      if (scanNodes[i].address == address)
      {
        slot = i;
        if (!UidIsBlank(scanNodes[i].uid) &&
            !constant_equal(scanNodes[i].uid, discovered.uid,
                            sizeof(discovered.uid)))
          return false;
      }
      else if (!UidIsBlank(scanNodes[i].uid) &&
               constant_equal(scanNodes[i].uid, discovered.uid,
                              sizeof(discovered.uid)))
      {
        return false;
      }
    }
    if (slot == count)
    {
      if (count == MAX_NODES)
        return false;
      ++count;
    }
    else if (discovered.sensorType == MODBUS_SENSOR_UNKNOWN &&
             constant_equal(scanNodes[slot].uid, discovered.uid,
                            sizeof(discovered.uid)))
    {
      discovered.sensorType = scanNodes[slot].sensorType;
    }
    scanNodes[slot] = discovered;
  }

  const size_t previousCount = registeredNodeCount;
  memcpy(previousNodes, registeredNodes, previousCount * sizeof(previousNodes[0]));
  memcpy(registeredNodes, scanNodes, count * sizeof(scanNodes[0]));
  registeredNodeCount = count;
  if (PersistRegistry(previousNodes, previousCount))
    return true;

  memcpy(registeredNodes, previousNodes,
         previousCount * sizeof(previousNodes[0]));
  registeredNodeCount = previousCount;
  return false;
}

int ModbusAssignAddress(uint8_t address, ModbusNodeInfo *assigned)
{
  std::lock_guard<std::mutex> lock(busMutex);
  if (!initialized || address == 0 || address > MAX_NODES ||
      registeredNodeCount == MAX_NODES)
    return -4;

  for (size_t i = 0; i < registeredNodeCount; ++i)
    if (registeredNodes[i].address == address)
      return -1;

  ModbusNodeInfo probe{};
  for (unsigned attempt = 0; attempt < 3; ++attempt)
  {
    int result = ProbeNode(address, &probe, nullptr);
    if (result > 0)
      return -1;
    if (result < 0)
      return -2;
  }

  const size_t previousCount = registeredNodeCount;
  memcpy(previousNodes, registeredNodes,
         previousCount * sizeof(previousNodes[0]));
  const size_t slot = registeredNodeCount++;
  memset(&registeredNodes[slot], 0, sizeof(registeredNodes[slot]));
  registeredNodes[slot].address = address;
  registeredNodes[slot].mode = 2;
  if (!PersistRegistry(previousNodes, previousCount))
  {
    --registeredNodeCount;
    memset(&registeredNodes[slot], 0, sizeof(registeredNodes[slot]));
    return -5;
  }

  uint8_t frame[8] = {0, 6, 0x01, 0x00, 0x00, address, 0, 0};
  put16(frame + 6, crc16(frame, 6));
  vTaskDelay(pdMS_TO_TICKS(4));
  if (uart_flush_input(BUS_UART) != ESP_OK)
    return -3;
  xQueueReset(uartEvents);
  if (uart_write_bytes(BUS_UART, frame, sizeof(frame)) != sizeof(frame) ||
      uart_wait_tx_done(BUS_UART, pdMS_TO_TICKS(100)) != ESP_OK)
    return -3;

  vTaskDelay(pdMS_TO_TICKS(200));
  for (unsigned attempt = 0; attempt < 3; ++attempt)
  {
    int result = ProbeNode(address, &probe, nullptr);
    if (result == 1)
    {
      for (size_t i = 0; i < slot; ++i)
      {
        if (constant_equal(registeredNodes[i].uid, probe.uid,
                           sizeof(probe.uid)))
        {
          memcpy(registeredNodes, previousNodes,
                 registeredNodeCount * sizeof(previousNodes[0]));
          registeredNodeCount = slot + 1;
          return -3;
        }
      }
      const size_t assignedPreviousCount = registeredNodeCount;
      memcpy(previousNodes, registeredNodes,
             assignedPreviousCount * sizeof(previousNodes[0]));
      registeredNodes[slot] = probe;
      if (!PersistRegistry(previousNodes, assignedPreviousCount))
      {
        memcpy(registeredNodes, previousNodes,
               assignedPreviousCount * sizeof(previousNodes[0]));
        registeredNodeCount = assignedPreviousCount;
        return -5;
      }
      if (assigned)
        *assigned = probe;
      return 1;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  return -3;
}

void ModbusGetNodes(std::vector<ModbusNodeInfo> &nodes)
{
  std::lock_guard<std::mutex> lock(busMutex);
  nodes.assign(registeredNodes, registeredNodes + registeredNodeCount);
}

bool ModbusReadNode(const ModbusNodeInfo &node, uint16_t registers[12])
{
  if (!registers)
    return false;
  std::lock_guard<std::mutex> lock(busMutex);
  const uint8_t request[4] = {0, 0, 0, 12};
  uint8_t response[FRAME_MAX];
  int length = BusExchange(node.address, 3, request, sizeof(request), response,
                           sizeof(response), 150, nullptr);
  if (length != 29 || response[2] != 24)
    return false;

  for (size_t i = 0; i < 12; ++i)
    registers[i] = static_cast<uint16_t>(response[3 + 2 * i] << 8) |
                   response[4 + 2 * i];
  for (size_t i = 0; i < 6; ++i)
  {
    uint16_t uidWord = static_cast<uint16_t>(node.uid[2 * i] << 8) |
                       node.uid[2 * i + 1];
    if (registers[6 + i] != uidWord)
      return false;
  }
  return registers[5] == node.address;
}

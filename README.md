# ESP32-C3 MQTT gateway with RS485 Modbus RTU

This ESP-IDF application keeps the existing gateway/cloud MQTT protocol and adds a Modbus RTU master for STM32 nodes over RS485. The ESP32 target is **ESP32-C3**. BLE device communication and UDP discovery/provisioning are not included in this build. Wi-Fi remains the network transport for MQTT; Wi-Fi credentials must already be available in NVS.

## Features

- Reconnects the existing gateway MQTT client over Wi-Fi.
- Scans Modbus addresses 1–247 at startup and polls registered nodes every five seconds.
- Accepts MQTT RPC commands `scan`, `discover` and `assign_address`.
- Persists an address reservation before the single broadcast FC06 assignment, then stores the UID after read-back confirmation.
- Publishes node status and FC03 register data using the gateway's existing `deviceUpdate` MQTT command.
- Reports ADC raw values and millivolts; these are not a temperature/humidity driver.

The gateway does **not** use the standalone `main` branch's `iot/{gateway}/commands` topics, `job_id` command format, queueing, or remote STM32 OTA integration. See [docs/PROTOCOL.md](./docs/PROTOCOL.md) for the current MQTT RPC and Modbus details.

## Hardware

Use a 3.3 V RS485 transceiver (for example, MAX3485); do not connect a 5 V receiver output directly to the ESP32-C3. Connect DE and /RE together.

| ESP32-C3 | RS485 transceiver |
|---|---|
| GPIO4 | DI |
| GPIO5 | RO |
| GPIO6 | DE + /RE |
| 3V3, GND | VCC, GND |

The default ESP32-C3 button pin is GPIO9 and the RTC I2C pins are GPIO8 (SDA) and GPIO10 (SCL). Verify all GPIO assignments against the exact board schematic. In particular, GPIO4/5/6 may conflict with external JTAG on some modules.

Use a single RS485 trunk with short stubs, connect A-to-A/B-to-B as specified by the transceiver vendor, and provide a suitable common reference. Fit 120 Ω termination only at the two physical ends and use biasing at one location. For long or electrically noisy installations, add suitable isolation and surge protection.

The bus uses **38400 baud, 8N1**. The STM32 node firmware must implement the register map and broadcast address assignment documented in [docs/PROTOCOL.md](./docs/PROTOCOL.md).

## Wi-Fi and MQTT configuration

The firmware starts in station mode and reads Wi-Fi/MQTT settings from the existing NVS-backed `Config` service. BLE and UDP provisioning have been removed from this target; configure credentials through an external provisioning process before boot. The factory-reset button sequence clears saved network configuration, so credentials must be provisioned again afterward.

Relevant existing NVS settings include `wifi_ssid`, `wifi_pass`, `host`, `port`, `client_id`, `username`, `password`, and `keep_alive`. The MQTT request/response topics are derived from the gateway MAC and follow the existing `/v3/server/hc/{mac}/jsonreq` and `/v3/hc/{mac}/server/jsonres` contract.

## Build and flash

Install/export the ESP-IDF version supported by this project, then run from `esp32`:

```sh
idf.py set-target esp32c3
idf.py build
idf.py -p COMx flash monitor
```

On Linux/macOS, replace `COMx` with the serial device. The project CMake configuration selects ESP32-C3 and rejects other targets. Run `set-target` once when converting an existing ESP-IDF build directory or `sdkconfig` from another target.

## Commissioning nodes

Only power **one unconfigured node at a time** while assigning an address. Configured nodes can remain connected. A broadcast cannot distinguish two unconfigured nodes, and both could store the same address.

1. Start the gateway and allow its initial read-only scan to complete.
2. Power one unconfigured node.
3. Send an `assign_address` RPC with an integer address from 1 to 247:

   ```json
   {"cmd":"assign_address","rqi":"request-id","data":{"address":5}}
   ```

4. Wait for a successful RPC response, including the confirmed UID. The gateway checks the registry and current bus occupancy, persists an address reservation, sends one FC06 broadcast, then reads the assigned node back.
5. If the result is uncertain, do not assign a different node to the reserved address. Power the same node and run read-only `scan`/`discover` first.

Use `scan` or `discover` to enumerate currently responding nodes; neither command assigns addresses. Modbus nodes and pending address reservations are stored in the existing SQLite `Device` table and retained across reboots, including offline nodes. Do not delete those rows unless all physical nodes and their configured addresses have been reconciled.

RPC commands follow the gateway's existing JSON contract: requests contain `cmd`, `rqi`, and object-valued `data`; the response method is `discoverRsp` or `assign_addressRsp`. `assign_address` takes `data: {"address": 5}`. It does not use the standalone firmware's `op`, `job_id`, or `address` top-level command schema.

## Telemetry

For a node with a known UID, the gateway publishes a `deviceUpdate` with device ID `rs485-{uid}`. Pending address reservations without a confirmed UID use `rs485-pending-{address}`. Data includes address, UID, version, mode, online status (`stt`), quality, ADC raw count, and millivolts when online.

Telemetry is not buffered while MQTT is disconnected; a later poll publishes the latest available node state after reconnection. Remote STM32 OTA is not connected to the current gateway RPC flow.

## Safety notes

- The gateway is the sole Modbus master on the bus.
- Address assignment is broadcast and has no Modbus response. The gateway reserves the address before sending and deliberately does not retry an uncertain broadcast.
- Offline registry entries are retained to avoid accidental address reuse.
- Validate wiring, transceiver direction control, node register values, and MQTT RPC responses on a bench setup before field deployment.

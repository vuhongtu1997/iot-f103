# ESP32-C3 MQTT gateway with RS485 Modbus RTU

This ESP-IDF application keeps the existing gateway/cloud MQTT protocol and adds a Modbus RTU master for STM32 nodes over RS485. The ESP32 target is **ESP32-C3**. BLE device communication, UDP discovery/provisioning, Ethernet, and an external RTC are not included in this build. Wi-Fi is the only network transport, and SNTP over Wi-Fi is the only time source. Wi-Fi credentials must already be available in NVS.

## Features

- Reconnects the existing gateway MQTT client over Wi-Fi.
- Scans Modbus addresses 1–247 at startup and polls registered nodes every five seconds.
- Accepts MQTT RPC commands `scan`, `discover` and `assign_address`.
- Persists an address reservation before the single broadcast FC06 assignment, then stores the UID after read-back confirmation.
- Reads the node's FC03 sensor type after address assignment and persists a typed sensor child (gas, temperature/humidity, or CH3) alongside its Modbus node in SQLite.
- Publishes the node and recognized child status through the gateway's existing `deviceUpdate` MQTT command.
- Reports ADC raw values and millivolts for gas/CH3 nodes. Temperature/humidity is identified, but needs a matching sensor driver before measured values can be reported.

The gateway does **not** use the standalone `main` branch's `iot/{gateway}/commands` topics, `job_id` command format, queueing, or remote STM32 OTA integration. See [docs/PROTOCOL.md](./docs/PROTOCOL.md) for the current MQTT RPC and Modbus details.

## Hardware

Use one 3.3 V RS485 transceiver (for example, MAX3485) at each processor. Do not connect the ESP32 and STM32 UART pins directly, and do not connect a 5 V receiver output to either processor. On both transceivers, connect `DE` and `/RE` together for half-duplex direction control.

### ESP32-C3 to RS485 transceiver

| ESP32-C3 | Transceiver pin | Function |
|---|---|---|
| GPIO4 (UART1 TX) | DI | Data sent by the ESP32 |
| GPIO5 (UART1 RX) | RO | Data received by the ESP32 |
| GPIO6 | DE + /RE | Transmit/receive direction |
| 3V3 | VCC | 3.3 V supply |
| GND | GND | Common reference |

### STM32F103C8T6 to RS485 transceiver

| STM32F103C8T6 | Transceiver pin | Function |
|---|---|---|
| PA9 (USART1 TX) | DI | Data sent by the STM32 |
| PA10 (USART1 RX) | RO | Data received by the STM32 |
| PA8 | DE + /RE | Transmit/receive direction |
| 3.3V | VCC | 3.3 V supply |
| GND | GND | Common reference |

### RS485 bus between ESP32 and STM32

```text
ESP32-C3                 MAX3485 #1        RS485 bus        MAX3485 #2                 STM32F103
GPIO4  (TX) ------------ DI                                      DI ---------------- PA9  (TX)
GPIO5  (RX) <----------- RO                                      RO ---------------> PA10 (RX)
GPIO6       ------------ DE + /RE                        DE + /RE ------------------- PA8
3V3         ------------ VCC                                     VCC ---------------- 3.3V
GND         ------------ GND -------- GND/common reference ------ GND ---------------- GND
                                      A -------------------------- A
                                      B -------------------------- B
```

Connect `A` to `A`, `B` to `B`, and connect the grounds/common reference. Some transceiver modules label the differential pair in reverse; if there is no communication, check the module datasheet before swapping A and B. For multiple STM32 nodes, connect every node in parallel on the same A/B trunk and give each node a unique Modbus address.

### STM32 factory-reset button

Connect a normally-open push button between `PB12` and `GND`. The firmware configures PB12 with its internal pull-up resistor, so no external pull-up is required.

```text
STM32 PB12 ---- push button ---- GND
```

Hold the button continuously for at least 5 seconds. The STM32 erases its saved Modbus address and restarts as an unconfigured node with address `0`. A short press has no effect. This reset does not erase the bootloader, application firmware, OTA key, or the compile-time `SENSOR_TYPE`. After reset, power only this unconfigured node while assigning it a new address.

### ESP32-C3 configuration button

Connect a normally-open push button between `GPIO9` and `GND`. The input is active-low and the firmware enables its internal pull-up resistor.

```text
ESP32-C3 GPIO9 ---- push button ---- GND
```

- Press and release 5 times within 10 seconds to perform a factory reset. The gateway flashes its status LEDs, clears the saved network configuration and local database, then restarts.
- If fewer than 5 clicks are completed in 10 seconds, the sequence is cancelled and both status LEDs blink twice.

GPIO9 is a strapping pin on ESP32-C3. Do not hold the button while applying power or resetting the module unless bootloader/download mode is intended. Verify the strapping requirements for the exact ESP32-C3 board.

The ESP32 build does not use an external RTC or Ethernet PHY. GPIO8/GPIO10 are no longer reserved for DS1307 I2C, and Ethernet MDC/MDIO/reset/clock pins are no longer configured. Time-dependent rules become available only after SNTP synchronization over Wi-Fi.

Verify all GPIO assignments against the exact board schematic. In particular, GPIO4/5/6 may conflict with external JTAG on some modules.

Use a single RS485 trunk with short stubs. Fit 120 Ω termination only at the two physical ends and use biasing at one location. Do not fit a 120 Ω resistor at every STM32 node. For long or electrically noisy installations, add suitable isolation and surge protection.

The bus uses **38400 baud, 8N1**. The STM32 node firmware must implement the register map and broadcast address assignment documented in [docs/PROTOCOL.md](./docs/PROTOCOL.md).

Build each STM32 node image for its sensor family by setting `SENSOR_TYPE`: `1` for gas, `2` for temperature/humidity, or `3` for CH3. The default `0` reports an unknown type and does not create a sensor child:

```sh
make clean
make SENSOR_TYPE=1
```

The generated sensor type is returned in FC03 register 12. The ESP32 remains compatible with older nodes that expose only registers 0–11; those nodes are detected as unknown.

## Wi-Fi and MQTT configuration

The firmware starts in station mode and reads Wi-Fi/MQTT settings from the existing NVS-backed `Config` service. BLE and UDP provisioning have been removed from this target; configure credentials through an external provisioning process before boot. The factory-reset button sequence clears saved network configuration, so credentials must be provisioned again afterward.

Relevant existing NVS settings include `wifi_ssid`, `wifi_pass`, `host`, `port`, `client_id`, `username`, `password`, and `keep_alive`. The MQTT request/response topics are derived from the gateway MAC and follow the existing `/v3/server/hc/{mac}/jsonreq` and `/v3/hc/{mac}/server/jsonres` contract. Complete JSON request, response, error, and telemetry examples are documented in [docs/MQTT.md](./docs/MQTT.md).

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

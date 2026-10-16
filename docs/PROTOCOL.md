# Wire protocol v1

UART 38400 8N1. Frame RTU: address[1], function[1], payload[N], CRC16[2] low byte first. Max 256 bytes. Custom integers LE; FC03 integers BE. Master waits for complete response before next transaction. Node recognizes silence >=3 ms between frames. UART errors drop the frame.

## Address commissioning: broadcast 0, FC06

Modbus Write Single Register frame: `00 06 01 00 00 ID CRClo CRChi`. Register address `0x0100` (zero-based wire offset), value 1..247. Both register and value are big endian. **No broadcast response**, including errors.

Only a node whose persistent configuration is absent/invalid accepts this write. Nodes already configured ignore it. Every other broadcast is ignored. The behavior is identical in application and recovery bootloader. This is a device-specific register implemented with a standard Modbus broadcast write; it is not a general standard register for every third-party sensor.

Exactly ONE unconfigured node must have power during assignment. Configured nodes may remain powered. Two unconfigured nodes would both accept the same ID; firmware cannot distinguish them from this request. Rebooting a configured node does not make it unconfigured; ID persists in flash.

Gateway checks ID against its SQLite `Device` registry and probes for a live occupant three times. It durably reserves the ID before broadcasting once, waits 200 ms, then reads FC03 at the new address to confirm UID/version/address. No automatic broadcast retry. An uncertain result retains the reservation. Read-only discovery can adopt its UID later. Offline IDs are not removed from the registry. A conflict between a known UID and the responding UID makes discovery fail. Registry changes are committed transactionally; persistence failure prevents the address broadcast or leaves the previous reservation intact.

In this ESP32-C3 gateway app, MQTT requests use the existing gateway RPC contract on `/v3/server/hc/{mac}/jsonreq`, with `cmd`, `rqi`, and object-valued `data`. Use `cmd: "scan"` or `"discover"` with empty data for read-only discovery, and `cmd: "assign_address"` with `data: {"address": 5}` to assign. Responses use `discoverRsp` or `assign_addressRsp` on `/v3/hc/{mac}/server/jsonres`. `assign_addressRsp.params.code` is `0` only after the new address and UID have been read back and persisted; `params.result` contains the bus-level result. Unlike the standalone gateway in the `main` branch, this app does not use `iot/{gateway}/commands` or the `job_id` deduplication API.

After assignment and during discovery, the gateway reads FC03 register 12 to identify the node's sensor family. A recognized family creates a child device in SQLite with ID `rs485-{uid}-{gas|temperature-humidity|ch3}` and type `MODBUS_GAS`, `MODBUS_TEMPERATURE_HUMIDITY`, or `MODBUS_CH3`. The `assign_addressRsp` includes `sensor_type`, `sensor_type_name`, and `child_device` when recognized. Discovery includes the same type and child ID. Unknown or legacy nodes still register as `MODBUS_NODE` without a child.

The gateway scans once after startup, then polls registered nodes every five seconds. Telemetry is published through the existing `deviceUpdate` MQTT command for the parent ID `rs485-{uid}` (or `rs485-pending-{address}` for an unconfirmed reservation) and, when recognized, its sensor child. Parent data includes `address`, `uid`, `version`, `mode`, `sensor_type`, `sensor_type_name`, `stt`, `quality`, `adc_raw`, and `millivolts`. Gas and CH3 child samples include `raw` and `millivolts`; temperature/humidity children currently report identification and online state only (`quality: "measurement_unavailable"`), because this STM32 firmware has no temperature/humidity sensor driver. MQTT-disconnected samples are not buffered; the next poll reports the latest node state after reconnection. Remote STM32 OTA is not wired into this gateway RPC integration.

`scan` and `discover` only read assigned IDs; neither grants new addresses. No prefix discovery or special address-247 multicast is implemented. To return a configured node to its initial unconfigured state, hold its PB12-to-GND factory-reset button for at least 5 seconds. The node invalidates and erases both address configuration pages, then restarts with address 0. Firmware and compile-time sensor type are retained. No remote reset command is implemented.

On ESP32-C3 the gateway uses UART1 at 38400 8N1 with TX=GPIO4, RX=GPIO5, and DE/RE=GPIO6. These GPIOs are shared with external JTAG on common C3 modules. This app build disables the old BLE device protocol, Ethernet, and external DS1307 RTC. Wi-Fi is the network transport and SNTP is the time source. UDP discovery and Wi-Fi provisioning use port 8181 and a fallback setup AP; see [UDP_WIFI.md](./UDP_WIFI.md) for requests and setup instructions.

## FC03 registers (zero-based offsets)

| Offset | Content |
|---|---|
| 0 | ADC PA0 raw 0..4095, 0xffff=error/unavailable |
| 1 | ADC mV assuming 3300mV VDDA, 0xffff=unavailable |
| 2,3 | Running firmware uint32, high then low register |
| 4 | 0=application, 1=recovery bootloader |
| 5 | Current address |
| 6..11 | UID bytes, two bytes per register, first byte high |
| 12 | Sensor family: 0 unknown, 1 gas, 2 temperature/humidity, 3 CH3 |

Read request start[2] and count[2] BE; count >0, start+count <=13. Response byte_count[1], registers[2*count]. Error function |=0x80, exception 02 for out-of-range. FC06 register 0x0100 is write-only and accepted only as commissioning broadcast on an unconfigured node. It is outside this FC03 readable range. Older node firmware may expose only registers 0–11; the gateway treats the optional sensor type read as unknown and continues to support the node.

STM32 builds select the reported sensor family with `make SENSOR_TYPE=1` (gas), `2` (temperature/humidity), or `3` (CH3). The default is 0 (unknown). The type is a firmware configuration for a node variant; it is not inferred from the analog sample. Gas and CH3 currently share the node's ADC raw/millivolt measurements. Temperature/humidity register values are not defined until a corresponding STM32 sensor driver is added.

## OTA: assigned address, function 0x42

Response for all OTA operations: op[1], success[1], received_bytes[4]. No automatic retry for explicit success=0; up to 3 retries when ACK absent/invalid. Duplicate exact chunks below received offset are acknowledged after comparison.

| Op | Request payload after op | Purpose |
|---|---|---|
| 1 | descriptor[16] | Erase W25 metadata + staging sectors; begin session |
| 2 | offset[4], length[1], data[length] | Sequential data, length 1..128 |
| 3 | none | Verify CRC32/vectors, write READY marker, ACK then reset |
| 4 | none | Return RAM receive offset |

Descriptor: model[4], size[4], CRC32[4], version[4], all LE. STM32 model=0x103c8, ESP32 model=0x32. CRC32 is reflected IEEE polynomial 0xedb88320, initial/final XOR 0xffffffff.

W25: descriptor at 0, READY `0xa55a5aa5` at 16, consumed at 20 (initial erased `0xffffffff`; zero after successful install), image at `0x1000`. READY is written last. Bootloader revalidates the staged image before erase, writes the internal vector after all other bytes and CRC verification, then marks the image consumed. No address 0 response, no broadcast OTA, no bootloader OTA.

Application size 8..47104 bytes; initial MSP aligned to 8 bytes in 0x20000000..0x20005000 and Thumb reset vector inside actual application image. A wrong build address fails validation. Flash writes pad a final odd byte with 0xff; the padding is excluded from CRC32.

OTA no longer uses a shared key or cryptographic signature. CRC32 detects accidental corruption but cannot prove who produced the firmware. Restrict OTA commands and firmware downloads using MQTT ACL/TLS and HTTPS access control.

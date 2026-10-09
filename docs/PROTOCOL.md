# Wire protocol v1

UART 38400 8N1. Frame RTU: address[1], function[1], payload[N], CRC16[2] low byte first. Max 256 bytes. Custom integers LE; FC03 integers BE. Master waits for complete response before next transaction. Node recognizes silence >=3 ms between frames. UART errors drop the frame.

## Address commissioning: broadcast 0, FC06

Modbus Write Single Register frame: `00 06 01 00 00 ID CRClo CRChi`. Register address `0x0100` (zero-based wire offset), value 1..247. Both register and value are big endian. **No broadcast response**, including errors.

Only a node whose persistent configuration is absent/invalid accepts this write. Nodes already configured ignore it. Every other broadcast is ignored. The behavior is identical in application and recovery bootloader. This is a device-specific register implemented with a standard Modbus broadcast write; it is not a general standard register for every third-party sensor.

Exactly ONE unconfigured node must have power during assignment. Configured nodes may remain powered. Two unconfigured nodes would both accept the same ID; firmware cannot distinguish them from this request. Rebooting a configured node does not make it unconfigured; ID persists in flash.

Gateway checks ID against NVS registry and probes for a live occupant three times. It durably reserves the ID before broadcasting once, waits 200 ms, then reads FC03 at the new address to confirm UID/version/address. No automatic broadcast retry. Uncertain result retains the reservation. Read-only `scan` can adopt its UID later. Offline IDs are not removed from the registry. A conflict between known UID and the responding UID makes scan fail. Persistence failure prevents broadcast; failure after confirmation may leave the reservation in NVS pending until next scan.

MQTT command: `{"op":"assign_address","job_id":"unique-id","address":5}`. App waits for `completed` and the node event on `iot/{gateway}/ota/status`. `scan` / `discover` only read assigned IDs; neither grants new addresses. No prefix discovery, no special address 247 multicast. To set a configured node again, clear its two configuration pages locally; no remote reset command is implemented.

## FC03 registers (zero-based offsets)

| Offset | Content |
|---|---|
| 0 | ADC PA0 raw 0..4095, 0xffff=error/unavailable |
| 1 | ADC mV assuming 3300mV VDDA, 0xffff=unavailable |
| 2,3 | Running firmware uint32, high then low register |
| 4 | 0=application, 1=recovery bootloader |
| 5 | Current address |
| 6..11 | UID bytes, two bytes per register, first byte high |

Read request start[2] and count[2] BE; count >0, start+count <=12. Response byte_count[1], registers[2*count]. Error function |=0x80, exception 02 for out-of-range. FC06 register 0x0100 is write-only and accepted only as commissioning broadcast on an unconfigured node. It is outside this FC03 readable range.

## OTA: assigned address, function 0x42

Response for all OTA operations: op[1], success[1], received_bytes[4]. No automatic retry for explicit success=0; up to 3 retries when ACK absent/invalid. Duplicate exact chunks below received offset are acknowledged after comparison.

| Op | Request payload after op | Purpose |
|---|---|---|
| 1 | descriptor[16], hmac[32] | Erase W25 metadata + staging sectors; begin session |
| 2 | offset[4], length[1], data[length] | Sequential data, length 1..128 |
| 3 | none | Verify HMAC/CRC/vectors, write READY marker, ACK then reset |
| 4 | none | Return RAM receive offset |

Descriptor: model[4], size[4], CRC32[4], version[4], all LE. STM32 model=0x103c8, ESP32 model=0x32. CRC32 is reflected IEEE polynomial 0xedb88320, initial/final XOR 0xffffffff. HMAC-SHA256(key, descriptor || exact image bytes); no padding in authenticated message. Authentication is shared-secret HMAC, not an asymmetric signature.

W25: descriptor at 0, MAC at 16, READY 0xa55a5aa5 at 48, consumed at 52 (initial erased 0xffffffff; zero after successful install), image at 0x1000. READY is written last. Bootloader revalidates the entire staged image before erase, writes internal vector after all other bytes and CRC verification, then marks consumed. No address 0 response, no broadcast OTA, no bootloader OTA.

Application size 8..47104 bytes; initial MSP aligned to 8 bytes in 0x20000000..0x20005000 and Thumb reset vector inside actual application image. A wrong build address fails validation. Flash writes pad a final odd byte with 0xff; the padding is excluded from HMAC/CRC.

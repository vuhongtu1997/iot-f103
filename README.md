# ESP32 + STM32F103C8T6: Wi-Fi/MQTT, cấp địa chỉ từ app qua broadcast và OTA

Bộ nguồn tham chiếu cho **ESP32-WROOM-32 có 4 MB flash**, **STM32F103C8T6 thật có 64 KB flash** và **W25Q32JV (3,3 V) trên mỗi nút**. ESP32 dùng ESP-IDF **v5.2.3**; STM32 dùng C bare metal, GNU Arm Embedded Toolchain và Make. Không phụ thuộc CubeMX/HAL hoặc Arduino. Xem `docs/VALIDATION.md` để biết chính xác những gì đã kiểm tra; không có xác nhận thử trên phần cứng.

## Chức năng

- ESP32 tự kết nối lại Wi-Fi và MQTT qua TLS, LWT, QoS 1.
- App chọn ID; ESP32 gửi Modbus broadcast FC06. Chỉ nút chưa cấu hình nhận ID, rồi ESP32 đọc lại để xác nhận.
- Registry địa chỉ/UID và địa chỉ chờ xác nhận được lưu trong NVS, giữ cả nút đang offline.
- STM32 lưu địa chỉ bằng hai trang flash, ghi dấu hợp lệ sau cùng; OTA giữ nguyên địa chỉ.
- Đọc Modbus RTU FC03; gửi giá trị ADC PA0 và điện áp quy đổi lên cloud. **Chưa phải driver nhiệt độ/độ ẩm cụ thể**.
- MQTT nhận lệnh OTA; ESP32 tải HTTPS, kiểm tra kích thước, CRC32 và HMAC-SHA256.
- ESP32 dùng hai phân vùng OTA và rollback nếu bản mới không kết nối MQTT trong 180 giây.
- STM32 nhận firmware qua RS485, xác thực lại, lưu W25Q32, khởi động bootloader để nạp ứng dụng. Mất điện khi nạp: bắt đầu nạp lại từ bản đã lưu. Vector ứng dụng được ghi cuối cùng để không chạy bản nạp dở.

## Đấu nối

Hai đầu RS485 đều dùng transceiver **logic 3,3 V**, ví dụ MAX3485. Không nối tín hiệu RO 5 V trực tiếp vào MCU. DE và /RE nối chung để TX bật khi mức cao, RX bật khi mức thấp.

| ESP32 | Transceiver |
|---|---|
| GPIO17 | DI |
| GPIO16 | RO |
| GPIO4 | DE + /RE |
| 3V3 / GND | VCC / GND |

| STM32F103C8T6 | Thiết bị |
|---|---|
| PA9 / PA10 / PA8 | RS485 DI / RO / DE + /RE |
| PA5 / PA6 / PA7 | W25Q32 CLK / DO / DI |
| PA4 | W25Q32 /CS |
| 3V3 | W25Q32 VCC, /WP, /HOLD |
| GND | W25Q32 GND |
| PA0 | Đầu vào cảm biến analog 0–3,3 V |
| PA13 / PA14 | ST-LINK SWDIO / SWCLK |

Nối A–A, B–B và đường tham chiếu GND phù hợp. Bus đường trục, nhánh ngắn, điện trở 120 Ω chỉ ở hai đầu; chọn bias một vị trí. Dùng tụ decoupling gần từng IC. Với môi trường công nghiệp, bổ sung cách ly và bảo vệ phù hợp. Tên A/B giữa các hãng có thể khác, kiểm tra datasheet. Bản này dùng USART1 **38400, 8N1** ở cả hai đầu; đây là cấu hình được chọn đồng bộ, không phải mặc định 8E1 thường gặp của Modbus.

STM32 chạy **HSI 8 MHz**, không cần thạch anh ngoài. ADC quy đổi theo giả định VDDA=3300 mV; đo/calibrate VDDA nếu cần chính xác.

## Chuẩn bị khóa và cấu hình

Ở thư mục gốc:

```sh
python3 tools/keygen.py
cp esp32/main/secrets.example.h esp32/main/secrets.h
```

Điền SSID, mật khẩu, URI `mqtts://...`, thông tin xác thực MQTT, `GATEWAY_ID` và `OTA_ORIGIN`. Chứng chỉ công khai dùng certificate bundle; với CA riêng, điền PEM vào `CLOUD_CA_PEM`. Khi broker và kho firmware dùng CA khác nhau, tách cấu hình CA trong `main.c` và `ota.c`.

Khóa `common/ota_key.h` dùng chung khi build bootloader, ứng dụng STM32 và ESP32; `tools/ota-key.bin` dùng phía phát hành firmware. Giữ bản sao riêng tư. Không dùng khóa công khai trong `ota_key.example.h` để triển khai. HMAC dùng khóa chia sẻ, **không phải chữ ký bất đối xứng**: lộ khóa một thiết bị có thể ảnh hưởng cả nhóm dùng chung khóa. Bootloader hiện không có chống hạ phiên bản hoặc Secure Boot phần cứng.

## Build và nạp lần đầu

STM32: cài `arm-none-eabi-gcc`, newlib, Make và OpenOCD, nối ST-LINK, BOOT0 kéo xuống thấp:

```sh
cd stm32
make VERSION=1
make flash
```

`make flash` nạp bootloader ở `0x08000000` và ứng dụng ở `0x08004000`. Không dùng mass erase khi muốn giữ cấu hình địa chỉ. Tạo file ELF/BIN bằng toolchain đầy đủ, không nạp ứng dụng đơn lẻ tại `0x08000000`.

| Vùng flash STM32 | Địa chỉ | Dung lượng |
|---|---|---|
| Bootloader | `0x08000000–0x08003FFF` | 16 KB |
| Ứng dụng | `0x08004000–0x0800F7FF` | 46 KB |
| Cấu hình A/B | `0x0800F800–0x0800FFFF` | 2 KB |

ESP32: cài/export ESP-IDF v5.2.3:

```sh
cd esp32
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Windows: thay cổng bằng `COMx`. Bảng phân vùng đã có trong `partitions.csv`. Không đổi bảng phân vùng bằng OTA. Build lỗi thiếu secrets/ota_key: thực hiện bước chuẩn bị trước. Phiên bản ESP32 mặc định của CMake project có thể đặt khi build bằng `idf.py -DPROJECT_VER=2.0.0 build`.

## Cấp địa chỉ từ app, cấp nguồn lần lượt

**Mỗi lần chỉ cấp nguồn cho MỘT nút chưa được cấu hình.** Những nút đã có địa chỉ có thể giữ nguồn vì firmware bỏ qua lệnh gán broadcast. Nút mới có trạng thái nội bộ UNCONFIGURED, không trả lời tại địa chỉ 0; 0 chỉ là địa chỉ broadcast trên wire.

Quy trình:

1. Bật ESP32, chờ kết nối MQTT. ESP32 quét đọc các địa chỉ đã cấu hình; không tự gán ID.
2. Cấp nguồn nút mới và chờ boot xong (khoảng 1 giây khi không có firmware chờ nạp).
3. Trong app chọn ID chưa dùng, từ 1 đến 247; gửi lệnh sau, QoS1, retain=false:

```json
{"op":"assign_address","job_id":"set-node-001","address":5}
```

4. ESP32 kiểm tra registry và đọc thử địa chỉ 5. ID đã dùng/được giữ chỗ hoặc bus có phản hồi lỗi thì từ chối.
5. ESP32 lưu giữ chỗ ID vào NVS trước khi phát **một** broadcast FC06 ghi thanh ghi `0x0100`, giá trị 5. Không chờ ACK broadcast.
6. Chỉ STM32 chưa có địa chỉ lưu ID vào flash. Các nút đã có ID không đổi.
7. ESP32 chờ 200 ms rồi đọc FC03 ở ID 5, nhận UID và phiên bản, lưu registry, báo `completed` và sự kiện `node` lên `ota/status`.
8. Khi app nhận completed, giữ nguồn nút đã cấu hình hoặc tắt nó; cấp nguồn nút mới tiếp theo và chọn ID khác.

App nối MQTT cloud và publish tới `iot/{gateway_id}/commands`; gateway nối bus tại chỗ. App này có thể là app hiện có của bạn; bộ nguồn cung cấp contract MQTT và script gửi lệnh, **không có giao diện mobile/web app riêng**. Gán ID khác với UID phần cứng: UID vẫn dùng để nhận diện thiết bị và chọn đích OTA.

Nếu broadcast đã phát nhưng không đọc được xác nhận, ESP32 giữ ID trong trạng thái pending (mode=2, UID tạm toàn 0). Không phát lại broadcast tự động: nếu nút vừa nhận ID rồi bị mất nguồn, một nút mới khác có thể nhận cùng ID khi broadcast phát lại. Cấp lại nguồn đúng nút vừa cấu hình và gửi:

```json
{"op":"scan","job_id":"scan-002"}
```

Scan chỉ đọc địa chỉ 1–247, không gán, không đổi ID, cập nhật UID cho giữ chỗ pending khi xác nhận được. Registry offline vẫn giữ. `discover` là alias đọc-only của `scan`, không còn tự gán. Scan có thể mất khoảng 15–20 giây khi ít nút có nguồn. Lịch sử job chống trùng vẫn áp dụng: dùng job_id mới cho lệnh scan mới.

Pending ID chưa có thiết bị phải được kiểm tra tại chỗ; bản này không có lệnh tự giải phóng giữ chỗ hoặc factory reset từ xa. Khi cần cấu hình lại ID đã lưu trên STM32, dùng ST-LINK xóa **hai trang cấu hình** `0x0800F800–0x0800FFFF`, giữ nguyên bootloader/app, rồi cấp nguồn và gán lại. Chỉ xóa registry NVS ESP32 khi đã đối chiếu danh sách toàn bộ thiết bị; xóa mù có thể tái sử dụng ID của nút đang tắt nguồn.

**Nếu bật đồng thời hai nút chưa cấu hình, cả hai có thể lưu cùng ID.** Phương pháp này không có cách xác định chắc chắn chỉ một nút đang nghe broadcast; quy trình cấp nguồn lần lượt là điều kiện bắt buộc. ESP32 có thể thấy lỗi CRC khi đọc lại, nhưng không dựa vào CRC để đảm bảo phát hiện mọi ID trùng. Một master duy nhất trên bus.

Bản này thay giao thức cấu hình prefix UID tại địa chỉ 247 của bản trước bằng broadcast address 0/FC06. Địa chỉ 247 hiện dùng được cho cảm biến. Nạp bootloader và app STM32 mới bằng ST-LINK khi chuyển sang bản này; không dùng firmware cấu hình cũ chung với quy trình mới.

## MQTT và cloud

| Topic | Hướng | Nội dung |
|---|---|---|
| `iot/gw-001/telemetry` | ESP32 → cloud | UID, address, boot, sequence, timestamp, ADC, quality |
| `iot/gw-001/status` | ESP32 → cloud | Online/LWT và phiên bản gateway, retained |
| `iot/gw-001/commands` | Cloud → ESP32 | `assign_address`, `scan`, `ota`, **không retained** |
| `iot/gw-001/ota/status` | ESP32 → cloud | Tiến trình, kết quả và danh sách UID |

Gán ACL broker: gateway chỉ subscribe commands của mình và publish telemetry/status của mình; chỉ tài khoản quản trị được publish commands. Mỗi gateway có ID và thông tin xác thực riêng. Code mẫu dùng username/password; cloud bắt buộc mTLS, token đặc thù hay topic khác cần chỉnh phần cấu hình client tương ứng.

Xem UID qua `ota/status` sau lệnh scan. Registry khi scan khởi động có thể chưa gửi vì MQTT chưa kết nối; gửi lệnh scan để lấy danh sách trên cloud. Telemetry cũng chứa UID. Backend loại trùng bằng `(gateway_id, boot, sequence)`.

Hàng đợi telemetry RAM giữ 64 mẫu; đầy sẽ bỏ mẫu cũ và tăng `dropped_samples`. MQTT outbox giới hạn 32 KB. Hàng đợi RAM/outbox mất sau khởi động lại, không cam kết lưu dữ liệu offline lâu dài. Chỉ có timestamp thực sau đồng bộ NTP. MQTT TLS chờ đồng bộ thời gian; mạng phải cho truy cập NTP hoặc đổi server NTP trong `main.c`.

Lệnh cần `job_id` duy nhất. Tám ID gần nhất lưu NVS để chặn QoS1 phát lại. Retry tác vụ lỗi phải dùng ID mới; lịch sử này không phải chống replay vĩnh viễn. Mỗi gateway xử lý tuần tự; trong OTA, polling toàn bus tạm dừng. Topic OTA trạng thái là non-retained và có thể không tới cloud nếu mất mạng; xem log serial để chẩn đoán.

## Phát hành OTA STM32

1. Đổi phiên bản khi build; `make clean` để không tái dùng binary cũ.
2. Đưa **app.bin**, không phải boot.bin, lên HTTPS server thuộc `OTA_ORIGIN`.
3. Tạo JSON có HMAC, dùng đúng UID cảm biến.
4. Publish JSON lên commands, QoS1, retain=false; theo dõi OTA status.

```sh
cd stm32
make clean
make VERSION=2
cd ..
python3 tools/package_firmware.py stm32/build/app.bin --target stm32 --version 2 --uid 00112233445566778899aabb --url https://firmware.example.com/stm32/app-v2.bin --output node-ota.json
```

Thay UID/URL bằng giá trị thật. Server trả HTTP 200 và Content-Length chính xác, không redirect/chunked. URL khác origin bị từ chối. STM32 chỉ báo `completed` khi ESP32 đọc đúng UID, chế độ ứng dụng và `FW_VERSION` bằng version trong lệnh. Bootloader hiện không OTA được; đổi khóa/bootloader cần ST-LINK.

## Phát hành OTA ESP32

Build ứng dụng ESP32 mới, upload `esp32/build/iot_gateway.bin`, rồi:

```sh
python3 tools/package_firmware.py esp32/build/iot_gateway.bin --target esp32 --version 2 --url https://firmware.example.com/esp32/gateway-v2.bin --output gateway-ota.json
```

`version` là metadata HMAC; version ESP32 báo lúc boot là `PROJECT_VER` trong binary. Sau OTA, ESP32 báo `rebooting`, sau đó `status` chứa phiên bản đang chạy và `ota/status` báo `confirmed` khi khởi tạo cục bộ và kết nối MQTT thành công. Chưa lưu job_id của OTA ESP32 để đối chiếu kết quả sau reboot. Nếu không kết nối MQTT trong 180 giây, rollback; vì vậy mất mạng dài sau OTA cũng có thể làm bản firmware mới bị rollback. Xác nhận MQTT không thay thế kiểm thử tất cả chức năng ứng dụng.

## Gửi lệnh từ máy tính

```sh
python3 -m pip install -r tools/requirements.txt
export MQTT_HOST=mqtt.example.com
export MQTT_PORT=8883
export MQTT_USERNAME=operator
export MQTT_PASSWORD='your-password'
python3 tools/send_command.py node-ota.json --gateway gw-001
```

Windows PowerShell dùng `$env:MQTT_HOST='...'` và tương tự cho các biến còn lại. Nếu dùng CA riêng, đặt `MQTT_CA_FILE` trỏ tới PEM. Script xác nhận broker nhận lệnh; kết quả thiết bị phải xem `ota/status`.

## Phục hồi và giới hạn

- W25Q32 bắt buộc cho OTA cảm biến. Thiếu/sai JEDEC ID: đọc cảm biến vẫn được nhưng bắt đầu OTA thất bại.
- Mất mạng lúc tải hoặc mất nguồn lúc truyền: firmware hiện tại giữ nguyên; chạy lại toàn bộ job với ID mới. Không resume download/chunk session sau reset.
- Mất nguồn khi bootloader copy: metadata pending vẫn tồn tại; nạp lại từ đầu khi có nguồn. Vector được ghi cuối; ứng dụng nạp dở không được boot nếu W25Q32 mất kết nối.
- STM32 **không rollback tự động khi firmware mới chạy lỗi**. Nếu ứng dụng mới vẫn trả lời Modbus, có thể OTA lại; nếu ứng dụng treo hoàn toàn, dùng ST-LINK. Bản này chưa có watchdog, boot-confirm hay bản sao firmware cũ.
- OTA không cập nhật driver cảm biến, bộ tham số hiệu chuẩn hoặc model MCU khác một cách tự động. HMAC bảo vệ descriptor+image; kiểm tra model/kích thước trước khi ghi.
- Đây là nguồn để đưa lên bàn thử nghiệm. Kiểm tra cấp nguồn, flash erase khi mất điện, nhiễu RS485, số nút, MQTT cloud và OTA thực trước khi triển khai ngoài hiện trường.

## Tệp quan trọng

- `esp32/main/main.c`: Wi-Fi, MQTT, hàng đợi và tác vụ.
- `esp32/main/bus.c`: UART, broadcast gán địa chỉ, quét read-only và Modbus.
- `esp32/main/ota.c`: HTTPS, xác thực và điều phối OTA.
- `stm32/src/board.c`: driver GPIO/UART/SPI/ADC/flash.
- `stm32/src/node.c`: server Modbus và giao thức cấu hình/OTA.
- `stm32/src/stage.c`: staging, xác thực, nạp firmware an toàn hơn khi mất nguồn.
- `stm32/src/config.c`: cấu hình địa chỉ hai trang flash.
- `common/`: giao thức, CRC, SHA256/HMAC dùng chung.
- `docs/PROTOCOL.md`: đặc tả frame.
- `tests/run.sh`: kiểm thử host, không cần board.

## Tài liệu chính thức

- STM32F103C8: https://www.st.com/en/microcontrollers-microprocessors/stm32f103c8.html
- RM0008: https://www.st.com/resource/en/reference_manual/cd00171190-stm32f101-103-105-107-stm32f100-series-armbased-32bit-mcus-stmicroelectronics.pdf
- Modbus serial: https://www.modbus.org/docs/Modbus_over_serial_line_V1_02.pdf
- ESP-IDF OTA: https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32/api-reference/system/ota.html
- ESP-MQTT: https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32/api-reference/protocols/mqtt.html

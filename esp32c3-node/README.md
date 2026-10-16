# Node cảm biến ESP32-C3 thay STM32F103

Phiên bản này thay **node STM32**, giữ gateway ở `../esp32`:

```text
MQTT / Wi-Fi → ESP32-C3 gateway → RS485 → ESP32-C3 node + cảm biến
```

Hai vai trò dùng hai bo mạch khác nhau. Node không chạy Wi-Fi/MQTT. Có thể nối
node ESP32-C3 và STM32 trên cùng bus, mỗi node có địa chỉ riêng. Mã xử lý Modbus
được dùng chung từ `../stm32/src/node.c`; driver phần cứng, NVS và OTA nằm trong
project này. Bản STM32 vẫn build theo cách cũ.

## Đấu dây node

Dùng module ESP32-C3 có flash **ít nhất 4 MB** và transceiver RS485 **3.3 V**
(ví dụ MAX3485). Không nối UART node trực tiếp với UART gateway.

| ESP32-C3 node | Kết nối |
|---|---|
| GPIO4 | DI / TX của transceiver |
| GPIO5 | RO / RX của transceiver |
| GPIO6 | DE và /RE nối chung |
| 3V3 / GND | VCC / GND transceiver |
| GPIO0 / ADC1 channel 0 | Ngõ ra analog cảm biến |
| GPIO3 | Nút nhấn xuống GND, giữ 5 giây để xóa địa chỉ và khởi động lại |

Nối A–A, B–B và GND chung giữa các transceiver. Chỉ đặt điện trở kết thúc
120 Ω tại hai đầu bus. Kiểm tra sơ đồ bo thực tế; GPIO4/5/6 có thể xung đột
JTAG ngoài. GPIO18/19 dành cho console phụ USB Serial/JTAG của project.

Với module tự động chuyển chiều chỉ có `VCC/TXD/RXD/GND` như loại đang dùng:
cấp nguồn 3.3 V theo thông số module, nối GPIO4 → TXD, GPIO5 ← RXD,
GND chung và để GPIO6 không nối. Không nối module vào chân TX/RX của UART0
đang dùng làm console trên bo CH340.

ADC trả raw 12 bit và millivolt đã hiệu chuẩn bằng driver ESP-IDF. Không dùng
công thức ADC STM32 `raw * 3300 / 4095`. Với ESP32-C3 ở mức suy hao 12 dB,
thiết kế tín hiệu analog trong dải đo khoảng **0–2.5 V**, dùng chia áp nếu cần;
không đưa tín hiệu 5 V vào GPIO0. Nếu đọc hoặc hiệu chuẩn thất bại, trường tương
ứng trả `0xffff`. Giá trị này không phải phép đo hợp lệ.

Sensor type 1 (gas) và 3 (CH3) báo raw/mV. Type 2 chỉ khai báo loại nhiệt độ/độ
ẩm theo giao thức hiện tại; chưa có driver đo nhiệt độ/độ ẩm.

## Build và nạp

Dùng ESP-IDF **5.4.3**. Trong terminal đã export ESP-IDF:

```sh
cd esp32c3-node
idf.py set-target esp32c3
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

Trong `RS485 sensor node`, chọn sensor family `0` unknown, `1` gas,
`2` temperature/humidity, hoặc `3` CH3. Mặc định là `0`. TX/RX/DE/nút reset
cũng cấu hình tại đây; không chọn các GPIO trùng nhau, GPIO0 đang dùng ADC,
hoặc chân dành cho flash/USB trên bo của bạn.

Ảnh ứng dụng: `build/esp32c3_node.bin`. Nạp lần đầu bằng `idf.py flash` để nạp
cả bootloader và bảng phân vùng; không chỉ nạp file ứng dụng vào offset 0.

Console mặc định dùng **UART0, 115200 baud** cho các bo có cầu USB–UART CH340;
USB Serial/JTAG trên GPIO18/19 là console phụ. Nếu đã có `sdkconfig` từ bản
cũ, vào `Component config` → `ESP System Settings`, chọn console UART0 mặc
định và bật console phụ USB Serial/JTAG, rồi build/nạp lại.

Sau khi khởi động, log có `Ready: address=... sensor_type=...` và nếu chưa gán
địa chỉ sẽ có `Unconfigured`. Cứ 10 giây có `Alive` kèm `address`, `rx_frames`,
`tx_frames`, `uart_errors`. `rx_frames` đếm khung UART phân tách được, chưa
khẳng định CRC hoặc địa chỉ hợp lệ. Node địa chỉ 0 không trả lời scan; cần
gán địa chỉ bằng `assign_address` trước. Cảm biến analog chưa cắm không ngăn
log `Ready`/`Alive` xuất hiện.

## Gán địa chỉ và đọc cảm biến

Giữ nguyên 38400 baud, 8N1, FC03 registers 0–12 và broadcast FC06 register
`0x0100` để gán địa chỉ 1–247. Địa chỉ lưu trong NVS qua reset/mất điện.
Node đã có địa chỉ bỏ qua broadcast gán lại. Giữ nút GPIO3 5 giây để trở về 0.
UID 12 byte gồm ASCII `C3NODE` và 6 byte MAC eFuse của chip.

Chỉ cấp nguồn **một node chưa gán địa chỉ tại một thời điểm**. Trên gateway hiện
tại, gửi RPC như với STM32:

```json
{"cmd":"assign_address","rqi":"new-c3-node","data":{"address":5}}
```

Sau đó dùng `scan` / `discover`; gateway nhận loại sensor từ register 12 và
publish theo luồng MQTT hiện tại. Xem `../docs/MQTT.md` và `../docs/PROTOCOL.md`.

## OTA node qua RS485

Node giữ custom FC `0x42` với begin/data/commit/progress như bản STM32, nhưng
dùng hai phân vùng OTA nội bộ ESP32-C3, không cần SPI NOR ngoài. Descriptor
16 byte little-endian gồm:

| Offset | uint32 |
|---|---|
| 0 | Model `0x0033434e` (`MODEL_ESP32C3_NODE`) |
| 4 | Kích thước file ứng dụng, tối đa `0x180000` byte |
| 8 | CRC32 của toàn bộ file ứng dụng |
| 12 | Version metadata, hiện không dùng để chặn downgrade |

Gửi data tuần tự, mỗi chunk tối đa 128 byte. Gửi lại chunk đã ghi được chấp
nhận nếu nội dung khớp. Commit kiểm CRC và để ESP-IDF xác thực image trước khi
chọn phân vùng boot và restart. Mất điện khi upload vẫn boot ứng dụng đang chạy;
upload chưa hoàn tất không tiếp tục sau reset, cần begin lại. Chưa cấu hình
rollback tự động khi ứng dụng mới boot lỗi.

Gateway MQTT hiện tại **chưa nối luồng OTA node**; không dùng
`tools/package_firmware.py --target stm32` cho file ESP32-C3. Nạp USB/UART là
cách cập nhật sẵn có; FC42 cần master/tool riêng hiểu descriptor trên.

Tài liệu phần cứng/API tham chiếu:
[ADC ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/v5.4.3/esp32c3/api-reference/peripherals/adc_oneshot.html),
[UART](https://docs.espressif.com/projects/esp-idf/en/v5.4.3/esp32c3/api-reference/peripherals/uart.html),
[OTA](https://docs.espressif.com/projects/esp-idf/en/v5.4.3/esp32c3/api-reference/system/ota.html).

# Node cảm biến ESP32-C3

Master ở `../esp32`, node ở project này. Mỗi node chọn một trong ba biến thể:
ZE03 H2S + NH3, WPCH01 áp suất nước hoặc ZS03 nhiệt độ/độ ẩm.
RS485 dùng 38400 baud, 8N1. UID gồm `C3NODE` và MAC eFuse.
Node ZE03 dành cả UART0/1 cho cảm biến nên RS485 dùng UART mềm (thu GPIO,
phát RMT). Các profile ZS03/WPCH01 vẫn dùng UART1 phần cứng cho RS485.

## Sơ đồ nối chân tổng hợp

Mỗi bo node chỉ nạp **một profile cảm biến**: ZE03, ZS03 hoặc WPCH01. Tất cả
master và node dùng transceiver RS485 mức logic 3.3 V, nối chung `A`, `B` và
`GND`. Không nối trực tiếp chân UART của master với chân UART của node.

### Master ESP32-C3

| Chân master | Nối đến | Ghi chú |
|---|---|---|
| GPIO7 (UART1 TX) | DI/TXD của transceiver RS485 | Master phát dữ liệu |
| GPIO6 (UART1 RX) | RO/RXD của transceiver RS485 | Master nhận dữ liệu |
| GPIO10 | DE và /RE nối chung | Điều khiển chiều truyền |
| 3V3 | VCC transceiver | Chỉ dùng transceiver mức logic 3.3 V |
| GND | GND transceiver và GND toàn hệ thống | Bắt buộc nối mass chung |
| GPIO9 | Nút nhấn xuống GND | Nút cấu hình/factory reset của master |

### Phần chung của mọi node ESP32-C3

| Chân node | Nối đến | Ghi chú |
|---|---|---|
| GPIO7 (TX) | DI/TXD của transceiver RS485 | UART1 phần cứng với ZS03/WPCH01; RMT với ZE03 |
| GPIO6 (RX) | RO/RXD của transceiver RS485 | UART1 phần cứng với ZS03/WPCH01; GPIO UART mềm với ZE03 |
| GPIO10 | DE và /RE nối chung | Điều khiển chiều truyền |
| 3V3 | VCC transceiver | Transceiver phải tương thích logic 3.3 V |
| GND | GND transceiver, cảm biến và master | Bắt buộc nối mass chung |
| GPIO3 | Nút nhấn xuống GND | Giữ 5 giây để xóa địa chỉ node |

Nếu module RS485 tự chuyển chiều chỉ có `VCC/TXD/RXD/GND`, nối GPIO7 đến
`TXD`, GPIO6 đến `RXD` và để GPIO10 không nối. Với loại có chân `DI/RO/DE/RE`,
nối theo bảng trên. Không nối đồng thời theo cả hai kiểu.

### Node ZE03: hai module H2S và NH3

| Chân ESP32-C3 node | Nối đến cảm biến |
|---|---|
| GPIO20 (UART0 RX) | TXD của ZE03-H2S |
| GPIO21 (UART0 TX) | RXD của ZE03-H2S |
| GPIO4 (UART1 RX) | TXD của ZE03-NH3 |
| GPIO5 (UART1 TX) | RXD của ZE03-NH3 |
| 5V | VCC của cả hai ZE03 |
| GND | GND của cả hai ZE03 |

Hai ZE03 có hai đường UART riêng, 9600 baud 8N1. Không nối chung RX/TX giữa
hai module. Nguồn cảm biến là 5 V; tín hiệu UART của ZE03 là TTL mức 3 V theo
tài liệu. Profile này dùng GPIO6/7 cho RS485 mềm vì cả hai UART phần cứng đã
dành cho cảm biến.

### Node ZS03 nhiệt độ và độ ẩm

| Chân ZS03 | Nối đến ESP32-C3 node |
|---|---|
| Chân 1 VCC | 3V3 |
| Chân 2 DATA | GPIO4 |
| Chân 3 NC | Không nối |
| Chân 4 GND | GND |

Lắp điện trở kéo lên 4.7 kohm từ `DATA/GPIO4` đến `3V3`. Profile này dùng
UART1 phần cứng GPIO6/7 cho RS485.

### Node WPCH01 bản I2C

| Dây WPCH01 | Nối đến ESP32-C3 node |
|---|---|
| Đỏ V+ | 3V3 |
| Đen V- | GND |
| Vàng SCL | GPIO5 |
| Xanh SDA | GPIO4 |

SDA và SCL cần điện trở kéo lên 3V3 nếu module không có sẵn. Chỉ áp dụng bảng
này cho đúng phiên bản WPCH01 I2C 3.3 V. Profile này dùng UART1 phần cứng
GPIO6/7 cho RS485. Firmware hiện chỉ dò địa chỉ I2C; chưa thể đổi dữ liệu sang
áp suất vì tài liệu hiện có không công bố giao thức đọc và hệ số chuyển đổi.

### Nối bus RS485 giữa master và các node

```text
Master MAX3485            Bus RS485             MAX3485 mỗi node
       A  ------------------- A ------------------- A
       B  ------------------- B ------------------- B
      GND ------------------ GND ----------------- GND
```

Mắc các node song song trên một đường trục, đặt địa chỉ Modbus khác nhau cho
từng node. Chỉ lắp điện trở kết thúc 120 ohm tại hai đầu vật lý của bus, không
lắp tại mọi node. Nếu không liên lạc được, kiểm tra ký hiệu A/B trong tài liệu
của đúng module transceiver trước khi thử đảo hai dây.

## RS485 cho cả master và node

| GPIO | Kết nối transceiver 3.3 V |
|---|---|
| GPIO6 (RX) | RO / RXD |
| GPIO7 (TX) | DI / TXD |
| GPIO10 | DE và /RE nối chung |
| 3V3 / GND | Nguồn và mass |

Với transceiver tự chuyển chiều, để GPIO10 không nối. Nối A-A, B-B, GND chung;
điện trở kết thúc 120 ohm tại hai đầu bus. GPIO3 là nút reset địa chỉ: kéo xuống
GND trong 5 giây. Không dùng chân này cho cảm biến.

## Node ZE03 H2S + NH3

ESP32-C3 chỉ có UART0 và UART1. H2S dùng UART0, NH3 dùng UART1 độc lập.
RS485 của profile này chuyển sang UART mềm trên GPIO6/7.
Console dùng USB Serial/JTAG (GPIO18/19); log không còn đi ra CH340/UART0.

| ESP32-C3 | Kết nối |
|---|---|
| GPIO20 (UART0 RX H2S) | TXD của ZE03-H2S |
| GPIO21 (UART0 TX H2S) | RXD của ZE03-H2S |
| GPIO4 (UART1 RX NH3) | TXD của ZE03-NH3 |
| GPIO5 (UART1 TX NH3) | RXD của ZE03-NH3 |
| GND | GND cả hai module |
| Nguồn 5 V ổn định | VCC cả hai ZE03 |

**Mỗi module có cặp RX/TX riêng; không nối chung các đường UART.** Hai task đọc
độc lập bằng lệnh 0x86 ở chế độ hỏi/đáp 0x78/0x04, 9600 baud 8N1. Mỗi task đợi
2 giây sau mỗi lần đọc. Kiểm checksum khung 9 byte, mã khí H2S=3, NH3=2 và số
chữ số thập phân. Không phát lệnh hiệu chuẩn. Nếu dùng dây theo bản cũ, chuyển
H2S dùng cặp UART0 mặc định GPIO20/21; NH3 dùng UART1 được ánh xạ sang GPIO4/5.

Chờ làm nóng 5 phút trước khi công bố phép đo. Mỗi kênh có bit hợp lệ riêng;
lỗi một module không làm mất số đo module kia. Mất dữ liệu quá 6 giây
sẽ xóa bit hợp lệ. MQTT dùng `h2s_ppm`, `nh3_ppm`; thiếu dữ liệu trả `null`.
ZE03 cấp nguồn 5 V nhưng giao tiếp TTL mức 3 V theo manual.

### RS485 mềm của node ZE03

RX=GPIO6, TX=GPIO7, DE/RE=GPIO10; master vẫn dùng UART1 phần cứng trên cùng số
chân. Node ghi thời điểm cạnh RX bằng ISR trong IRAM, giải mã 8N1 trong task,
ghép khung theo khoảng im lặng RTU 1.75 ms. Khoảng nghỉ giữa ký tự trên 750 us,
lỗi stop bit hoặc tràn bộ đệm làm hủy khung; CRC Modbus tiếp tục kiểm tra ở parser.
Phát dùng RMT để tạo độ rộng bit 26/27 us, giữ stop bit và chỉ hạ DE sau khi phát xong.
Không đổi baud rate hay giao thức commissioning/telemetry.

Đây là UART mềm có hỗ trợ RMT, không phải UART phần cứng thứ ba. Đã kiểm thử bộ
giải mã bằng dạng sóng mô phỏng (256 byte, lệch clock, jitter, tràn/lỗi và phục hồi),
nhưng **chưa xác nhận timing trên bo thật**. Cần kiểm tra bằng logic analyzer và
thử tải hai cảm biến cùng lúc, kể cả khi ghi NVS/OTA; không xem build thành công
là bằng chứng đường truyền đã ổn định trên phần cứng.

## Node ZS03

| Chân ZS03 | Kết nối |
|---|---|
| 1 VCC | 3V3 |
| 2 DATA | GPIO4, điện trở kéo lên 4.7 kohm về 3V3 |
| 3 NC | Không nối |
| 4 GND | GND |

Dùng giao tiếp một dây riêng của ZS03. Firmware kéo thấp 20 ms, thu 40 bit theo
độ rộng xung, kiểm checksum và dấu nhiệt độ ở bit 7 byte thứ tư. Chu kỳ đọc 6 giây.
Độ phân giải nguyên theo manual; thanh ghi vẫn dùng đơn vị 0.1 C / 0.1 %RH.
Dưới 0 C chỉ công bố nhiệt độ vì độ ẩm không chính xác theo manual.
MQTT dùng `temperature_c`, `humidity_rh`; dữ liệu lỗi/hết hạn trả `null`.

## Node WPCH01 I2C: chưa có giao thức đo được xác nhận

| Dây WPCH01 bản I2C | Kết nối |
|---|---|
| Đỏ V+ | 3V3 |
| Đen V- | GND |
| Vàng SCL | GPIO5 |
| Xanh SDA | GPIO4 |

Dùng điện trở kéo lên SDA/SCL về 3V3 phù hợp với bus. Phải đúng phiên bản I2C;
các phiên bản điện áp/dòng có nguồn và cách đọc khác.

Đã kiểm tra PDF đi kèm, trang sản phẩm hãng và manual V4.0 công khai vào
2026-10-16. Chưa tìm thấy địa chỉ I2C, lệnh/định dạng phản hồi, checksum hoặc hệ số
chuyển đổi cho WPCH01 bản I2C. Dải đo tùy mã đặt hàng.

Firmware khởi tạo bus I2C, dò ACK địa chỉ 7 bit để chẩn đoán, đăng ký node loại
`water_pressure` và child `MODBUS_PRESSURE`. **Chưa triển khai đọc áp suất**:
`pressure_pa=null`, `quality=configuration_required`. ACK không xác định giao thức.
Cần bảng giao thức đúng phiên bản từ hãng/nhà cung cấp và dải đo của thiết bị để
hoàn thiện; không áp dụng giao thức của cảm biến khác cho WPCH01.

Nguồn tìm kiếm:

- [Trang WPCH01 của hãng](https://www.winsen-sensor.com/product/wpch01.html).
- [Manual V4.0](https://www.winsen-sensor.com/d/files/manual/wpch01.pdf): bản cũ chỉ có ngõ analog.

## Chọn loại, build và nạp

ESP-IDF 5.4.3. Chọn trong `idf.py menuconfig` → `RS485 sensor node` → `Node sensor`.
Type: 1 ZE03, 2 ZS03, 4 WPCH01. Type 3 giữ cho CH3 legacy của STM32 để master
không diễn giải lại dữ liệu đã lưu thành áp suất. Mặc định chọn ZS03.

Tạo build riêng từ profile, trong terminal đã export ESP-IDF:

```powershell
cd esp32c3-node
idf.py -B build/ze03 -D SDKCONFIG=build/ze03/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ze03' build
idf.py -B build/zs03 -D SDKCONFIG=build/zs03/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.zs03' build
idf.py -B build/wpch01 -D SDKCONFIG=build/wpch01/sdkconfig -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.wpch01' build
```

Nạp bằng `idf.py -B build/ze03 -p COMx flash monitor` (thay profile tương ứng).
Ảnh ứng dụng là `build/<profile>/esp32c3_node.bin`. Lần đầu dùng `flash` để nạp cả
bootloader và bảng phân vùng. Với `sdkconfig` cũ, chọn console USB Serial/JTAG và
đổi RX=6, TX=7, DE=10 trong menuconfig; defaults không ghi đè giá trị đã lưu.
Firmware chặn build ZE03 nếu console vẫn chiếm UART0.

## Modbus và MQTT

Gán địa chỉ bằng broadcast FC06, register `0x0100`, ID 1-247. Chỉ cấp nguồn một
node chưa gán tại một thời điểm. Địa chỉ lưu trong NVS. Node đã gán không nhận
gán lại; nhấn GPIO3 5 giây để xóa địa chỉ. Gateway nhận dạng qua register 12.

```json
{"cmd":"assign_address","rqi":"new-c3-node","data":{"address":5}}
```

Register 0-12 giữ vị trí cũ; node digital không dùng ADC nên 0/1 là `0xffff`.
Master đọc phần mở rộng riêng, firmware C3 cũ vẫn được nhận dạng:

| Register | Ý nghĩa |
|---|---|
| 13 | Schema `0x0101` |
| 14 | Valid bits: H2S=1, NH3=2, pressure=4, temp=8, humidity=16; warming up=0x4000; protocol unavailable=0x8000 |
| 15 | H2S, ppm x100 |
| 16 | NH3, ppm x100 |
| 17-18 | Áp suất uint32 Pa, word cao trước (chưa có phép đo WPCH01) |
| 19 | Nhiệt độ int16 có dấu, C x10 |
| 20 | Độ ẩm %RH x10 |

Chỉ dùng giá trị khi bit hợp lệ tương ứng bật, kể cả nhiệt độ có thể trùng `0xffff`.
Master gửi `null` khi dữ liệu lỗi/offline, trạng thái `partial` khi chỉ có một phần
dữ liệu. `warming_up` cho ZE03, `configuration_required` cho WPCH01.

## OTA node qua RS485

Giữ FC `0x42` với begin/data/commit/progress, sử dụng hai phân vùng OTA nội bộ.
Descriptor 16 byte little endian: model `0x0033434e`, kích thước ảnh ứng dụng tối
đa `0x180000`, CRC32 và version metadata (mỗi trường uint32). Gửi tuần tự từng
chunk tối đa 128 byte. Commit kiểm CRC và image ESP-IDF rồi chọn boot/restart.

Gateway MQTT chưa nối luồng OTA node; dùng USB/UART để nạp hoặc master/tool
FC42 tương thích. Không dùng gói OTA STM32 cho ESP32-C3.

# Cấu hình Wi-Fi master ESP32-C3 qua UDP

> Cloud telemetry uses MQTT 5 / schema v2. `assign_address` now requires
> `data.sensorUid`; cloud UDP provisioning uses `setupCloud`. See
> [Cloud v2](./CLOUD_V2.md) for the current telemetry and commissioning contract.


Chức năng này thuộc firmware master `esp32/`, không thuộc `esp32c3-node/`.
Giữ tên lệnh UDP cũ và envelope `cmd` / `rqi` / `data`, cổng **8181**.

## Kết nối lần đầu

1. Nạp master và bật nguồn. Nếu chưa có SSID, AP cấu hình mở ngay. Nếu có
   cấu hình nhưng không kết nối được, đợi khoảng 30 giây.
2. Kết nối điện thoại/máy tính vào `RD_MH_<6 ký tự MAC cuối>`.
   Mật khẩu mặc định: **`configureme`**. Có thể đổi khi build tại
   `idf.py menuconfig` → `Minihub Configuration` → `Wi-Fi setup AP password`.
   Mật khẩu phải dài 8–63 byte. Nếu cấu hình ngoài khoảng này, firmware ghi
   cảnh báo và dùng `configureme` để vẫn khởi động được.
3. Gửi UDP tới **192.168.4.1:8181**. Máy gửi cần ở cùng mạng AP của master.
4. Khi master lấy được IP router, AP cấu hình tiếp tục hoạt động khoảng
   30 giây để nhận phản hồi, sau đó tắt. Nếu mất kết nối lâu, AP mở lại.

Lệnh `setupHC` và `HC_SCAN_WIFI` chỉ nhận từ subnet AP khi AP đang bật.
Discovery có thể dùng trên LAN bằng IP master hoặc broadcast
`255.255.255.255:8181`. Mỗi `rqi` được trả nguyên vẹn để đối chiếu phản hồi.

## Công cụ Python

Không cần cài thêm thư viện. Từ thư mục gốc repository:

```sh
python tools/configure_wifi_udp.py discover
python tools/configure_wifi_udp.py scan
python tools/configure_wifi_udp.py setup --ssid "TenWifi"
```

Lệnh setup hỏi mật khẩu bằng prompt ẩn. Với mạng không có mật khẩu:

```sh
python tools/configure_wifi_udp.py setup --ssid "TenWifiMo" --open
```

Tìm master đang online trong LAN:

```sh
python tools/configure_wifi_udp.py discover --host 255.255.255.255
```

UDP không bảo đảm chuyển gói; nếu mất phản hồi, kiểm tra discovery hoặc thử lại.
Khi đổi kênh để kết nối router, thiết bị gửi có thể cần nối lại AP cấu hình.

## JSON request / response

Discovery (`aiHubBroadCast` và `scanIpHc` cũng được hỗ trợ):

```json
{"cmd":"SCAN_HC","rqi":"find-1","data":{}}
```

Phản hồi có `CMD: "HC_RESPONSE"`, `IP`, `MAC`, `HOSTNAME`, `TYPE: 2`, cùng
`data.ip`, `data.mac`, `data.setup_active`, `data.code`.

Quét tối đa 32 mạng Wi-Fi:

```json
{"cmd":"HC_SCAN_WIFI","rqi":"scan-1","data":{}}
```

Phản hồi có `CMD: "HC_SCAN_WIFI_RESPONSE"`, `REQUEST_ID`, `DATA` là danh sách
`SSID` / `RSSI` / `AUTHMODE`, và `data.code` (`0` thành công).

Thiết lập SSID/mật khẩu:

```json
{"cmd":"setupHC","rqi":"wifi-1","data":{"ssid":"TenWifi","pass":"MatKhauWifi"}}
```

SSID dài 1–32 byte; mật khẩu rỗng cho mạng mở, hoặc 8–63 byte cho WPA/WPA2.
Không chấp nhận NUL, không hỗ trợ khóa PSK dạng hex 64 ký tự. `homeId` từ ứng
dụng cũ không được dùng để thay đổi thông tin gateway trong endpoint Wi-Fi này.
Mỗi request tối đa 1024 byte.

Khi dữ liệu hợp lệ, master trả trước:

```json
{"cmd":"setupHCRsp","rqi":"wifi-1","data":{"code":0,"mac":"112233445566","status":"connecting"}}
```

Sau khi lấy được IP router, master trả:

```json
{"cmd":"setupHCRsp","rqi":"wifi-1","data":{"code":0,"mac":"112233445566","status":"connected","ip":"192.168.1.50"}}
```

Nếu chưa kết nối được sau khoảng 15 giây, phản hồi có `code: -1`,
`status: "connection_failed"`. Cấu hình đã lưu trong Wi-Fi NVS vẫn được dùng
để thử lại; AP còn mở để gửi mật khẩu mới. `connecting` chỉ xác nhận nhận lệnh,
không có nghĩa master đã online. Request sai định dạng trả code khác 0 hoặc
bị bỏ qua nếu envelope không hợp lệ. Mật khẩu không được ghi vào log/phản hồi.

Wi-Fi NVS của ESP-IDF lưu cặp SSID/mật khẩu để dùng sau reset; factory reset
5 click hiện tại xóa cấu hình này. UDP không đổi cấu hình MQTT hay địa chỉ node.
SNTP khởi động cả khi chưa có mạng để có thể đồng bộ sau khi cấu hình thành công.

API tham chiếu: [ESP-IDF Wi-Fi/APSTA](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32c3/api-reference/network/esp_wifi.html).

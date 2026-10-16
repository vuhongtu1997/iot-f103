# Cloud MQTT 5 / telemetry v2

## Cấu hình cloud bằng UDP

Kết nối máy tính với AP setup của Master và gửi UDP tới `192.168.4.1:8181`:

```json
{"cmd":"setupCloud","rqi":"cloud-1","data":{"host":"emqx.example.com","user":"gw-TH-KIMDONG-NVS-A1","pass":"<INGEST_TOKEN>","port":1883}}
```

`host` là hostname/IP broker, không có tiền tố `mqtt://`. `user` là gateway
`device_uid` (1–128 byte, không chứa `/`, `+`, `#`); `pass` là ingest token
plaintext (1–255 byte). Port là số nguyên 1–65535. Cấu hình được lưu NVS;
phản hồi thành công có `data.status="saved"`, `data.reboot_required=true`.
Khởi động lại Master để áp dụng. Phản hồi không chứa token.

Công cụ dòng lệnh sẽ hỏi token qua prompt ẩn:

```sh
python tools/configure_wifi_udp.py cloud --cloud-host emqx.example.com --cloud-port 1883 --user gw-TH-KIMDONG-NVS-A1
```

Wi-Fi vẫn cấu hình riêng bằng action `setup`. `setupCloud` chỉ chấp nhận từ
subnet AP khi chế độ setup đang hoạt động. Firmware hiện dùng MQTT TCP;
thay port thành 8883 không tự bật TLS.

## Gán địa chỉ và sensorUid

RPC qua topic quản lý hiện có `/v3/server/hc/{mac}/jsonreq`:

```json
{"cmd":"assign_address","rqi":"address-5","data":{"address":5,"sensorUid":"gas-TH-KIMDONG-A1-1"}}
```

`sensorUid` bắt buộc, 1–128 byte, không chứa NUL. Master lưu UID cloud trong
JSON `data.sensor_uid` của bảng SQLite `Device`, cùng với địa chỉ và UID phần
cứng. Bản ghi giữ địa chỉ trước khi broadcast cũng chứa sensor UID, nên scan
sau một lần gán chưa xác nhận vẫn giữ được mapping. UID cloud không ghi xuống node.
Response `params.sensorUid` và kết quả `scan`/`discover` trả lại mapping này.

Để gán hoặc sửa UID cho node đã có địa chỉ (kể cả dữ liệu cũ chưa có UID cloud):

```json
{"cmd":"set_sensor_uid","rqi":"uid-5","data":{"address":5,"sensorUid":"gas-TH-KIMDONG-A1-1"}}
```

Lệnh này cập nhật database Master, không broadcast hay đổi địa chỉ node.
Node nhiệt/ẩm dùng UID như `env-TH-KIMDONG-A1`. Nếu có node áp lực riêng,
gán **cùng UID môi trường** để ghép metrics vào một `env`. Các node gas phải
dùng UID khác nhau. Hiện mỗi Master tạo batch cho một khu với một UID môi trường.

## Publish telemetry

- MQTT **5**, CONNECT username = `user`, password = `pass` đã lưu.
- Topic `smarttoilet/v2/{user}/telemetry`, QoS **1**, retain **false**.
- JSON có `schema_version: 2`, `device_uid`, `ingest_token`, `sampled_at`,
  `env` và `gas`. Không bọc trong `cmd`/`data`.
- Một batch sau mỗi vòng poll, nghỉ 5 giây giữa các vòng. Chỉ dùng mẫu đọc
  thành công trong chính vòng đó, có mapping UID cloud và giờ đã đồng bộ SNTP.
- Timestamp UTC RFC3339; bỏ batch nếu mẫu cũ hơn 24 giờ hoặc vượt tương lai
  5 phút. Không gửi batch khi thiếu `env` hoặc không có gas hợp lệ.
- Bỏ metric lỗi/chưa sẵn sàng, không gửi `null`, NaN, Inf hoặc số ADC giả ppm.
  Batch có hơn 32 gas, trùng UID gas, nhiều UID môi trường hoặc trùng metric
  môi trường sẽ bị bỏ. Body tối đa 32 KiB.
- `faucets` được bỏ qua vì chưa có cảm biến vòi trong register map; server giữ
  snapshot vòi hiện có. Firmware không gửi lệnh điều khiển vòi.
- Topic RPC/heartbeat v3 vẫn phục vụ quản lý; vòng poll cảm biến dùng telemetry v2.

| Register metric | Cloud metric | Đơn vị |
|---|---|---|
| ZE03 NH3 / H2S | `nh3` / `h2s` | ppm |
| ZS03 temperature / humidity | `temp_c` / `humidity_pct` | °C / % |
| Pressure | `water_pressure` | Pa, giữ đơn vị register hiện tại |

Schema được cung cấp chưa định nghĩa đơn vị `water_pressure`. Cần thống nhất
đơn vị với backend trước khi dùng áp lực thực tế. WPCH01 hiện chưa giải mã được
giá trị áp lực nên metric này không xuất hiện. Node STM32 chỉ có ADC legacy
không được đưa vào telemetry v2 khi chưa có công thức hiệu chuẩn.

Cấu hình MQTT 5 theo [ESP-MQTT của Espressif](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/protocols/mqtt.html).

Chưa kiểm chứng trên broker EMQX thật hoặc bus RS485 vật lý.

# Bản tin JSON MQTT cho gateway RS485

> Cloud telemetry uses MQTT 5 / schema v2. `assign_address` now requires
> `data.sensorUid`; cloud UDP provisioning uses `setupCloud`. See
> [Cloud v2](./CLOUD_V2.md) for the current telemetry and commissioning contract.


Tài liệu này mô tả các bản tin MQTT liên quan đến gateway ESP32-C3 và các node STM32 Modbus. `{mac}` là MAC/ID của gateway được dùng khi khởi tạo `Gateway`.

## Topic

| Chiều truyền | Topic | Nội dung |
|---|---|---|
| Server → ESP32 | `/v3/server/hc/{mac}/jsonreq` | Lệnh RPC gửi xuống gateway |
| ESP32 → Server | `/v3/hc/{mac}/server/jsonres` | Kết quả RPC |
| ESP32 → Server | `/v3/hc/{mac}/server/jsonreq` | Telemetry và trạng thái do gateway chủ động gửi |

Firmware dùng MQTT QoS 0 và không bật retain cho các bản tin này.

## Khuôn dạng RPC

Mọi request gửi từ server xuống ESP32 phải có đủ ba trường:

```json
{
  "cmd": "ten_lenh",
  "rqi": "request-id-duy-nhat",
  "data": {}
}
```

- `cmd`: tên lệnh.
- `rqi`: chuỗi định danh request. Response trả lại đúng giá trị này để ghép cặp request/response.
- `data`: luôn phải là một JSON object, kể cả khi lệnh không có tham số.

Response có dạng:

```json
{
  "method": "ten_lenhRsp",
  "params": {},
  "rqi": "request-id-duy-nhat"
}
```

JSON sai định dạng, thiếu `cmd`, `rqi`, `data`, hoặc `data` không phải object sẽ bị bỏ qua và không có response. Lệnh chưa đăng ký cũng không có response.

## Quét node: `scan` hoặc `discover`

Hai tên lệnh có cùng chức năng và cùng trả về `discoverRsp`. Đây là thao tác chỉ đọc, không cấp địa chỉ cho node mới.

Request:

```json
{
  "cmd": "discover",
  "rqi": "discover-001",
  "data": {}
}
```

Có thể thay `"discover"` bằng `"scan"`.

Response thành công, ví dụ node gas ở địa chỉ 5:

```json
{
  "method": "discoverRsp",
  "params": {
    "code": 0,
    "nodes": [
      {
        "address": 5,
        "uid": "112233445566778899aabbcc",
        "version": 1,
        "mode": 0,
        "sensor_type": 1,
        "sensor_type_name": "gas",
        "child_device": {
          "id": "rs485-112233445566778899aabbcc-gas",
          "type": "MODBUS_GAS",
          "name": "gas"
        }
      }
    ]
  },
  "rqi": "discover-001"
}
```

Nếu quét bus hoặc lưu registry thất bại, `params.code` là `-1` và response có thêm:

```json
"error": "Modbus scan failed or registry could not be saved"
```

`sensor_type` có các giá trị: `0` unknown, `1` gas, `2` temperature/humidity, `3` CH3 legacy, `4` water_pressure. Trường `child_device` chỉ xuất hiện khi loại cảm biến được nhận dạng.

Node ESP32-C3 digital dùng type `1` cho hai ZE03 H2S + NH3, `2` cho ZS03 và `4`
cho WPCH01. Parent và child gửi các trường `h2s_ppm`, `nh3_ppm`, `temperature_c`,
`humidity_rh` hoặc `pressure_pa` theo loại. Giá trị không hợp lệ hoặc offline là
`null`; `quality` có thể là `ok`, `partial`, `sensor_error`, `warming_up`,
`configuration_required`, `measurement_unavailable`, `bootloader`, `offline`.
Child áp suất có type `MODBUS_PRESSURE`, ID `rs485-{uid}-water-pressure`.
WPCH01 hiện chỉ khởi tạo/dò bus I2C, chưa đọc áp suất do chưa có giao thức I2C
được xác nhận: `pressure_pa=null`, `quality=configuration_required`.
Các mô tả raw/millivolt bên dưới áp dụng cho STM32 legacy.

## Cấp địa chỉ: `assign_address`

Chỉ cấp nguồn cho một node chưa cấu hình tại một thời điểm. `address` phải là số nguyên từ 1 đến 247.

Request:

```json
{
  "cmd": "assign_address",
  "rqi": "assign-001",
  "data": {
    "address": 5
  }
}
```

Response thành công:

```json
{
  "method": "assign_addressRsp",
  "params": {
    "code": 0,
    "result": 1,
    "address": 5,
    "uid": "112233445566778899aabbcc",
    "version": 1,
    "sensor_type": 1,
    "sensor_type_name": "gas",
    "child_device": {
      "id": "rs485-112233445566778899aabbcc-gas",
      "type": "MODBUS_GAS"
    }
  },
  "rqi": "assign-001"
}
```

Response lỗi có `params.code = -1`, trừ lỗi định dạng địa chỉ có `params.code = 9`. `params.result` là kết quả chi tiết của tầng RS485:

| `result` | Ý nghĩa |
|---:|---|
| `1` | Đã cấp địa chỉ, đọc lại UID và lưu thành công |
| `-1` | Địa chỉ đã đăng ký hoặc đang có node sử dụng |
| `-2` | Bus có hoạt động nên không thể xác nhận địa chỉ đang trống |
| `-3` | Đã gửi lệnh nhưng không xác nhận được; chạy `scan` trước khi thử lại |
| `-4` | Địa chỉ không hợp lệ hoặc registry đã đầy |
| `-5` | Không lưu được reservation/registry |

Ví dụ lỗi:

```json
{
  "method": "assign_addressRsp",
  "params": {
    "code": -1,
    "result": -1,
    "error": "address is already registered or occupied"
  },
  "rqi": "assign-001"
}
```

Nếu `address` thiếu, sai kiểu hoặc ngoài khoảng, response không có `result`:

```json
{
  "method": "assign_addressRsp",
  "params": {
    "code": 9,
    "error": "address must be an integer from 1 to 247"
  },
  "rqi": "assign-001"
}
```

## Telemetry `deviceUpdate`

Gateway quét các node đã đăng ký mỗi 5 giây. Khi MQTT đang kết nối, ESP32 gửi telemetry lên topic `/v3/hc/{mac}/server/jsonreq`.

Ví dụ node gas online:

```json
{
  "cmd": "deviceUpdate",
  "rqi": "rqi-ngau-nhien-16-ky-tu",
  "data": {
    "device": [
      {
        "id": "rs485-112233445566778899aabbcc",
        "data": {
          "address": 5,
          "uid": "112233445566778899aabbcc",
          "stt": 1,
          "sensor_type": 1,
          "sensor_type_name": "gas",
          "version": 1,
          "mode": 0,
          "adc_raw": 2048,
          "millivolts": 1650,
          "quality": "ok"
        }
      },
      {
        "id": "rs485-112233445566778899aabbcc-gas",
        "data": {
          "parent_id": "rs485-112233445566778899aabbcc",
          "address": 5,
          "uid": "112233445566778899aabbcc",
          "type": "MODBUS_GAS",
          "sensor_type": 1,
          "sensor_type_name": "gas",
          "stt": 1,
          "quality": "ok",
          "raw": 2048,
          "millivolts": 1650
        }
      }
    ]
  }
}
```

`stt` bằng `1` khi online và `0` khi offline. `quality` của node có thể là `ok`, `sensor_error`, `bootloader` hoặc `offline`. Cảm biến nhiệt độ/độ ẩm hiện dùng `measurement_unavailable` khi online vì STM32 chưa có driver đo; child gas và CH3 có `raw` và `millivolts`.

Server có thể gửi `deviceUpdateRsp` về `/v3/server/hc/{mac}/jsonres`, nhưng firmware coi đây là phản hồi fire-and-forget và không chờ bản tin này.

## Trạng thái gateway

Khi kết nối cloud, gateway gửi trạng thái `homeController`:

```json
{
  "cmd": "homeController",
  "data": {
    "status": 1,
    "version": "1.0.0",
    "ip": "192.168.1.20"
  }
}
```

Last Will dùng cùng cấu trúc với `status: 0`. Gateway cũng gửi `hcInfo` định kỳ, gồm `version`, `ipLocal`, `nameWifi`, `isHCmini`, `deviceCount` và `maxDevice`. Cả hai được gửi lên `/v3/hc/{mac}/server/jsonreq`.

## Ghi chú

- Các UID và giá trị đo trong ví dụ chỉ để minh họa.
- Telemetry không được lưu hàng đợi khi MQTT mất kết nối; sau khi kết nối lại, lần poll tiếp theo gửi trạng thái mới nhất.
- API hiện tại không dùng `op`, `job_id`, topic `iot/{gateway}/commands`, hoặc địa chỉ ở cấp trên cùng của JSON.

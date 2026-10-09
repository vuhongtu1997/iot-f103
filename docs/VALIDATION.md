# Kiểm tra bản bàn giao

## Build thành công

| Thành phần | Toolchain | Kết quả |
|---|---|---|
| ESP32 gateway | ESP-IDF v5.2.3, Xtensa GCC esp-13.2.0_20230928 | ELF/BIN sinh thành công; image 0x109b20 = 1.088.288 byte, vừa OTA slot 1.572.864 byte |
| STM32 bootloader | GNU Arm Embedded GCC 13.2.1, Cortex-M3 Thumb, -Os | text 5.600 byte, bss 332 byte; vừa vùng 16 KB |
| STM32 application | Cùng toolchain | text 5.152 byte, bss 332 byte; vừa vùng 46 KB |

Build dùng thông số thử nghiệm và khóa demo để xác minh liên kết; không phân phối firmware build này để triển khai. ESP32 build dùng SSID thử khác CHANGE_ME để compiler giữ đầy đủ đường chạy Wi-Fi/MQTT/OTA. Archive chỉ có nguồn và cấu hình mẫu; người dùng sinh khóa, điền cloud rồi tự build. ESP-IDF component manager được tắt khi build trong môi trường kiểm tra vì môi trường không hỗ trợ truy vấn PID mà manager sử dụng; dự án không cần component ngoài SDK.

## Kiểm thử host

Chạy `sh tests/run.sh` trên Linux có GCC và Python 3:

- CRC16/CRC32: vector chuẩn `123456789`.
- SHA256/HMAC: vector công khai và so sánh C với Python tại các biên khối và kích thước firmware.
- Prefix UID: kiểm tra độ dài 0..96, từ chối >96.
- Cấu hình: mô phỏng ngắt giữa từng halfword khi ghi hai trang flash; địa chỉ cũ hoặc mới vẫn hợp lệ.
- OTA chunks: từ chối sai offset, chunk rỗng; chấp nhận retry chunk giống hệt.
- Firmware bị sửa: CRC/HMAC chặn kích hoạt/nạp.
- Mất điện: ngắt khi xóa/ghi flash ứng dụng, nạp lại từ staging và giữ nguyên trang địa chỉ.
- Mất W25Q32 sau nạp dở: vector ứng dụng vẫn trắng.
- Chạy **bus.c thật** với UART mô phỏng: broadcast không có ACK, xác nhận unicast, từ chối ID đã dùng/offline, lỗi lưu trước broadcast không phát lệnh, giữ chỗ khi thiếu xác nhận và phục hồi bằng scan đọc-only.
- Chạy **node.c thật**: chỉ nút chưa cấu hình nhận FC06, nút đã cấu hình giữ ID, chế độ bootloader cũng hỗ trợ, từ chối ID/register sai, mọi broadcast đều im lặng.
- Script đóng gói: xác thực layout HMAC, từ chối bootloader có vector sai và URL HTTP.
- GCC host kiểm tra cú pháp toàn bộ nguồn STM32; GNU Arm build kiểm tra assembler/linker thực.

## Chưa kiểm tra

Chưa nối ESP32, STM32, W25Q32, transceiver RS485 hoặc tài khoản cloud thật. Không có kiểm thử Wi-Fi/MQTT TLS thực, waveform RS485, ADC, OTA xuyên suốt trên board, mất điện vật lý, EMI hoặc nhiều nút ngoài hiện trường.

Mô hình host không mô phỏng flash đang bị brownout, hỏng bit ngẫu nhiên, chênh lệch nguồn hoặc va chạm điện có thể không tạo dữ liệu quan sát được. Build thành công và kiểm thử host không chứng minh hệ thống đã sẵn sàng triển khai sản xuất.

## Thử nghiệm bàn cần làm

1. Một STM32: đọc ADC, UID, địa chỉ và kiểm tra điện áp 3,3 V.
2. Ba STM32 mới: cấp nguồn lần lượt, app chọn ID và chờ completed từng nút; mất nguồn rồi kiểm tra giữ địa chỉ.
3. Nút đã cấu hình vẫn bật: xác minh broadcast tiếp theo không đổi ID. Tắt nguồn nút cũ và kiểm tra ESP32 không tái cấp ID đã lưu NVS.
4. Mất xác nhận sau broadcast: giữ chỗ, cấp lại nguồn đúng nút và phục hồi bằng scan. Khi chuyển từ bản cũ, bật các nút đã cấu hình để scan thu registry trước khi thêm nút mới.
5. Kết nối cloud thật: TLS, ACL, QoS1, mất mạng, timestamp, queue/drop counter.
6. OTA STM32: cập nhật phiên bản 1→2, firmware sai HMAC, ngắt mạng khi truyền, ngắt nguồn lúc copy.
7. OTA ESP32: kiểm tra phiên bản báo lại và rollback khi firmware mới không đạt điều kiện xác nhận.
8. Kiểm tra firmware mới chạy lỗi: STM32 chưa có rollback/boot-confirm, cần kế hoạch phục hồi qua ST-LINK.

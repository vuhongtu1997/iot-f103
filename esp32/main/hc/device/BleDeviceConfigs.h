#ifndef BLE_DEVICE_CONFIGS_H
#define BLE_DEVICE_CONFIGS_H

#include <map>
#include <string>

/**
 * @brief Cấu hình số lượng element của thiết bị BLE dựa trên PID.
 *
 * Mỗi thiết bị BLE Mesh có một PID (Product ID) xác định loại thiết bị.
 * Số lượng element (thành phần) của thiết bị được dùng để tính toán địa chỉ
 * Unicast tiếp theo. Thông thường: total_elements = button_count + 1 (cho
 * configuration element).
 */
struct BleDeviceConfig
{
  std::string modelName;
  int elementCount;
  unsigned int serverTypeId;
};

/**
 * @brief Lấy cấu hình thiết bị BLE dựa trên PID.
 * Sử dụng pattern Singleton (Lazy Initialization) để tránh lỗi khởi tạo sớm
 * và cấp phát heap trước khi hệ thống sẵn sàng.
 */
inline const std::map<std::string, BleDeviceConfig> &getBleDeviceConfigs()
{
  static const std::map<std::string, BleDeviceConfig> configs = {
      {"BLEHGAA0201", {"HG_BLE_LIGHT_WHITE", 1, 11010}},     // Đèn trắng
      {"BLEHG01020D", {"HG_BLE_LIGHT_WHITE", 1, 11011}},
      {"BLEHG01020B", {"HG_BLE_LIGHT_WHITE", 1, 11012}},
      {"BLEHG01020A", {"HG_BLE_LIGHT_WHITE", 1, 11013}},
      {"BLEHG010201", {"HG_BLE_LIGHT_WHITE", 1, 11014}},
      {"BLEHG010202", {"HG_BLE_LIGHT_WHITE", 1, 11015}},
      {"BLEHG010204", {"HG_BLE_LIGHT_WHITE", 1, 11016}},
      {"BLEHG010205", {"HG_BLE_LIGHT_WHITE", 1, 11017}},
      {"BLEHG010206", {"HG_BLE_LIGHT_WHITE", 1, 11018}},
      {"BLEHG010207", {"HG_BLE_LIGHT_WHITE", 1, 11019}},
      {"BLEHGAA0202", {"HG_BLE_LIGHT_RGB", 2, 12006}},
      {"BLEHG010401", {"HG_BLE_LIGHT_RGB", 2, 12007}},
      {"BLEHG010402", {"HG_BLE_LIGHT_RGB", 2, 12008}},
      {"BLEHGAA0101", {"HG_BLE_SWITCH_1", 1, 13005}},        // 1 nút
      {"BLEHGAA0102", {"HG_BLE_SWITCH_2", 2, 14004}},        // 2 nút
      {"BLEHGAA0103", {"HG_BLE_SWITCH_3", 3, 15003}},        // 3 nút
      {"BLEHGAA0104", {"HG_BLE_SWITCH_4", 4, 16003}},        // 4 nút
      {"BLEHGAA0601", {"HG_BLE_SWITCH_1", 1, 13006}},        // Module công tắc 1 kênh
      {"BLEHGAA0602", {"HG_BLE_SWITCH_2", 2, 14005}},        // Module công tắc 2 kênh
      {"BLEHGAA0603", {"HG_BLE_SWITCH_3", 3, 15004}},        // Module công tắc 3 kênh
      {"BLEHGAA010B", {"HG_BLE_SWITCH_SCENE", 4, 56001}},     // Công tắc cảnh 4 nút, 8 DP cấu hình scene
      {"BLEHGAA010C", {"HG_BLE_SWITCH_DIMMING", 1, 18001}},   // Công tắc DIM LED cảm ứng, các DP dùng chung địa chỉ
      {"BLEHGAA010A", {"HG_BLE_SWITCH_DIMMING", 1, 18002}},   // Công tắc DIM LED legacy
      {"BLEHGAA010D", {"HG_BLE_SWITCH_DIMMING", 1, 61001}},   // Công tắc DIM LED cơ, các DP dùng chung địa chỉ
      {"BLEHGAA010E", {"HG_BLE_SWITCH_DIMMING", 1, 61002}},   // Công tắc DIM LED VIP/cơ, các DP dùng chung địa chỉ
      {"BLEHGAA0105", {"HG_BLE_CURTAIN_OLD", 1, 19019}},     // Rèm 1 lớp legacy dùng Generic OnOff
      {"BLEHGAA0501", {"HG_BLE_CURTAIN_NEW", 1, 19020}},     // Rèm ngang
      {"BLEHGAA0107", {"HG_BLE_CURTAIN_DOUBLE", 2, 22001}},  // Rèm đôi đời cũ
      {"BLEHGAA0106", {"HG_BLE_ROLLING_DOOR", 1, 37002}},   // Cửa cuốn
      {"BLEHGAA0109", {"HG_BLE_ROLLING_DOOR_SWITCH", 1, 37003}}, // Công tắc cửa cuốn đời cũ
      {"BLEHGAA0108", {"HG_BLE_STAIR", 3, 17001}},           // Công tắc cầu thang cảm ứng
      {"BLEHGAA0605", {"HG_BLE_ROLLING_DOOR_MODULE", 1, 37004}}, // Module cửa cuốn, điều khiển 0/1/2 trên địa chỉ gốc
      {"BLEHGAA0208", {"HG_BLE_STAIR", 3, 62001}},           // Công tắc cầu thang cơ Luxury
      {"BLEHGAA0209", {"HG_BLE_STAIR", 3, 62002}},           // Công tắc cầu thang cơ VIP
      {"BLEHGAA020F", {"HG_BLE_CURTAIN_DOUBLE", 2, 20003}},   // Công tắc rèm đôi cơ Luxury
      {"BHGRDNS0110", {"HG_BLE_ROLLING_DOOR_NEW", 1, 35001}}, // Công tắc cửa cuốn cơ 3 nút
      {"BHGRDMS0113", {"HG_BLE_ROLLING_DOOR_NEW", 1, 36001}}, // Công tắc cửa cuốn cơ 1 nút
      {"BHGRDNS0118", {"HG_BLE_ROLLING_DOOR_NEW", 1, 35002}}, // Công tắc cửa cuốn VIP 3 nút
      {"BHGRDMS8000", {"HG_BLE_ROLLING_DOOR_NEW", 1, 36002}}, // Công tắc cửa cuốn VIP 1 nút
      {"BLEHGAA020E", {"HG_BLE_ROLLING_DOOR_NEW", 1, 35003}}, // Công tắc cửa cuốn Luxury 3 nút
      {"BLEHGAA0503", {"HG_BLE_ROLLING_DOOR_NEW", 1, 33002}}, // Cửa cổng tự động
      {"BLEHGAA0701", {"HG_BLE_SWITCH_1", 1, 57001}},        // Công tắc cơ 1 nút
      {"BLEHGAA0702", {"HG_BLE_SWITCH_2", 2, 58001}},        // Công tắc cơ 2 nút
      {"BLEHGAA0703", {"HG_BLE_SWITCH_3", 3, 59001}},        // Công tắc cơ 3 nút
      {"BLEHGAA0704", {"HG_BLE_SWITCH_4", 4, 60001}},        // Công tắc cơ 4 nút
      {"BLEHGAA0801", {"HG_BLE_SWITCH_1", 1, 57002}},        // Công tắc VIP 1 nút
      {"BLEHGAA0802", {"HG_BLE_SWITCH_2", 2, 58002}},        // Công tắc VIP 2 nút
      {"BLEHGAA0803", {"HG_BLE_SWITCH_3", 3, 59002}},        // Công tắc VIP 3 nút
      {"BLEHGAA0804", {"HG_BLE_SWITCH_4", 4, 60002}},        // Mặt công tắc cơ 4 Cao cấp
      {"BLEHGAA0901", {"HG_BLE_SWITCH_1", 1, 57003}},        // Công tắc Luxury 1 nút
      {"BLEHGAA0902", {"HG_BLE_SWITCH_2", 2, 58003}},        // Công tắc Luxury 2 nút
      {"BLEHGAA0903", {"HG_BLE_SWITCH_3", 3, 59003}},        // Công tắc Luxury 3 nút
      {"BLEHGAA0904", {"HG_BLE_SWITCH_4", 4, 60003}},        // Mặt công tắc cơ 4 Luxury
      {"BLEHGAA0406", {"RD_BLE_SENSOR_SMOKE", 1, 46006}},    // Cảm biến khói
      {"BLEHG030301", {"RD_BLE_SENSOR_SMOKE", 1, 46005}},    // Cảm biến khói nhỏ
      {"BLEHG030601", {"RD_BLE_SENSOR_DOOR", 1, 42004}},     // Cảm biến cửa nhỏ
      {"BLEHGAA0404", {"RD_BLE_SENSOR_DOOR", 1, 42005}},     // Cảm biến cửa lớn
      {"BLEHGAA0606", {"HG_BLE_SENSOR_PRESENCE", 1, 47004}}, // Cảm biến hiện diện
      {"BLEHG030201", {"HG_BLE_GENERIC", 1, 43006}},  // Cảm biến chuyển động nhỏ
      {"BLEHGAA0401", {"HG_BLE_GENERIC", 1, 43007}},  // Cảm biến chuyển động
      {"BLEHG030801", {"HG_BLE_GENERIC", 1, 44004}},  // Cảm biến môi trường nhỏ
      {"BLEHGAA0405", {"HG_BLE_GENERIC", 1, 44005}},  // Cảm biến môi trường
      {"BLEHGAA0607", {"HG_BLE_GENERIC", 1, 48001}},  // Module cảm biến đa năng
      {"BLEHGCH0101", {"HG_BLE_GENERIC", 1, 38001}},  // Camera HomeGy (chỉ thêm mới/đồng bộ)
      {"BLEHGAA0301", {"HG_BLE_IR", 1, 24010}},              // IR base
      {"BLEHGAA0302", {"HG_BLE_IR", 1, 26006}},              // IR TV
      {"BLEHGAA0303", {"HG_BLE_IR", 1, 27006}},              // IR FAN
      {"BLEHGAA0304", {"HG_BLE_IR", 1, 25009}},              // IR AC
      {"BLEHGAA0305", {"HG_BLE_IR", 1, 28008}},              // IR REMOTE
      {"BLEHGAA0316", {"HG_BLE_IR", 1, 24011}},              // IR V2
      {"BLEHGAA0317", {"HG_BLE_IR", 1, 26007}},              // IR TV V2
      {"BLEHGAA0318", {"HG_BLE_IR", 1, 27008}},              // IR FAN V2
      {"BLEHGAA0319", {"HG_BLE_IR", 1, 25010}},              // IR AC V2
      {"BLEHGAA0320", {"HG_BLE_IR", 1, 28010}},              // IR REMOTE V2
      {"BLEHGAA0321", {"HG_BLE_IR", 1, 24012}},              // IR V2 LAO / voice
      {"BLEHGAA0322", {"HG_BLE_IR", 1, 26008}},              // IR TV V2 LAO
      {"BLEHGAA0323", {"HG_BLE_IR", 1, 27009}},              // IR FAN V2 LAO
      {"BLEHGAA0324", {"HG_BLE_IR", 1, 25011}},              // IR AC V2 LAO
      {"BLEHGAA0325", {"HG_BLE_IR", 1, 28011}},              // IR REMOTE V2 LAO
      {"BLEHGAA0608", {"HG_BLE_CURTAIN_NEW", 1, 23001}},     // Động cơ rèm cuốn mới, 3 DP dùng chung địa chỉ
      {"BLEHGAA0609", {"HG_BLE_CURTAIN_NEW", 1, 19021}},     // Động cơ rèm ngang mới, 3 DP dùng chung địa chỉ
      {"BHGMCS0112", {"HG_BLE_CURTAIN_NEW", 1, 21001}},      // Rèm cơ 1 lớp mới
      {"BHGMCS0111", {"HG_BLE_CURTAIN_DOUBLE", 2, 20001}},    // Công tắc rèm đôi cơ
      {"BHGCS0114", {"HG_BLE_CURTAIN_NEW", 1, 19023}},       // Rèm cảm ứng mới
      {"BHGMDC0115", {"HG_BLE_CURTAIN_NEW", 1, 19024}},      // Module rèm mới
      {"BHGMCS0116", {"HG_BLE_CURTAIN_DOUBLE", 2, 20002}},   // Công tắc rèm đôi cơ VIP
      {"BHGMCS0117", {"HG_BLE_CURTAIN_NEW", 1, 21002}},      // Rèm VIP 1 lớp mới
      {"BLEHGAA020C", {"HG_BLE_CURTAIN_NEW", 1, 21003}},     // Rèm luxury 1 lớp mới
                                                      // Thêm các PID khác ở đây
  };
  return configs;
}

#endif // BLE_DEVICE_CONFIGS_H

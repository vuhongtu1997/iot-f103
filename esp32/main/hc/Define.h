#pragma once

#include "../define.h"

/**
 * @defgroup Defines_Module Macro và cấu hình hệ thống
 * @brief Chứa các định nghĩa model, version, đường dẫn file và cổng UART BLE
 * @{
 */

/**
 * @brief Model thiết bị theo nền tảng.
 *
 * - Trên OpenWRT: `"RD_HC_FULL"`
 * - Trên ESP32/ESP_PLATFORM: `"RD_HC_MINI"`
 * - Trên các nền tảng khác: `"RD_HC_V2"`
 */
#ifndef MODEL
#ifdef __OPENWRT__
#define MODEL "RD_HC_FULL"
#elif defined(ESP_PLATFORM)
#define MODEL "RD_HC_MINI"
#else
#define MODEL "RD_HC"
#endif
#endif

/**
 * @brief Chuyển macro thành chuỗi.
 */
#define STR_(x) #x
#define STR(x) STR_(x)

/**
 * @brief Phiên bản firmware theo nền tảng.
 *
 * - ESP_PLATFORM: trên esp
 * - Các nền tảng khác
 */
#ifndef VERSION
#ifdef ESP_PLATFORM
#define VERSION 1.0.0
#else
#define VERSION 1.0.0
#endif
#endif

/**
 * @brief Loại thiết bị
 */
#ifndef TYPE_DEVICE
#define TYPE_DEVICE "GATEWAY_HC02"
#endif

/**
 * @brief Đường dẫn các file cấu hình, chứng chỉ, log theo nền tảng.
 */
#if defined(__OPENWRT__)
#define CONFIG_FILE_NAME "/root/smh/config.json" /**< File cấu hình JSON */
#define TMP_FOLDER "/tmp/"                       /**< Thư mục tạm thời */
#define CERT_FILE_NAME "/root/smh/server.pem"    /**< File chứng chỉ SSL */
#define LOG_FILE_PATH "/root/smh/"               /**< Thư mục log */
#define LOG_FILE_NAME "smh-"                     /**< Prefix file log */
#else
#define CONFIG_FILE_NAME "config.json"
#define TMP_FOLDER ""
#define CERT_FILE_NAME "./server.pem"
#define LOG_FILE_PATH ""
#define LOG_FILE_NAME "smh-"
#endif

/**
 * @brief Tên file cơ sở dữ liệu SQLite theo nền tảng.
 */
#ifndef DB_NAME
#ifdef __OPENWRT__
#define DB_NAME "/root/smh/smh.sqlite"
#elif defined(ESP_PLATFORM)
#define DB_NAME "/littlefs/smh.sqlite"
#else
#define DB_NAME "./smh.sqlite"
#endif
#endif

/**
 * @brief Cổng UART cho BLE theo nền tảng.
 *
 * - OpenWRT: `/dev/ttyS1`
 * - ESP_PLATFORM: không định nghĩa (ESP32 dùng UART khác)
 * - Nền tảng khác: `/dev/ttyUSB0`
 */
#ifndef BLE_UART_PORT
#ifdef __OPENWRT__
#define BLE_UART_PORT "/dev/ttyS1"
#elif defined(ESP_PLATFORM)
#else
#define BLE_UART_PORT "/dev/ttyUSB0"
#endif
#endif

//define for value of JSON data
#define TYPE_ADD_GW                     (26U)
#define TYPE_FEEDBACK_GATEWAY           (1U)
#define TYPE_ADD_DEVICE                 (2U)
#define TYPE_DEL_DEVICE                 (3U)
#define TYPE_CTR_DEVICE                 (4U)
#define TYPE_ADD_SCENE                  (5U)
#define TYPE_EDIT_SCENE                 (6U)
#define TYPE_DEL_SCENE                  (7U)
#define TYPE_CTR_SCENE                  (8U)
#define TYPE_CTR_SCENE_LC               (8U)
#define TYPE_UPDATE_DEVICE              (9U)
#define TYPE_ADD_GROUP_LIGHT            (12U)   // Tạo nhóm đèn
#define TYPE_DEL_GROUP_LIGHT            (13U)   // Xoá nhóm đèn
#define TYPE_CTR_GROUP_NORMAL           (14U)   
#define TYPE_UPDATE_GROUP_LIGHT         (15U)   // Chỉnh sửa nhóm đèn

//SWITCH
#define TYPE_ADD_GROUP_LINK             (16U)   // Tạo nhóm liên động cho công tắc
#define TYPE_UPDATE_GROUP_LINK          (17U)   // Chỉnh sửa nhóm liên động cho công tắc
#define TYPE_DEL_GROUP_LINK             (18U)   // Xoá nhóm liên động cho công tắc
#define TYPE_GET_DEVICE_HISTORY         (19U)
#define TYPE_DIM_LED_SWITCH             (20U)
#define TYPE_LOCK_KIDS                  (21U)
#define TYPE_LOCK_AGENCY                (22U)
#define TYPE_NOTIFI_REPONSE             (23U)
#define TYPE_PING_HC_FROM_APP           (24U)
#define TYPE_UPDATE_SERVICE             (25U)
#define TYPE_SET_GROUP_TTL              (27U)
#define TYPE_SET_DEVICE_TTL             (28U)
#define TYPE_GET_ALL_DEVICES            (29U)
#define TYPE_REALTIME_STATUS_FB         (31U)
#define TYPE_UPDATE_GATEWAY_INFO        (32U)
#define TYPE_GET_SCENES_OF_DEVICE       (33U)
#define TYPE_GET_GROUPS_OF_DEVICE       (34U)
#define TYPE_SYNC_DEVICE_STATUS         (35U)
#define TYPE_OTA_GATEWAY                (36U)
#define TYPE_OTA_HC                     (37U)
#define TYPE_GET_LOG                    (39U)
#define TYPE_DEL_HOMEKIT                (40U)
#define TYPE_ADD_DIM_SWITCH_LIGHT       (41U)
#define TYPE_UPDATE_TYPE_SCENE_SWITCH   (42U)
#define TYPE_UPDATE_GROUP_DIM_SWITCH_LIGHT       (43U)

#define TYPE_RESET_DATABASE             (44U)
#define TYPE_GET_NUM_OF_PAGE            (45U)
#define TYPE_SYNC_DB_DEVICES            (46U)
#define TYPE_SYNC_DB_SCENES             (47U)
#define TYPE_SYNC_DB_GROUPS             (48U)
#define TYPE_UPDATE_LANGUAGE            (49U)

#define TYPE_SYNC_DEVICE_STATE          (99U)
#define TYPE_GET_ONOFF_STATE            (100U)
#define TYPE_UPDATE_NOTI_CONF           (101U)
#define TYPE_DELETE_ID_INDEX_IR_V2      (110U)
#define TYPE_DELETE_CODE_STATIC_IR_V2   (113U)

///////////////////////Define type repons from Devices///////////////////////////////////////
#define GW_RESPONSE_UNKNOW               (-1)
#define GW_RESP_ONOFF_STATE              (50U)
#define GW_RESPONSE_DEVICE_KICKOUT       (51U)
#define GW_RESPONSE_SMOKE_SENSOR         (52U)
#define GW_RESPONSE_SENSOR_ENVIRONMENT   (53U)
#define GW_RESP_ONLINE_STATE             (54U)
#define GW_RESPONSE_SENSOR_BATTERY       (55U)
#define GW_RESPONSE_SENSOR_PIR_DETECT    (56U)
#define GW_RESPONSE_SENSOR_PIR_LIGHT     (57U)
#define GW_RESPONSE_SENSOR_DOOR_DETECT   (58U)
#define GW_RESPONSE_SENSOR_DOOR_ALARM    (59U)
#define GW_RESPONSE_GROUP                (60U)
#define GW_RESPONSE_LIGHT_RD_CONTROL     (61U)
#define GW_RESPONSE_RGB_COLOR            (86U)

#define GW_RESPONSE_SENSOR_ENVIRONMENT_TEMPER           (62U)
#define GW_RESPONSE_SENSOR_ENVIRONMENT_HUMIDITY         (63U)

#define GW_RESPONSE_ACTIVE_DEVICE_HG_RD_SUCCESS         (64U)
#define GW_RESPONSE_SAVE_GATEWAY_HG                     (65U)
#define GW_RESPONSE_ACTIVE_DEVICE_HG_RD_UNSUCCESS       (66U)
#define GW_RESPONSE_SAVE_GATEWAY_RD                     (67U)

#define GW_RESPONSE_ADD_DEVICE                          (68U)
#define GW_RESPONSE_ADD_SCENE_HC                        (69U)
#define GW_RESPONSE_DEL_SCENE_HC                        (70U)
#define GW_RESPONSE_ADD_SCENE                           (71U)
#define GW_RESPONSE_SCENE_LC_CALL_FROM_DEVICE           (72U)
#define GW_RESPONSE_ADD_SCENE_LC                        (73U)
#define GW_RESPONSE_DEL_SCENE_LC                        (74U)
#define GW_RESPONSE_DIM_LED_SWITCH_HOMEGY               (75U)
#define GW_RESPONSE_ADD_GROUP_NORMAL                    (76U)
#define GW_RESPONSE_DEL_GROUP_NORMAL                    (77U)
#define GW_RESPONSE_ADD_GROUP_LINK                      (78U)
#define GW_RESPONSE_DEL_GROUP_LINK                      (79U)
#define GW_RESPONSE_SET_TIME_SENSOR_PIR                 (80U)
#define GW_RESPONSE_UPDATE_SCENE                        (81U)
#define GW_RESPONSE_UPDATE_GROUP                        (82U)
#define GW_RESPONSE_SET_TTL                             (83U)
#define GW_RESPONSE_IR                                  (84U)
#define CAM_HANET_RESPONSE                              (85U)
#define GW_RESPONSE_LOCK_KIDS                           (87U)
#define GW_RESPONSE_SENSOR_PRESENCE                     (88U)
#define GW_RESPONSE_FORWARD_ONLY                        (89U)
#define GW_RESP_MODULE                                  (90U)
#define GW_RESP_NEW_CURTAIN                             (91U)
#define GW_RESP_SWICH_SCENE                             (92U)
#define GW_RESP_UPDATE_TYPE_SCENE_SWITCH                (93U)
#define GW_RESP_SET_TIME_PROCESS_CURTAIN                (94U)
#define GW_RESP_SET_TYPE_CURTAIN                        (95U)
#define GW_RESP_SET_DIRECT_CURTAIN                      (96U)
#define GW_RESPONSE_ADD_GROUP_SWICH_SCENE               (97U)
#define GW_RESPONSE_LIGHT_SCENE_SWITCH                  (98U)
#define GW_RESPONSE_NOTIFI                              (111U)

/** @} */ // Kết thúc nhóm Defines_Module

/**
 * @file ButtonSignal.h
 * @brief Lớp quản lý trạng thái nút bấm và điều khiển hành vi LED/AP
 *
 * File này khai báo lớp `ButtonSignal` dùng để bắt các sự kiện nhấn/thả nút bấm,
 * điều khiển LED hiển thị trạng thái dịch vụ/internet và thực thi một số hành vi
 * đặc biệt như chuyển sang chế độ Access Point (AP), reset factory, hoặc pair mode.
 *
 * Lớp được triển khai theo mẫu Singleton, cho phép truy cập thống nhất từ nhiều phần
 * trong hệ thống. Sử dụng kết hợp với FreeRTOS task và ISR để xử lý sự kiện real-time.
 *
 */

#pragma once

#include "../define.h"

//======================================================================
// Định nghĩa GPIO liên quan đến nút bấm và LED
//======================================================================

/** @brief GPIO đầu vào (nút bấm vật lý) */
#define GPIO_INPUT_IO_0 BTN
/** @brief Bit mask chọn GPIO input */
#define GPIO_INPUT_PIN_SEL (1ULL << GPIO_INPUT_IO_0)

/** @brief Cờ mặc định cho interrupt của ESP-IDF */
#define ESP_INTR_FLAG_DEFAULT 0

//======================================================================
// Khai báo lớp ButtonSignal
//======================================================================

/**
 * @class ButtonSignal
 * @brief Lớp xử lý sự kiện nút bấm và hành vi LED/AP liên quan.
 *
 * Lớp này cung cấp các chức năng:
 * - Theo dõi trạng thái nhấn/thả của nút (`isPressed`).
 * - Tạo và quản lý FreeRTOS task để xử lý logic giữ, double click, v.v.
 * - Điều khiển LED hiển thị trạng thái dịch vụ.
 * - Cung cấp API công khai: `Init()`, `OnPress()`, `OnRelease()`, `GetStatus()`.
 *
 * @note Lớp sử dụng mẫu Singleton để đảm bảo chỉ có một thể hiện duy nhất trong hệ thống.
 */
class ButtonSignal
{
public:
    /**
     * @brief Cờ trạng thái nút hiện tại (true = đang nhấn).
     *
     * Được khai báo là volatile vì có thể thay đổi trong ISR hoặc task song song.
     */
    volatile bool isPressed;
    volatile bool isConfigMode;
    volatile bool isConfigHcMode;
    volatile bool isFactoryResetting;

    /**
     * @brief Lấy thể hiện Singleton của lớp ButtonSignal.
     *
     * @return Con trỏ tới thể hiện duy nhất của `ButtonSignal`.
     */
    static ButtonSignal *getInstance();

    /**
     * @brief Hàm khởi tạo.
     *
     * Không thực hiện công việc nặng tại đây — thay vào đó,
     * nên gọi `Init()` để khởi tạo task và cấu hình GPIO.
     */
    ButtonSignal();

    /**
     * @brief Hàm hủy đối tượng.
     *
     * Hiện tại không thực hiện thao tác đặc biệt nào.
     */
    ~ButtonSignal();

    /**
     * @brief Khởi tạo task xử lý nút và cấu hình GPIO liên quan.
     *
     * Gọi từ luồng chính (main task) để tạo FreeRTOS task xử lý nút nhấn.
     */
    void Init();

    /**
     * @brief Xử lý khi nút được nhấn.
     *
     * Được gọi từ ISR hoặc task GPIO để đánh dấu trạng thái nhấn
     * và khởi đầu quá trình xử lý hành vi.
     */
    void OnPress();

    /**
     * @brief Xử lý khi nút được thả.
     *
     * Được gọi từ ISR hoặc task GPIO để xác định hành vi người dùng,
     * như click đơn, click đúp, hoặc nhấn giữ.
     */
    void OnRelease();

    /**
     * @brief Lấy trạng thái hiện tại của nút.
     *
     * @return `true` nếu nút đang được nhấn, `false` nếu không.
     */
    bool GetStatus();

    /**
     * @brief Kiểm tra yêu cầu vào config mode trong cửa sổ boot.
     *
     * Hành vi được port từ branch root:
     * - chờ tối đa 2 giây đầu boot để phát hiện người dùng bắt đầu nhấn nút
     * - nếu phát hiện nhấn, yêu cầu giữ liên tục 5 giây để vào config mode
     */
    bool CheckBootConfigRequest();

    /**
     * @brief Đưa hệ thống vào config mode.
     */
    void EnterConfigMode();

    /**
     * @brief Đánh dấu đang xử lý cấu hình HC để LED nháy nhanh hơn.
     */
    void SetConfigHcMode(bool enabled);

    /**
     * @brief Kết thúc config mode sau khi HC kết nối cloud thành công.
     */
    void CompleteConfigMode();

    /**
     * @brief Kiểm tra có đang ở config mode hay không.
     */
    bool IsConfigMode() const;

    /**
     * @brief Kiểm tra timeout của chuỗi nhấn factory reset.
     */
    void PollFactoryResetSequence();

    /**
     * @brief Bắt đầu quy trình factory reset.
     */
    void StartFactoryReset();

private:
    int factoryResetPressCount;
    int64_t factoryResetWindowStartUs;
};

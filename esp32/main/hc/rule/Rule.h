/**
 * @file Rule.h
 * @brief Định nghĩa lớp Rule và các kiểu enum liên quan cho hệ thống automation.
 *
 * Lớp Rule quản lý các luật (rule) trong hệ thống, bao gồm đầu vào (RuleInput),
 * đầu ra (RuleOutput), thời gian, chế độ hoạt động, trạng thái bật/tắt, và thông tin khác.
 */

#pragma once

#include <string>
#include <vector>
#include <unistd.h>
#include "Object.h"
#include "RuleInput.h"
#include "RuleOutput.h"
#include "json.h"

/**
 * @enum RuleType
 * @brief Các loại rule được hỗ trợ.
 */
typedef enum
{
	RULE_TYPE_TAP_TO_RUN = -2, /**< Rule kích hoạt bằng thao tác nhấn (Tap to Run) */
	RULE_TYPE_TIME = -1,	   /**< Rule dựa trên thời gian */
	RULE_TYPE_OR = 0,			   /**< Rule kiểu AND giữa các điều kiện đầu vào */
	RULE_TYPE_TIME_OR,		   /**< Rule kiểu OR với giới hạn thời gian */
	RULE_TYPE_TIME_AND		   /**< Rule kiểu AND với giới hạn thời gian */
} RuleType;

/**
 * @enum RuleMode
 * @brief Các chế độ hoạt động của Rule theo ngày.
 */
typedef enum
{
	RULE_MODE_ALL_DAY = 0xFE,	 /**< Rule hoạt động cả ngày */
	RULE_MODE_WORK_DAY = 0xF1,	 /**< Rule hoạt động trong ngày làm việc */
	RULE_MODE_WEEKEND_DAY = 0x06 /**< Rule hoạt động vào cuối tuần */
} RuleMode;

/**
 * @class Rule
 * @brief Lớp quản lý một Rule trong hệ thống automation.
 *
 * Rule gồm danh sách các RuleInput (điều kiện đầu vào) và RuleOutput (hành động đầu ra),
 * thông tin thời gian, chế độ hoạt động, trạng thái bật/tắt và các thuộc tính khác.
 */
class Rule : public Object
{
private:
	Json::Value ruleData; /**< Dữ liệu rule dưới dạng JSON */
	RuleType type;		  /**< Kiểu rule */
	std::string name;	  /**< Tên rule */

	unsigned char repeater; /**< Số lần lặp lại */
	bool fullDay;			/**< Rule hoạt động cả ngày hay không */
	int startTime;			/**< Thời gian bắt đầu (nếu có) */
	int endTime;			/**< Thời gian kết thúc (nếu có) */

	bool isAvailable;		/**< Rule có khả dụng không */
	bool isEnable;			/**< Rule đang bật hay tắt */
	bool isFavorite;		/**< Rule có được đánh dấu yêu thích không */
	int timerRegisterIndex; /**< Chỉ số đăng ký timer (nếu có) */

	std::vector<RuleInput *> ruleInputList;	  /**< Danh sách đầu vào của rule */
	std::vector<RuleOutput *> ruleOutputList; /**< Danh sách đầu ra của rule */

public:
	/**
	 * @brief Constructor khởi tạo Rule cơ bản.
	 */
	Rule(std::string id, RuleType type, unsigned char repeater, std::string name, uint16_t addr, Json::Value &ruleData);

	/**
	 * @brief Constructor khởi tạo Rule với khoảng thời gian.
	 */
	Rule(std::string id, RuleType type, unsigned char repeater, std::string name, uint16_t addr, int startTime, int endTime, Json::Value &ruleData, bool isRegisterTimer);

	/**
	 * @brief Destructor.
	 */
	~Rule();

	/**
	 * @brief Lấy dữ liệu JSON của rule.
	 */
	Json::Value GetRuleData();

	/**
	 * @brief Thiết lập dữ liệu JSON cho rule.
	 */
	void SetRuleData(Json::Value ruleData);

	/**
	 * @brief Lấy kiểu rule.
	 */
	RuleType GetType();

	/**
	 * @brief Thêm RuleInput vào rule.
	 */
	void AddRuleInput(RuleInput *ruleInput);

	/**
	 * @brief Thêm RuleOutput vào rule.
	 */
	void AddRuleOutput(RuleOutput *ruleOutput);

	/**
	 * @brief Xóa tất cả RuleInput của rule.
	 */
	void DelAllRuleInput();

	/**
	 * @brief Xóa tất cả RuleOutput của rule.
	 */
	void DelAllRuleOutput();

	/**
	 * @brief Kiểm tra các điều kiện đầu vào của rule.
	 */
	void Check();

	/**
	 * @brief Thực thi các hành động đầu ra khi rule được kích hoạt.
	 */
	void RunOutput();

	/**
	 * @brief Cập nhật dữ liệu rule với thông tin cơ bản.
	 */
	void UpdateData(RuleType type, unsigned char repeater, std::string name, uint16_t addr, Json::Value &ruleData);

	/**
	 * @brief Cập nhật dữ liệu rule với thông tin cơ bản và khoảng thời gian.
	 */
	void UpdateData(RuleType type, unsigned char repeater, std::string name, uint16_t addr, int startTime, int endTime, Json::Value &ruleData);

	/**
	 * @brief Cập nhật dữ liệu rule từ JSON.
	 */
	void UpdateData(Json::Value &ruleData);

	/**
	 * @brief Lấy trạng thái bật/tắt của rule.
	 */
	bool GetStatus();

	/**
	 * @brief Thiết lập trạng thái bật/tắt của rule.
	 */
	void SetStatus(bool enable);

	/**
	 * @brief Lấy trạng thái yêu thích của rule.
	 */
	bool GetIsFavorite();

	/**
	 * @brief Thiết lập trạng thái yêu thích của rule.
	 */
	void SetIsFavorite(bool isFavorite);
};

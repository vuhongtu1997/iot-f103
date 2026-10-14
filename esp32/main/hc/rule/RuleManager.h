/**
 * @file RuleManager.h
 * @brief Quản lý tất cả các Rule trong hệ thống.
 *
 * RuleManager lưu trữ, thêm, xóa và kiểm tra trạng thái của các Rule.
 * Hỗ trợ danh sách rule cần kiểm tra (queue) để tối ưu hiệu năng.
 */

#pragma once

#include <map>
#include <mutex>
#include <thread>
#include <functional>
#include "Rule.h"

#define MAX_RULE_IN_QUEUE_TO_CHECK 10 /**< Số lượng rule tối đa trong danh sách cần kiểm tra */

/**
 * @class RuleManager
 * @brief Quản lý tất cả các Rule trong hệ thống.
 *
 * RuleManager là singleton, chịu trách nhiệm:
 * - Thêm/xóa Rule
 * - Truy xuất Rule theo ID
 * - Thực thi kiểm tra Rule định kỳ hoặc theo yêu cầu
 */
class RuleManager
{
private:
	std::map<std::string, Rule *> ruleList;	 /**< Danh sách tất cả Rule, key là ID rule */
	std::mutex ruleListMtx;					 /**< Mutex bảo vệ ruleList */
	std::vector<Rule *> ruleNeedToCheckList; /**< Danh sách các Rule cần kiểm tra */
	std::mutex ruleNeedToCheckListMtx;		 /**< Mutex bảo vệ ruleNeedToCheckList */

	/**
	 * @brief Constructor private để đảm bảo singleton.
	 */
	RuleManager();

public:
	/**
	 * @brief Lấy instance singleton của RuleManager.
	 * @return Con trỏ tới RuleManager.
	 */
	static RuleManager *getInstance();

	/**
	 * @brief Duyệt qua tất cả Rule và thực hiện một hàm callback.
	 * @param func Hàm callback nhận tham số là con trỏ Rule.
	 */
	void ForEach(std::function<void(Rule *)> func);

	/**
	 * @brief Thêm một Rule mới từ dữ liệu JSON.
	 * @param ruleValue Dữ liệu JSON mô tả Rule.
	 * @return Con trỏ tới Rule vừa thêm.
	 */
	Rule *AddRule(Json::Value &ruleValue);

	/**
	 * @brief Lấy Rule từ ID.
	 * @param id ID của Rule cần lấy.
	 * @return Con trỏ tới Rule nếu tồn tại, nullptr nếu không có.
	 */
	Rule *GetRuleFromId(std::string id);

	/**
	 * @brief Xóa một Rule cụ thể.
	 * @param rule Con trỏ tới Rule cần xóa.
	 */
	void DelRule(Rule *rule);

	/**
	 * @brief Xóa tất cả Rule.
	 */
	void DelAllRule();

	/**
	 * @brief Thực thi task kiểm tra tất cả Rule trong danh sách cần kiểm tra.
	 */
	void CheckRuleTask();

	/**
	 * @brief Kiểm tra trạng thái một Rule.
	 * @param rule Con trỏ tới Rule cần kiểm tra.
	 * @return Giá trị boolean hoặc số trạng thái (tùy cài đặt CheckRule).
	 */
	int CheckRule(Rule *rule);
};

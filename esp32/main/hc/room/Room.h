/**
 * @file Room.h
 * @brief Định nghĩa lớp Room — đại diện cho một phòng trong hệ thống SmartHome.
 *
 * Lớp này kế thừa từ Group, mở rộng thêm các thành phần như danh sách Scene, Group và Rule.
 * Mỗi Room có thể chứa nhiều Group (thiết bị hoặc nhóm thiết bị con), Scene (ngữ cảnh hoạt động)
 * và Rule (quy tắc tự động hóa). Ngoài ra, lớp còn cung cấp các hàm thread-safe để thêm, xóa và kiểm tra các phần tử.
 *
 * @note Các phương thức có sử dụng mutex để đảm bảo an toàn luồng.
 */

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "json.h"
#include "Object.h"
#include "Device.h"
#include "Group.h"
#include "Scene.h"
#include "Rule.h"

/**
 * @class Room
 * @brief Lớp đại diện cho một phòng (Room) trong hệ thống.
 *
 * Mỗi phòng có danh sách Group, Scene và Rule riêng, được quản lý độc lập và thread-safe.
 * Dữ liệu cấu hình phòng được lưu dưới dạng chuỗi JSON.
 */
class Room : public Group
{
private:
	std::string dataConfig; ///< Dữ liệu cấu hình của phòng (định dạng JSON).
	std::mutex mtxGroup;	///< Mutex bảo vệ thao tác với groupList.
	std::mutex mtxScene;	///< Mutex bảo vệ thao tác với sceneList.
	std::mutex mtxRule;		///< Mutex bảo vệ thao tác với ruleList.

public:
	std::vector<Scene *> sceneList; ///< Danh sách các scene thuộc phòng.
	std::vector<Group *> groupList; ///< Danh sách các group (thiết bị hoặc nhóm con) thuộc phòng.
	std::vector<Rule *> ruleList;	///< Danh sách các rule (quy tắc tự động hóa) của phòng.

	/**
	 * @brief Khởi tạo một đối tượng Room.
	 *
	 * @param id     Mã định danh duy nhất của phòng.
	 * @param addr   Địa chỉ định danh trong hệ thống.
	 * @param name   Tên của phòng.
	 */
	Room(std::string id, uint16_t addr, std::string name);

	/**
	 * @brief Destructor — giải phóng bộ nhớ của các đối tượng con nếu cần.
	 */
	~Room();

	/**
	 * @brief Lấy vị trí của một Group trong danh sách.
	 * @param group Con trỏ đến Group cần tìm.
	 * @return Vị trí trong danh sách hoặc -1 nếu không tìm thấy.
	 */
	int GetPositionGroup(Group *group);

	/**
	 * @brief Lấy vị trí của một Scene trong danh sách.
	 * @param scene Con trỏ đến Scene cần tìm.
	 * @return Vị trí trong danh sách hoặc -1 nếu không tồn tại.
	 */
	int GetPositionScene(Scene *scene);

	/**
	 * @brief Lấy vị trí của một Rule trong danh sách.
	 * @param rule Con trỏ đến Rule cần tìm.
	 * @return Vị trí trong danh sách hoặc -1 nếu không tồn tại.
	 */
	int GetPositionRule(Rule *rule);

	/**
	 * @brief Thêm một Group vào phòng.
	 * @param group Con trỏ đến Group cần thêm.
	 * @return 0 nếu thành công, -1 nếu đã tồn tại.
	 */
	int AddGroup(Group *group);

	/**
	 * @brief Xóa một Group khỏi phòng.
	 * @param group Con trỏ đến Group cần xóa.
	 * @return 0 nếu thành công, -1 nếu không tìm thấy.
	 */
	int DelGroup(Group *group);

	/**
	 * @brief Thêm một Scene vào phòng.
	 * @param scene Con trỏ đến Scene cần thêm.
	 * @return 0 nếu thành công, -1 nếu đã tồn tại.
	 */
	int AddScene(Scene *scene);

	/**
	 * @brief Xóa một Scene khỏi phòng.
	 * @param scene Con trỏ đến Scene cần xóa.
	 * @return 0 nếu thành công, -1 nếu không tìm thấy.
	 */
	int DelScene(Scene *scene);

	/**
	 * @brief Thêm một Rule vào phòng.
	 * @param rule Con trỏ đến Rule cần thêm.
	 * @return 0 nếu thành công, -1 nếu đã tồn tại.
	 */
	int AddRule(Rule *rule);

	/**
	 * @brief Xóa một Rule khỏi phòng.
	 * @param rule Con trỏ đến Rule cần xóa.
	 * @return 0 nếu thành công, -1 nếu không tìm thấy.
	 */
	int DelRule(Rule *rule);

	/**
	 * @brief Kiểm tra xem Group có thuộc phòng không.
	 * @param group Con trỏ đến Group cần kiểm tra.
	 * @return true nếu Group thuộc phòng, false nếu không.
	 */
	bool IsGroupInRoom(Group *group);

	/**
	 * @brief Kiểm tra xem Scene có thuộc phòng không.
	 * @param scene Con trỏ đến Scene cần kiểm tra.
	 * @return true nếu Scene thuộc phòng, false nếu không.
	 */
	bool IsSceneInRoom(Scene *scene);

	/**
	 * @brief Lấy dữ liệu cấu hình của phòng.
	 * @return Chuỗi JSON chứa dữ liệu cấu hình.
	 */
	std::string GetDataConfig();

	/**
	 * @brief Cập nhật dữ liệu cấu hình của phòng.
	 * @param dataConfig Chuỗi JSON chứa cấu hình mới.
	 */
	void SetDataConfig(std::string dataConfig);
};

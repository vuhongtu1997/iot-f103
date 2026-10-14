/**
 * @file RuleOutputScene.h
 * @brief Định nghĩa lớp RuleOutputScene — lớp đầu ra (output) để điều khiển Scene.
 *
 * RuleOutputScene triển khai RuleOutput cho phép thực hiện hành động lên một Scene
 * khi Rule được kích hoạt. Dữ liệu JSON có thể lưu trữ các thông số hành động.
 */

#pragma once

#include "json.h"
#include "Scene.h"
#include "RuleOutput.h"

/**
 * @class RuleOutputScene
 * @brief Lớp đại diện cho đầu ra Scene của một Rule.
 *
 * Khi Rule được kích hoạt, RuleOutputScene sẽ chạy hành động tương ứng trên Scene
 * đã được liên kết.
 */
class RuleOutputScene : public RuleOutput
{
private:
	Scene *scene;	  ///< Con trỏ tới Scene mà đầu ra sẽ điều khiển.
	Json::Value data; ///< Dữ liệu cấu hình hành động (tùy chọn).

public:
	/**
	 * @brief Constructor.
	 * @param scene Con trỏ tới Scene sẽ điều khiển.
	 */
	RuleOutputScene(Scene *scene);

	/**
	 * @brief Destructor.
	 */
	~RuleOutputScene();

	/**
	 * @brief Thực thi hành động của Rule trên Scene.
	 *
	 * Khi Rule được kích hoạt, phương thức này sẽ áp dụng dữ liệu action lên Scene.
	 */
	void RunOutput();
};

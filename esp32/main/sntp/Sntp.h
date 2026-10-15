/**
 * @file Sntp.h
 * @brief Cung cấp các hàm để đồng bộ time.
 *
 * File này bao gồm các hàm để đồng bộ time và check trạng thái đồng bộ.
 */
#pragma once

/**
 * @namespace Sntp
 */
namespace Sntp
{
	/**
	 * @brief Khởi tạo đồng bộ thời gian.
	 */
	void init(bool waitNetwork);

	/**
	 * @brief Check trạng thái đồng bộ
	 */
	bool haveNtpTime(void);
	bool haveTime(void);
}

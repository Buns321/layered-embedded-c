//
// 由 Buns_ 创建于 2026/10/26。
//

/**
 * @file app_car.h
 * @brief F407 四轮全向小车应用接口。
 * @addtogroup app
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_APP_CAR_H
#define LAYERED_EMBEDDED_C_APP_CAR_H

#include "app_car_dependencies.h"
#include "app_car_runtime.h"
#include <stdbool.h>

/**
 * @brief 初始化小车 App 状态并保存逻辑服务依赖。
 * @param dependencies Core/Board 填写的依赖集合，不得为 NULL。
 * @retval true App 已初始化，或已有有效初始化状态。
 * @retval false 依赖缺失或必选服务为空。
 * @pre Core_Board_Init 已成功完成。
 * @pre RTT 日志后端已按构建选项完成初始化；首次初始化须在 RTOS 调度器启动前完成。
 * @post 成功时进入 RATE_CONFIG 状态，四轮保持禁用。
 * @note 不初始化具体硬件，不分配动态内存；重复调用保留已有状态。
 */
bool App_Car_Init(const app_car_dependencies_t *dependencies);

/** @} */

#endif // LAYERED_EMBEDDED_C_APP_CAR_H

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

#include "platform_def.h"
#include <stdint.h>

/**
 * @brief 初始化并静态装配小车全部设备。
 * @retval PLATFORM_STATUS_OK 初始化成功。
 * @retval PLATFORM_STATUS_ERROR 任一设备初始化失败。
 * @pre CubeMX 已初始化 TIM1、TIM2–TIM5、USART1–USART3 和 GPIO。
 * @pre RTT 日志后端已按构建选项完成初始化。
 * @note 只允许在 RTOS 调度器启动前调用一次。
 * @note 初始化完成后电机仍保持禁用，等待 WHT101 归零和有效 JoyCon 命令。
 */
platform_status_t App_Car_Init(void);

/**
 * @brief 推进 WHT101、JoyCon 和上电归零状态机。
 * @param now_ms 当前单调毫秒时间戳。
 * @pre 只允许 UARTTask 调用。
 * @note 配置、静止确认和传感器归零完成前不会使能电机。
 */
void App_Car_ProcessInputs(uint32_t now_ms);

/**
 * @brief 运行一次 100 Hz 底盘与四轮闭环控制。
 * @param now_ms 当前单调毫秒时间戳。
 * @pre 只允许 ControlTask 调用，且调用周期与 APP_CAR_CONTROL_PERIOD_S 一致。
 * @note JoyCon 超时、IMU 失效、控制器错误或故障锁存时执行制动或急停。
 */
void App_Car_ProcessControl(uint32_t now_ms);

/**
 * @brief 运行 50 Hz JustFloat 遥测、状态日志和任务心跳监控。
 * @param now_ms 当前单调毫秒时间戳。
 * @pre 只允许 MonitorTask 调用。
 */
void App_Car_ProcessMonitor(uint32_t now_ms);

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_CAR_H

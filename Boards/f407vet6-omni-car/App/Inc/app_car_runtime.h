/**
 * @file app_car_runtime.h
 * @brief F407 四轮小车按任务上下文划分的运行入口。
 * @addtogroup app
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_APP_CAR_RUNTIME_H
#define LAYERED_EMBEDDED_C_APP_CAR_RUNTIME_H

#include <stdint.h>

/**
 * @brief 推进 IMU、输入设备和上电归零状态机。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 只允许 UARTTask 调用，Core_Board_Init 和 App_Car_Init 已成功返回。
 * @post 已处理 IMU 帧、输入帧及启动状态机。
 * @note 不分配内存，不阻塞 RTOS 队列。
 */
void App_Car_ProcessInputs(uint32_t now_ms);

/**
 * @brief 运行一次 100 Hz 底盘与四轮闭环控制。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 只允许 ControlTask 调用。
 * @post 已推进安全判断、输入映射、底盘解算和四轮闭环。
 * @note 不分配内存，不阻塞等待。
 */
void App_Car_ProcessControl(uint32_t now_ms);

/**
 * @brief 运行一次监控、遥测和状态日志步骤。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 只允许 MonitorTask 调用。
 * @post 已检查控制心跳并按周期提交遥测和日志。
 * @note 不分配内存，不等待 DMA。
 */
void App_Car_ProcessMonitor(uint32_t now_ms);

/** @} */

#endif // LAYERED_EMBEDDED_C_APP_CAR_RUNTIME_H

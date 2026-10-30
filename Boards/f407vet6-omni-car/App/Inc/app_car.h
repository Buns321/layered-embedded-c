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
 * @retval PLATFORM_STATUS_OK 设备、策略、底盘和遥测已完成静态装配；重复调用直接返回 OK。
 * @retval PLATFORM_STATUS_ERROR JoyCon 接收启动或任一底层设备装配失败。
 * @retval PLATFORM_STATUS_BUSY 某个物理 UART 的回调槽已被其他所有者占用。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 底层实例或静态配置不合法。
 * @pre CubeMX 已初始化 TIM1、TIM2–TIM5、USART1–USART3、GPIO 和 NVIC。
 * @pre RTT 日志后端已按构建选项完成初始化；首次初始化须在 RTOS 调度器启动前完成。
 * @post 成功时启动 WHT101 10 Hz 输出配置并进入 RATE_CONFIG 状态，四轮保持禁用。
 * @post 失败时模块保持未初始化并返回底层错误，调用方不得继续启动控制任务。
 * @note 不分配动态内存；初始化包含同步 UART/驱动操作，重复调用不会重新装配。
 */
platform_status_t App_Car_Init(void);

/**
 * @brief 推进 WHT101、JoyCon 和上电归零状态机。
 * @param now_ms 当前单调毫秒时间戳，必须与 ControlTask/MonitorTask 共用同一回绕时基。
 * @pre 只允许 UARTTask 在 2 ms 周期上下文中调用，且 App_Car_Init 已成功返回。
 * @post 已处理 WHT101 帧、JoyCon 帧及速率配置/静止确认/归零验证状态机。
 * @post 任一阶段失败会锁存故障，并在下一次调用进入 FAULT；成功归零后进入 READY，但本函数不会使能电机。
 * @note 未初始化时直接返回；任务上下文，不阻塞 RTOS 队列，不分配内存。
 */
void App_Car_ProcessInputs(uint32_t now_ms);

/**
 * @brief 运行一次 100 Hz 底盘与四轮闭环控制。
 * @param now_ms 当前单调毫秒时间戳，同时用于心跳、命令和 IMU 时效判断。
 * @pre 只允许 ControlTask 调用，调用周期必须为 10 ms 且与 APP_CAR_CONTROL_PERIOD_S 一致。
 * @post 每次调用先刷新控制心跳；故障或未就绪时制动，无有效 JoyCon/IMU 或进入死区时制动。
 * @post 有效输入时启用四轮、更新底盘与 PID，并在临界区发布统一命令快照；控制器错误触发锁存急停。
 * @note 任务上下文；不分配内存，不阻塞等待，底盘/PID 输出最终由执行器限幅。
 */
void App_Car_ProcessControl(uint32_t now_ms);

/**
 * @brief 运行 100 Hz 监控循环：50 Hz JustFloat 遥测、1 Hz 状态日志和任务心跳检查。
 * @param now_ms 当前单调毫秒时间戳，用于心跳超时、20 ms 遥测和 1 s 日志节流。
 * @pre 只允许 MonitorTask 在 10 ms 周期上下文中调用，且 App_Car_Init 已成功返回。
 * @post 控制心跳超时锁存故障并执行急停；已有故障时继续请求急停；未初始化时直接返回。
 * @post 首次 ControlTask 刷新心跳前不判定超时；此时四轮尚未由控制路径使能。
 * @post 每 20 ms 发起一次非阻塞 16 通道 JustFloat，每 1000 ms 输出一次 RTT 状态日志。
 * @note 不分配内存；遥测忙时整帧丢弃，不等待 DMA。
 */
void App_Car_ProcessMonitor(uint32_t now_ms);

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_CAR_H

/**
 * @file app_car.h
 * @brief F407 四轮全向小车应用层启动与周期步进接口。
 * @details 本文件仅暴露纯业务生命周期接口，依托 CAR_OMNI_APP_VIEW=1 与
 *          底层 HAL、BSP 及 FreeRTOS 保持单向隔离。
 *
 * @defgroup User_App 应用层入口
 * @{
 */

#ifndef LAYERED_CAR_USER_APP_CAR_H
#define LAYERED_CAR_USER_APP_CAR_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 启动板级基础服务并完成整车门面初始化。
 * @details 依次调用 Board_BootServices() 与 Board_InitCar()，并将返回的
 *          静态整车实例指针缓存于应用层内部句柄。
 * @return 初始化成功与否。
 * @retval true  板级服务与整车静态装配全部就绪。
 * @retval false 板级服务启动失败或整车初始化返回 NULL。
 * @pre 须在 CubeMX 外设初始化（MX_*_Init）完成后、osKernelStart() 启动前调用。
 * @note 内部已自动向 RTOS 安全钩子注册紧急断电回调。
 */
bool App_Car_Boot(void);

/**
 * @brief 推进 IMU 与遥控器输入解析。
 * @param[in] now_ms 当前系统单调毫秒时间戳（单位：ms）。
 * @pre 须在 App_Car_Boot() 成功返回后由输入周期任务调用。
 * @note 当前由 s_input_task 以 2 ms（500 Hz）周期驱动，负责喂狗与数据更新。
 */
void App_Car_StepInput(uint32_t now_ms);

/**
 * @brief 读取遥控期望速度并运行底盘四轮闭环控制。
 * @param[in] now_ms 当前系统单调毫秒时间戳（单位：ms）。
 * @pre 须在 App_Car_Boot() 成功返回后由最高优先级控制任务调用。
 * @note 当前由 s_control_task 以 10 ms（100 Hz）周期驱动，须与
 *       board_cfg.c 中的 control_period_s（0.01f）严格一致。
 */
void App_Car_StepControl(uint32_t now_ms);

/**
 * @brief 推进看门狗监控、遥测发送与周期状态日志。
 * @param[in] now_ms 当前系统单调毫秒时间戳（单位：ms）。
 * @pre 须在 App_Car_Boot() 成功返回后由低优先级服务任务调用。
 * @note 当前由 s_service_task 以 10 ms（100 Hz）周期驱动，非实时关键路径。
 */
void App_Car_StepService(uint32_t now_ms);

#endif /* LAYERED_CAR_USER_APP_CAR_H */

/** @} */

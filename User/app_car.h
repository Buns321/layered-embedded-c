/**
 * @file app_car.h
 * @brief F407 四轮全向小车应用层启动与周期步进接口。
 */

#ifndef LAYERED_CAR_USER_APP_CAR_H
#define LAYERED_CAR_USER_APP_CAR_H

#include <stdbool.h>
#include <stdint.h>

/** @brief 启动板级基础服务并完成整车门面初始化。 */
bool App_Car_Boot(void);

/** @brief 推进 IMU 与遥控输入解析（输入任务周期调用）。 */
void App_Car_StepInput(uint32_t now_ms);

/** @brief 读取遥控输入并运行底盘四轮闭环控制（控制任务周期调用）。 */
void App_Car_StepControl(uint32_t now_ms);

/** @brief 推进看门狗监控、遥测发送与周期状态日志（服务任务周期调用）。 */
void App_Car_StepService(uint32_t now_ms);

#endif /* LAYERED_CAR_USER_APP_CAR_H */

//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file rtos_app.h
 * @brief F103 任务一的 ImuEkfTask 与 VofaTask 入口。
 * @addtogroup rtos
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_RTOS_APP_H
#define LAYERED_EMBEDDED_C_RTOS_APP_H

#include <stdbool.h>

/**
 * @brief 创建静态 ImuEkfTask。
 * @retval true 任务创建成功。
 * @retval false 静态内存或 CMSIS-RTOS2 属性非法。
 * @pre osKernelInitialize() 已调用。
 */
bool RTOS_App_CreateTasks(void);

/**
 * @brief 100 Hz IMU、标定和 EKF 任务入口。
 * @param argument CMSIS-RTOS2 参数，当前未使用。
 */
void RTOS_App_ImuTask(void *argument);

/**
 * @brief 50 Hz JustFloat 与参数命令任务入口。
 * @param argument CMSIS-RTOS2 参数，当前未使用。
 * @note 当前由 CubeMX 生成的 defaultTask 槽位调用。
 */
void RTOS_App_TelemetryTask(void *argument);

/** @} */

#endif //LAYERED_EMBEDDED_C_RTOS_APP_H

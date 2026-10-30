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
 * @retval true ImuEkfTask 已用静态 TCB 和静态栈成功创建。
 * @retval false osThreadNew 返回 NULL，表示静态内存、优先级或内核属性不合法。
 * @pre osKernelInitialize() 已调用，且 App_FastChannel_Init 已完成所需的静态对象初始化。
 * @post 成功时任务处于 AboveNormal 优先级并可被调度；VofaTask 由 CubeMX 的 defaultTask 槽位创建。
 * @note 不分配动态内存；应在 osKernelStart 前调用一次。
 */
bool RTOS_App_CreateTasks(void);

/**
 * @brief 100 Hz IMU、标定和 EKF 任务入口。
 * @param argument CMSIS-RTOS2 任务参数；当前忽略，允许为 NULL。
 * @pre 仅由 CMSIS-RTOS2 任务调度器调用，且 App_FastChannel_Init 已成功返回。
 * @post 每次周期调用 App_FastChannel_ProcessImu，并通过 App_FastChannel_ReportImuMetrics 更新 ImuEkfTask 内部性能统计。
 * @note 任务以 osDelayUntil 维持 10 ms 周期；正常运行时永不返回，不分配内存。
 * @note 若延迟超过一个周期，下一次唤醒从当前 tick 重新排程，不补跑历史周期。
 */
void RTOS_App_ImuTask(void *argument);

/**
 * @brief 50 Hz JustFloat 与参数命令任务入口。
 * @param argument CMSIS-RTOS2 任务参数；当前忽略，允许为 NULL。
 * @pre 当前由 CubeMX 生成的 defaultTask 槽位调用，且 App_FastChannel_Init 已成功返回。
 * @post 每次周期调用 App_FastChannel_ProcessTelemetry，完成命令消费和 32 通道 JustFloat 发送。
 * @note 任务以 osDelayUntil 维持 20 ms 周期；正常运行时永不返回，不分配内存。
 * @note 若延迟超过一个周期，下一次唤醒从当前 tick 重新排程，不补跑历史周期。
 */
void RTOS_App_TelemetryTask(void *argument);

/** @} */

#endif //LAYERED_EMBEDDED_C_RTOS_APP_H

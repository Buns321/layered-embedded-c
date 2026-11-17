/**
 * @file rtos_periodic.h
 * @brief CMSIS-RTOS2 固定周期任务循环配置。
 * @addtogroup rtos
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_RTOS_PERIODIC_H
#define LAYERED_EMBEDDED_C_RTOS_PERIODIC_H

#include <stdint.h>

/**
 * @brief 固定周期任务的静态配置。
 * @note process 在任务上下文运行，接收统一毫秒时间戳。
 */
typedef struct rtos_periodic_task_config_s {
  /** @brief 调试名称；生命周期覆盖整个固件运行期。 */
  const char *name;
  /** @brief App 周期处理入口；不得为 NULL，且不得返回。 */
  void (*process)(uint32_t now_ms);
  /** @brief 调度周期，单位 ms；必须大于 0。 */
  uint32_t period_ms;
} rtos_periodic_task_config_t;

/**
 * @brief 运行一个固定周期 CMSIS-RTOS2 任务循环。
 * @param argument 指向只读 rtos_periodic_task_config_t 的静态存储。
 * @pre argument 非空，配置中的 process 非空，period_ms 大于 0。
 * @post 每个周期调用一次 process，并把 tick 转换为毫秒时间戳。
 * @note 本函数不返回；错过周期时从当前 tick 重新对齐，不补跑历史周期。
 * @warning 只适用于固定周期、无事件阻塞的任务。
 */
void RTOS_Periodic_RunTask(void *argument);

/** @} */

#endif //LAYERED_EMBEDDED_C_RTOS_PERIODIC_H

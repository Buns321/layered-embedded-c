//
// 由 Buns_ 创建于 2026/10/30。
//

/**
 * @file rtos_config.h
 * @brief F407 小车任务调度配置。
 * @addtogroup rtos
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_RTOS_CONFIG_H
#define LAYERED_EMBEDDED_C_RTOS_CONFIG_H

/** @brief 控制任务调度周期（RTOS tick）。 */
#define RTOS_CAR_CONTROL_PERIOD_TICKS 10u

/** @brief UART 任务调度周期（RTOS tick）。 */
#define RTOS_CAR_UART_PERIOD_TICKS 2u

/** @brief 监控任务调度周期（RTOS tick）。 */
#define RTOS_CAR_MONITOR_PERIOD_TICKS 10u

/** @} */

#endif //LAYERED_EMBEDDED_C_RTOS_CONFIG_H

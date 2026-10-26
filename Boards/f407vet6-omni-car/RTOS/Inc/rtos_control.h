//
// 由 Buns_ 创建于 2026/8/31。
//

/**
 * @file rtos_control.h
 * @brief 控制任务接口。
 * @addtogroup rtos
 * @{
 * @note 板级强符号实现覆盖 Core/Src/freertos.c 中的弱占位。
 */

#ifndef LAYERED_EMBEDDED_C_RTOS_CONTROL_H
#define LAYERED_EMBEDDED_C_RTOS_CONTROL_H

/**
 * @brief 固定 100 Hz 的小车控制任务。
 * @param argument CMSIS-RTOS2 任务参数，当前未使用。
 * @note 任务入口不得阻塞；控制周期由 osDelayUntil 维护。
 */
void RTOS_Control_StartTask(void *argument);

/** @} */

#endif //LAYERED_EMBEDDED_C_RTOS_CONTROL_H

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
 * @brief 固定 100 Hz 的小车控制任务入口。
 * @param argument CMSIS-RTOS2 任务参数；当前忽略，允许为 NULL。
 * @pre 仅在调度器启动后由内核调用，且 App_Car_Init 已成功完成。
 * @post 每个 10 tick 周期调用一次 App_Car_ProcessControl；错过多个周期时只从当前 tick 重新排程，不补跑历史周期。
 * @note 任务通过 osDelayUntil 保持 100 Hz；正常运行时永不返回，不分配内存，也不阻塞等待应用事件。
 * @warning 本任务负责四轮闭环和故障急停，长时间被更高优先级任务抢占会触发心跳超时。
 */
void RTOS_Control_StartTask(void *argument);

/** @} */

#endif //LAYERED_EMBEDDED_C_RTOS_CONTROL_H

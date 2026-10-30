//
// 由 Buns_ 创建于 2026/8/31。
//

/**
 * @file rtos_monitor.h
 * @brief 监控任务接口。
 * @addtogroup rtos
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_RTOS_MONITOR_H
#define LAYERED_EMBEDDED_C_RTOS_MONITOR_H

/**
 * @brief 固定 100 Hz 的监控任务入口。
 * @param argument CMSIS-RTOS2 任务参数；当前忽略，允许为 NULL。
 * @pre 仅在调度器启动后由内核调用，且 App_Car_Init 已成功完成。
 * @post 每 10 tick 调用一次 App_Car_ProcessMonitor，由其执行心跳检查、50 Hz 遥测和 1 Hz 状态日志。
 * @note 遥测和日志分别按 20 ms、1000 ms 在应用层节流；任务本身仍为 100 Hz，正常运行时永不返回。
 * @note 错过多个调度周期时只从当前 tick 重新排程；不分配内存，不等待 DMA 或应用事件。
 * @warning 控制心跳超时只在本任务获得 CPU 后才会锁存；监控任务长期得不到调度时无法保证及时急停。
 */
void RTOS_Monitor_StartTask(void *argument);

/** @} */

#endif //LAYERED_EMBEDDED_C_RTOS_MONITOR_H

/**
 * @file f103_selftest.h
 * @brief F103C8T6 非破坏性板载自检入口。
 * @addtogroup tests
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_F103_SELFTEST_H
#define LAYERED_EMBEDDED_C_F103_SELFTEST_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 运行一次非破坏性板载自检，检查时钟、tick、日志和 CRC。
 * @param scheduler_tick_ok 调用方对 FreeRTOS tick 健康状态的判断；true 计入
 *        tick 项通过，false 计入失败。
 * @param scheduler_tick_elapsed_ms 调用方测得的 tick 延时，单位 ms；仅写入日志，
 *        不参与通过/失败判定。
 * @retval true clock、tick、log、crc 四项全部通过（failed_count 为 0）。
 * @retval false 至少一项失败；失败项由 TEST 标签 WARN 日志给出。
 * @pre 已在 CubeMX 初始化完成、RTOS 调度器启动后的任务上下文调用；
 *      RTT 日志后端和硬件 CRC 外设可用。
 * @post 输出 BEGIN、各项结果和 SUMMARY；计数只保存在本函数局部变量中。
 * @note 不操作继电器、电机、舵机、蜂鸣器和灯；只读取 SystemCoreClock、日志统计，
 *       并对硬件 CRC 单元执行一次计算。
 * @note CRC 项会复位并占用 PLATFORM_CRC_1，结束后其数据寄存器保留本次 CRC 值。
 * @warning 日志项读取全局丢弃/竞争计数，启动期任何日志异常都会使该项失败；
 *          自检自身的日志也可能被过滤或丢弃；实现不使用 assert，失败通过
 *          返回值和日志呈现。
 */
bool F103_SelfTest_RunAll(
  bool scheduler_tick_ok,
  uint32_t scheduler_tick_elapsed_ms
);

/** @} */

#endif //LAYERED_EMBEDDED_C_F103_SELFTEST_H

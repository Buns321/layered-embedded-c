/**
 * @file stm32f1xx.h
 * @brief System 临界区契约测试使用的 Cortex-M PRIMASK 访问桩。
 * @ingroup tests
 */

#ifndef LAYERED_EMBEDDED_C_TEST_STM32F1XX_H
#define LAYERED_EMBEDDED_C_TEST_STM32F1XX_H

#include <stdint.h>

/**
 * @brief 返回测试注入的 PRIMASK 快照。
 * @return g_test_primask 的当前值；0 表示中断未屏蔽，1 表示已屏蔽。
 * @post 不修改 PRIMASK 测试状态或硬件寄存器。
 * @note 该桩直接返回普通内存变量，不读取 Cortex-M 的 PRIMASK。
 */
uint32_t __get_PRIMASK(void);

/**
 * @brief 在测试中设置 PRIMASK，模拟关闭中断。
 * @post g_test_primask 被置为 1，供 Sys_Critical_Enter 保存和恢复。
 * @note 该桩不执行真实的 CPSID 指令，也不影响主机进程中断。
 */
void __disable_irq(void);

/**
 * @brief 在测试中写入指定 PRIMASK 值。
 * @param primask 要保存到测试状态中的中断掩码值；不做符号或位宽校验。
 * @post g_test_primask 等于 primask。
 * @note 该桩只模拟中断状态恢复，不操作 Cortex-M 寄存器。
 */
void __set_PRIMASK(uint32_t primask);

#endif //LAYERED_EMBEDDED_C_TEST_STM32F1XX_H

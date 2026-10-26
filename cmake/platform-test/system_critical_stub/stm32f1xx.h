/**
 * @file stm32f1xx.h
 * @brief System 临界区契约测试使用的 Cortex-M PRIMASK 访问桩。
 * @ingroup tests
 */

#ifndef LAYERED_EMBEDDED_C_TEST_STM32F1XX_H
#define LAYERED_EMBEDDED_C_TEST_STM32F1XX_H

#include <stdint.h>

/**
 * @brief 读取测试 PRIMASK。
 * @return 当前测试 PRIMASK。
 */
uint32_t __get_PRIMASK(void);

/** @brief 在测试环境中模拟关闭中断。 */
void __disable_irq(void);

/**
 * @brief 在测试环境中写入 PRIMASK。
 * @param primask 目标 PRIMASK。
 */
void __set_PRIMASK(uint32_t primask);

#endif //LAYERED_EMBEDDED_C_TEST_STM32F1XX_H

/**
 * @file f103_hardware_test.h
 * @brief F103C8T6 板载外设实测与 USART2 主机自检入口。
 * @addtogroup tests
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_F103_HARDWARE_TEST_H
#define LAYERED_EMBEDDED_C_F103_HARDWARE_TEST_H

#include <stdint.h>

/** @brief 一次板载硬件测试的计数结果。 */
typedef struct f103_hardware_test_summary_s {
  uint32_t passed;  /**< 通过项数量。 */
  uint32_t failed;  /**< 失败项数量。 */
  uint32_t skipped; /**< 因外部设备或接线缺失而跳过项数量。 */
} f103_hardware_test_summary_t;

/**
 * @brief 初始化 USART2 自检命令接收并发送上电信息。
 * @note 必须在所有 CubeMX 外设初始化完成、RTOS 调度器启动后调用。
 */
void F103_HardwareTest_Init(void);

/**
 * @brief 执行全部非复位类板载硬件测试。
 * @return 本次测试的通过、失败和跳过计数。
 * @note IWDG、WWDG 复位测试只能通过 USART2 命令触发。
 */
f103_hardware_test_summary_t F103_HardwareTest_RunAll(void);

/**
 * @brief 处理 USART2 自检命令。
 * @note 必须在任务上下文周期调用。
 */
void F103_HardwareTest_Process(void);

/** @} */

#endif //LAYERED_EMBEDDED_C_F103_HARDWARE_TEST_H

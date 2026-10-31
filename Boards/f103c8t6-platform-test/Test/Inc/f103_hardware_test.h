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
 * @brief 注册 USART2/ADC 回调并发送上电信息。
 * @pre 所有 CubeMX 外设初始化完成、RTOS 调度器已启动，且在任务上下文调用；
 *      USART2 和 ADC1 已按测试固件配置启用。
 * @post 按注册结果配置 UART2/ADC1 回调；尝试以单字节 IT 接收启动 USART2；
 *       尝试通过 DMA 发送 "F103 READY" 上电信息，发送失败时仅清除忙标志且不重试。
 * @note 回调注册失败只记录 SYS_LOG，不中止初始化；后续命令收发可能不可用。
 * @note 该函数无返回值，调用方必须通过日志、F103_HardwareTest_RunAll() 或
 *       USART2 命令观察结果。
 * @warning 应在调度器启动后调用一次；重复调用会使回调注册返回 BUSY，并可能
 *          使 USART2 接收重挂失败而进入待重挂状态。
 */
void F103_HardwareTest_Init(void);

/**
 * @brief 依次执行 GPIO、I2C、ADC、编码器、PWM 和 USART3 共 9 项测试。
 * @return 本次运行的汇总：passed、failed、skipped 为各项计数之和（总项数为 9）；
 *         返回值为值拷贝，调用方拥有该副本。
 * @pre 已调用 F103_HardwareTest_Init() 注册 ADC1 完成回调，且测试固件已启用
 *      I2C1、ADC1、TIM1-TIM4、USART3；在任务上下文调用。
 * @post 输出 BEGIN 和 SUMMARY 日志；各子测试结束后的硬件状态见各自说明。
 * @note 执行顺序固定：GPIO 回读、AHT20、OLED、ADC DMA、TIM1 编码器、
 *       TIM2、TIM3、TIM4、USART3 探测。
 * @warning 会短暂拉高 PA7/PB0/PB5/PB9/PA11、改写 TIM1 计数器为 1234 并停止编码器、
 *          让 TIM4_CH3 输出约 1.5 ms 中位脉冲，且 USART3 收发可能各阻塞 100 ms。
 *          执行前必须确认继电器、蜂鸣器、舵机和编码器连接处于安全状态。
 * @warning 未调用 Init 时 ADC 项会因缺少完成回调在约 20 ms 后失败，UART3 结果也
 *          取决于外部设备；本函数不返回单独的错误码，失败仅体现在计数和日志中。
 */
f103_hardware_test_summary_t F103_HardwareTest_RunAll(void);

/**
 * @brief 单次处理 USART2 DMA 接收结果、重挂请求和命令行输入。
 * @pre 已调用 F103_HardwareTest_Init() 且 USART2 回调已注册；必须在任务上下文
 *      周期调用（当前默认任务每 100 ms 一次），不得在 ISR 中调用。
 * @post 消费环形缓冲区中当前可读字节；以 LF 结束的非空命令会先写日志再执行，
 *       并清空命令长度。DMA 完成时输出 RX_DMA PASS/FAIL 并重新挂接收；
 *       重挂标志置位时先中止 USART2 接收再重新挂接。
 * @note 忽略 CR；超长命令仅保留前 63 个字符，其余字节在下一个 LF 前丢弃。
 * @note 不主动轮询串口硬件，只处理回调已写入的状态；若调用间隔过长，64 字节
 *       环形缓冲区会满并静默丢弃新字节。
 * @warning 执行 TEST 命令会运行完整硬件套件。
 */
void F103_HardwareTest_Process(void);

/** @} */

#endif //LAYERED_EMBEDDED_C_F103_HARDWARE_TEST_H

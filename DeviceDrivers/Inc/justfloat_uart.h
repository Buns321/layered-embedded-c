//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file justfloat_uart.h
 * @brief 通过 UART DMA 发送的 JustFloat 二进制遥测。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_JUSTFLOAT_UART_H
#define LAYERED_EMBEDDED_C_JUSTFLOAT_UART_H

#include "telemetry_base.h"
#include "uart_base.h"
#include <stdbool.h>
#include <stdint.h>

#define JUSTFLOAT_FRAME_TAIL_SIZE 4u
#define JUSTFLOAT_UART_TX_CAPACITY \
  ((TELEMETRY_CHANNEL_CAPACITY * 4u) + JUSTFLOAT_FRAME_TAIL_SIZE)

/**
 * @brief JustFloat UART 设备对象。
 * @note Init 会取得该 UART 回调槽的所有权。该 UART 实例不得再有其他
 *       独立回调使用者。
 */
typedef struct justfloat_uart_s {
  telemetry_base_t base;
  uart_base_t *uart_h;
  uint8_t channel_count;

  uint8_t tx_buffer[JUSTFLOAT_UART_TX_CAPACITY];
  volatile bool busy;
  volatile uint32_t sent_frames;
  volatile uint32_t dropped_frames;
  volatile uint32_t error_count;
} justfloat_uart_t;

/**
 * @brief 在 UART 上初始化 JustFloat 遥测对象。
 * @param telemetry_h 待初始化的静态对象，不得为 NULL；调用方保留所有权。
 * @param name 调试名称，不得为 NULL；指针会被保存，必须在对象生命周期内有效。
 * @param uart_h 已初始化且 TX 支持 DMA 的 UART 后端，借用且不得为 NULL。
 * @param channel_count 每帧通道数，范围 1..TELEMETRY_CHANNEL_CAPACITY。
 * @pre telemetry_h 尚未被并发使用，uart_h 当前没有进行中的 TX，也没有其他回调所有者。
 * @post 基类、通道数、忙状态和计数器已初始化，并在 uart_h 上注册 TX 完成回调；
 *       该回调集合会覆盖 uart_h 原有回调，其他回调槽被清空。
 * @note 不启动传输、不阻塞、不分配动态内存；必须在任务和中断使用前由单一装配者调用。
 * @warning 本对象取得 uart_h 的设备级回调槽所有权；不得再注册其他回调或并发初始化，
 *          也不得在已有 DMA 发送时调用，否则旧完成事件将无法归还给原所有者。
 */
void Dev_JustFloat_UART_Init(
  justfloat_uart_t *telemetry_h,
  const char *name,
  uart_base_t *uart_h,
  uint8_t channel_count
);

/** @} */

#endif //LAYERED_EMBEDDED_C_JUSTFLOAT_UART_H

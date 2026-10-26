//
// 由 Buns_ 创建于 2026/10/26。
//

/**
 * @file joycon_uart.h
 * @brief JoyCon ASCII UART 命令接收设备。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_JOYCON_UART_H
#define LAYERED_EMBEDDED_C_JOYCON_UART_H

#include "joycon_base.h"
#include "uart_base.h"
#include <stdbool.h>
#include <stdint.h>

#define JOYCON_UART_RX_BUFFER_SIZE 256u
#define JOYCON_UART_RX_INDEX_MASK \
  ((uint16_t)(JOYCON_UART_RX_BUFFER_SIZE - 1u))
#define JOYCON_UART_FRAME_CAPACITY 64u

_Static_assert(
  (JOYCON_UART_RX_BUFFER_SIZE & (JOYCON_UART_RX_BUFFER_SIZE - 1u)) == 0u,
  "JoyCon UART RX buffer size must be a power of two"
);

/**
 * @brief JoyCon UART 设备对象。
 * @note UART 中断只把单字节写入环形队列，完整帧解析由 Process 在任务上下文完成。
 * @note frame_text 只允许任务上下文访问；rx_buffer 由 ISR 单写、任务单读。
 * @note 对象不允许复制，生命周期必须覆盖其 UART 回调注册期。
 */
typedef struct joycon_uart_s {
  joycon_base_t base;
  uart_base_t *uart_h;

  uint8_t rx_byte;
  uint8_t rx_buffer[JOYCON_UART_RX_BUFFER_SIZE];
  volatile uint16_t rx_write;
  volatile uint16_t rx_read;

  char frame_text[JOYCON_UART_FRAME_CAPACITY];
  uint16_t frame_length;
  bool     frame_active;
  bool     frame_overflow;
  volatile bool is_started;

  joycon_input_t input;

  volatile bool     rx_rearm_pending;
  volatile uint32_t rx_byte_count;
  volatile uint32_t rx_overflow_count;
  volatile uint32_t uart_error_count;
  volatile uint32_t rearm_error_count;
  volatile uint32_t invalid_frame_count;
  volatile uint32_t parsed_frame_count;
} joycon_uart_t;

/**
 * @brief 初始化 JoyCon UART 设备。
 * @param joycon_h 对象。
 * @param name 调试名称。
 * @param uart_h 已初始化的 UART 后端，必须支持中断接收。
 * @pre uart_h 对应物理 UART 不得再注册其他回调所有者。
 * @note 初始化不启动接收，也不会调用 UART 传输接口。
 */
void Dev_JoyCon_UART_Init(
  joycon_uart_t *joycon_h,
  const char *name,
  uart_base_t *uart_h
);

/**
 * @brief 注册 UART 回调并启动单字节中断接收。
 * @param joycon_h 对象。
 * @retval DEV_UART_OK 启动成功。
 * @retval DEV_UART_ERROR 参数或 UART 后端错误。
 * @retval DEV_UART_BUSY UART 已被占用。
 * @note 只允许在任务上下文调用一次。
 */
dev_uart_status_t Dev_JoyCon_UART_Start(joycon_uart_t *joycon_h);

/**
 * @brief 解析环形队列中的已接收字节。
 * @param joycon_h 对象。
 * @param now_ms 当前单调毫秒时间戳。
 * @pre 只允许单一 UART 任务调用。
 * @note 完整帧发布是原子的，其他任务只能通过 GetInput 读取副本。
 */
void Dev_JoyCon_UART_Process(
  joycon_uart_t *joycon_h,
  uint32_t now_ms
);

/**
 * @brief 向 ASCII 解析器输入一个字节。
 * @param joycon_h 对象。
 * @param byte 输入字节。
 * @param now_ms 当前单调毫秒时间戳。
 * @note 该接口供任务上下文和主机测试使用，不得从 ISR 调用。
 */
void Dev_JoyCon_UART_FeedByte(
  joycon_uart_t *joycon_h,
  uint8_t byte,
  uint32_t now_ms
);

/**
 * @brief 复制最近一帧有效 JoyCon 输入。
 * @param joycon_h 对象。
 * @param input 输出快照。
 * @return 至少收到一帧有效报文时返回 true。
 * @note 可以从其他任务调用；复制期间使用 System 临界区保护。
 */
bool Dev_JoyCon_UART_GetInput(
  joycon_uart_t *joycon_h,
  joycon_input_t *input
);

/**
 * @brief 复制 JoyCon 设备状态快照。
 * @param joycon_h 对象。
 * @param status 输出状态。
 * @note 可以从其他任务调用。
 */
void Dev_JoyCon_UART_GetStatus(
  joycon_uart_t *joycon_h,
  joycon_status_t *status
);

/** @} */

#endif //LAYERED_EMBEDDED_C_JOYCON_UART_H

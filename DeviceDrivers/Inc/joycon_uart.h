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
#include "system_byte_ring.h"
#include "uart_base.h"
#include <stdbool.h>
#include <stdint.h>

#define JOYCON_UART_RX_BUFFER_SIZE 256u
#define JOYCON_UART_FRAME_CAPACITY 64u

_Static_assert(
  (JOYCON_UART_RX_BUFFER_SIZE & (JOYCON_UART_RX_BUFFER_SIZE - 1u)) == 0u,
  "JoyCon UART RX buffer size must be a power of two"
);

/**
 * @brief JoyCon UART 设备对象。
 * @note UART 中断只把单字节写入 rx_ring（实际可存 255 字节），完整帧解析由 Process 在任务上下文完成。
 * @note frame_text 容量为 64 字节且不保存结束符；只允许任务上下文访问。
 * @note 对象不允许复制；uart_h 与 base.name 都是非 owning 指针，生命周期必须覆盖对象及 UART 回调注册期。
 */
typedef struct joycon_uart_s {
  joycon_base_t base;
  uart_base_t *uart_h;

  uint8_t rx_byte;
  uint8_t rx_buffer[JOYCON_UART_RX_BUFFER_SIZE];
  sys_byte_ring_t rx_ring;

  char frame_text[JOYCON_UART_FRAME_CAPACITY];
  uint16_t frame_length;
  bool     frame_active;
  bool     frame_overflow;
  volatile bool is_started;

  joycon_input_t input;

  volatile bool     rx_rearm_pending;
  volatile uint32_t rx_byte_count;
  volatile uint32_t uart_error_count;
  volatile uint32_t rearm_error_count;
  volatile uint32_t invalid_frame_count;
  volatile uint32_t parsed_frame_count;
} joycon_uart_t;

/**
 * @brief 初始化 JoyCon UART 设备对象并绑定静态操作表。
 * @param joycon_h 具体对象；调用方静态分配，函数会清零全部字段。
 * @param name 调试名称；只保存指针，其生命周期必须覆盖对象。
 * @param uart_h 已初始化的 UART 后端；只保存非 owning 指针，后端必须支持中断接收。
 * @pre joycon_h、name、uart_h 非空；对象未被并发使用；同一物理 UART 没有其他回调所有者。
 * @post joycon_h 全部字段清零，base 绑定 JoyCon 操作表并保存 name，uart_h 已记录；is_started 为 false。
 * @note 不注册回调、不启动接收，也不调用 UART 传输接口，因此没有硬件副作用。
 * @note 必须在任务上下文调用，且接收启动后不得重复调用。
 */
void Dev_JoyCon_UART_Init(
  joycon_uart_t *joycon_h,
  const char *name,
  uart_base_t *uart_h
);

/**
 * @brief 注册接收/错误回调并启动单字节中断接收。
 * @param joycon_h 已由 Init 初始化且尚未启动的设备对象。
 * @retval DEV_UART_OK 回调已注册且首次单字节接收挂接成功。
 * @retval DEV_UART_ERROR 首次 Dev_UART_ReceiveIT 失败；此处 is_started 仍为 true，Process 会继续重试。
 * @pre joycon_h 非空且已 Init；is_started 为 false；UART 后端可用；同一物理 UART 无其他回调所有者。
 * @post 回调写入 UART 后端（覆盖原有回调），is_started 为 true；接收处于已挂接或待重挂状态。
 * @note 注册本身不阻塞，接收完成回调在中断上下文执行；只允许在任务上下文调用一次。
 * @warning 实现不检测 UART 是否已被其他所有者占用；覆盖回调可能使其他接收者失联，且不会返回 BUSY。
 */
dev_uart_status_t Dev_JoyCon_UART_Start(joycon_uart_t *joycon_h);

/**
 * @brief 处理待重挂接收并解析环形队列中的全部字节。
 * @param joycon_h 已 Start 的对象；rx_ring 只由本任务消费。
 * @param now_ms 当前单调毫秒时间戳；同一轮完成的一帧使用该时间。
 * @pre joycon_h 非空且已 Start；仅由单一 UART 任务调用，不得与 FeedByte 或其他 Process 并发。
 * @post rx_ring 被消费到空，队列字节按序送入解析器；可能发布新快照并更新计数；如有待重挂则重试。
 * @note 不等待数据，队列为空时立即返回；ISR 只生产，任务只消费。
 * @warning 同一批字节共享调用时的 now_ms，不能用于逐字节计时。
 */
void Dev_JoyCon_UART_Process(
  joycon_uart_t *joycon_h,
  uint32_t now_ms
);

/**
 * @brief 向 JoyCon ASCII 状态机输入一个字节。
 * @param joycon_h 已 Init 的对象；由 Process 或主机测试直接调用。
 * @param byte 输入字节，取值 0..255；'[' 开始帧，']' 结束帧，CR/LF 被忽略。
 * @param now_ms 当前单调毫秒时间戳；仅在字节完成有效帧时作为该帧的 time_ms。
 * @pre joycon_h 非空；对象未被其他任务并发解析；不得在 ISR 中调用。
 * @post 组帧缓冲最多接收 64 字节；超长帧置溢出标志并在 ']' 时计为无效；有效帧在临界区内原子发布。
 * @note 失败不返回错误，只递增 invalid_frame_count；溢出帧丢弃且不影响上一帧快照。
 */
void Dev_JoyCon_UART_FeedByte(
  joycon_uart_t *joycon_h,
  uint8_t byte,
  uint32_t now_ms
);

/**
 * @brief 复制最近一帧有效 JoyCon 输入快照。
 * @param joycon_h 已 Init 的对象；允许其他任务读取。
 * @param input 输出缓冲区；调用方拥有，函数在临界区内覆盖全部字段，即使返回 false 也会写入零快照。
 * @return 快照 sequence 非零时返回 true，表示至少成功解析过一帧；false 表示尚无有效帧。
 * @pre joycon_h、input 非空；input 的生命周期覆盖调用。
 * @post input 含最近一次成功发布的归一化摇杆量、毫秒时间戳和序号。
 * @note 复制期间进入 System 临界区，不得在 ISR 中调用；临界区只复制快照，不解析协议。
 */
bool Dev_JoyCon_UART_GetInput(
  joycon_uart_t *joycon_h,
  joycon_input_t *input
);

/**
 * @brief 复制 JoyCon 设备运行状态快照。
 * @param joycon_h 已由 Init 初始化且未销毁的 JoyCon 对象；调用期间不得并发 Init 或销毁。
 * @param status 输出缓冲区；调用方拥有，函数在临界区内完整覆盖。
 * @pre joycon_h、status 非空；status 的生命周期覆盖调用。
 * @post status 含启动标志、接收/解析/无效帧/错误计数及最后一帧时间和序号，各计数为同一临界区快照。
 * @note 可在其他任务调用，不得在 ISR 中调用；计数只增不减，溢出后自然回绕。
 */
void Dev_JoyCon_UART_GetStatus(
  joycon_uart_t *joycon_h,
  joycon_status_t *status
);

/** @} */

#endif //LAYERED_EMBEDDED_C_JOYCON_UART_H

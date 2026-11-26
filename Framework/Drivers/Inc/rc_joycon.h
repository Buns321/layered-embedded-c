/**
 * @file rc_joycon.h
 * @brief JoyCon 蓝牙串口遥控接收与 ASCII 帧解析器接口。
 * @details 属于 Framework/Drivers 设备驱动层，直连 bsp_uart_t（支持字节中断或
 *          DMA+IDLE 接收），解析 `[j,lx,ly,rx,ry]` 文本帧，归一化至 [-1.0, 1.0]，
 *          并在有效帧到达时可选自动喂狗 sys_daemon_t。
 *
 * @addtogroup Framework_Drivers
 * @{
 */

#ifndef LAYERED_CAR_RC_JOYCON_H
#define LAYERED_CAR_RC_JOYCON_H

#include "bsp_def.h"
#include "bsp_uart.h"
#include "sys_daemon.h"
#include <stdbool.h>
#include <stdint.h>

#ifndef RC_JOYCON_RX_RING_SIZE
/** @brief JoyCon 串口接收环形缓冲区容量（单位：字节，须为 2 的幂）。 */
#define RC_JOYCON_RX_RING_SIZE 128U
#endif
#ifndef RC_JOYCON_RX_DMA_SIZE
/** @brief JoyCon DMA+IDLE 模式单次接收暂存缓冲区容量（单位：字节）。 */
#define RC_JOYCON_RX_DMA_SIZE 32U
#endif
/** @brief 单帧 ASCII 载荷（不含起止方括号 `[` `]`）最大字符容量。 */
#define RC_JOYCON_FRAME_CAPACITY 64U

_Static_assert((RC_JOYCON_RX_RING_SIZE & (RC_JOYCON_RX_RING_SIZE - 1U)) == 0U,
               "RC_JOYCON_RX_RING_SIZE must be a power of two");

/**
 * @brief 归一化 JoyCon 遥控输入快照结构体。
 */
typedef struct rc_joycon_input_s {
  float left_x_norm;  /**< 左摇杆 X 轴归一化值（无量纲，范围 [-1.0, 1.0]） */
  float left_y_norm;  /**< 左摇杆 Y 轴归一化值（无量纲，范围 [-1.0, 1.0]） */
  float right_x_norm; /**< 右摇杆 X 轴归一化值（无量纲，范围 [-1.0, 1.0]） */
  float right_y_norm; /**< 右摇杆 Y 轴归一化值（无量纲，范围 [-1.0, 1.0]） */
  uint32_t time_ms;   /**< 该帧解析完成时的单调毫秒时间戳（单位：ms） */
  uint32_t sequence;  /**< 单调递增有效帧序号（0 表示尚无有效帧） */
} rc_joycon_input_t;

/**
 * @brief JoyCon 接收统计与状态快照结构体。
 */
typedef struct rc_joycon_status_s {
  bool is_started;           /**< 是否已调用 RC_JoyCon_Start() 启动接收 */
  uint32_t received_bytes;   /**< 底层 UART 累计接收字节数（单位：bytes） */
  uint32_t parsed_frames;    /**< 成功解析并发布的有效帧计数 */
  uint32_t invalid_frames;   /**< 格式错误、超限或溢出丢弃的无效帧计数 */
  uint32_t rx_dropped_bytes; /**< 环形缓冲区满导致的丢弃字节数（单位：bytes） */
  uint32_t rx_error_count;   /**< 底层 UART 硬件错误计数 */
  uint32_t last_frame_ms;    /**< 最近一次有效帧时间戳（单位：ms） */
  uint32_t sequence;         /**< 最近一次有效帧序号 */
} rc_joycon_status_t;

/**
 * @brief JoyCon 初始化配置结构体。
 */
typedef struct rc_joycon_cfg_s {
  UART_HandleTypeDef *huart;  /**< STM32 HAL UART 句柄指针（非 NULL） */
  bsp_uart_rx_mode_t rx_mode; /**< 串口接收模式（NONE 时默认按 BSP_UART_RX_IT_BYTE 初始化） */
  sys_daemon_t *daemon;       /**< 可选离线看门狗节点指针（非 NULL 时有效帧自动喂狗） */
} rc_joycon_cfg_t;

/**
 * @brief JoyCon 静态接收对象结构体。
 * @note 内含环形缓冲与跨任务快照，禁止按值拷贝。
 */
typedef struct rc_joycon_s {
  bsp_uart_t uart;                             /**< 底层串口接收实例 */
  sys_daemon_t *daemon;                        /**< 绑定的看门狗节点句柄（可为 NULL） */
  uint8_t rx_ring_buf[RC_JOYCON_RX_RING_SIZE]; /**< ISR 写入、任务读出的 SPSC 接收环形缓冲 */
  uint8_t rx_dma_buf[RC_JOYCON_RX_DMA_SIZE];   /**< DMA+IDLE 模式硬件接收缓冲区 */
  char frame_text[RC_JOYCON_FRAME_CAPACITY];   /**< 当前正在组装的 ASCII 帧载荷 */
  uint16_t frame_length;                       /**< 当前帧已接收字符数 */
  bool frame_active;                           /**< 是否已捕获起始符 `[` 处于帧内 */
  bool frame_overflow;                         /**< 当前帧是否超出 RC_JOYCON_FRAME_CAPACITY */
  volatile bool is_started;                    /**< 接收启动标志 */
  rc_joycon_input_t input;                     /**< 最新有效归一化遥控快照（受临界区保护） */
  volatile uint32_t invalid_frame_count;       /**< 累计无效帧计数 */
  volatile uint32_t parsed_frame_count;        /**< 累计有效帧计数 */
} rc_joycon_t;

/**
 * @brief 初始化 JoyCon 与内部静态 UART 接收缓冲（不启动接收）。
 * @param[out] rc  JoyCon 静态对象指针（非 NULL）。
 * @param[in]  cfg 初始化配置结构体指针（非 NULL）。
 * @return 初始化状态码。
 * @retval BSP_OK              串口与对象初始化成功。
 * @retval BSP_ERR_INVALID_ARG 参数为空或 rx_mode 非法。
 * @retval 其他 bsp_status_t   底层 BSP_UART_Init 返回的错误码。
 * @pre 对应 UART 外设已完成 CubeMX 初始化。
 */
bsp_status_t RC_JoyCon_Init(rc_joycon_t *rc, const rc_joycon_cfg_t *cfg);

/**
 * @brief 启动 UART 接收。
 * @param[in,out] rc JoyCon 对象指针（非 NULL）。
 * @return 启动状态码。
 * @retval BSP_OK              接收启动成功。
 * @retval BSP_ERR_INVALID_ARG 对象为空、未完成 Init 或已处于启动状态。
 * @retval 其他 bsp_status_t   底层 BSP_UART_StartRx 返回的错误码。
 * @pre 仅在任务上下文调用一次。
 */
bsp_status_t RC_JoyCon_Start(rc_joycon_t *rc);

/**
 * @brief 排空接收环形缓冲、执行重挂补偿并解析完整帧。
 * @param[in,out] rc     JoyCon 对象指针（为 NULL 或未启动时直接返回）。
 * @param[in]     now_ms 当前系统单调毫秒时间戳（单位：ms）。
 * @pre 仅在单一输入处理任务中周期调用。
 */
void RC_JoyCon_Process(rc_joycon_t *rc, uint32_t now_ms);

/**
 * @brief 向 ASCII 帧解析状态机喂入单字节（格式 `[j,lx,ly,rx,ry]`）。
 * @param[in,out] rc     JoyCon 对象指针（为 NULL 时安全忽略）。
 * @param[in]     byte   新接收到的串口字节。
 * @param[in]     now_ms 当前系统单调毫秒时间戳（单位：ms）。
 */
void RC_JoyCon_FeedByte(rc_joycon_t *rc, uint8_t byte, uint32_t now_ms);

/**
 * @brief 临界区内复制最近一次有效遥控输入快照。
 * @param[in]  rc        JoyCon 对象指针（非 NULL）。
 * @param[out] out_input 输出快照结构体指针（非 NULL）。
 * @return 是否存在有效遥控帧。
 * @retval true  至少已成功解析过一帧（sequence != 0）。
 * @retval false 参数为空或尚无有效遥控帧。
 */
bool RC_JoyCon_GetInput(const rc_joycon_t *rc, rc_joycon_input_t *out_input);

/**
 * @brief 临界区内复制接收与解析状态快照。
 * @param[in]  rc         JoyCon 对象指针（非 NULL）。
 * @param[out] out_status 输出状态结构体指针（非 NULL）。
 */
void RC_JoyCon_GetStatus(const rc_joycon_t *rc, rc_joycon_status_t *out_status);

#endif /* LAYERED_CAR_RC_JOYCON_H */

/** @} */

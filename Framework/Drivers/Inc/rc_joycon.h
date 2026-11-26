/**
 * @file rc_joycon.h
 * @brief JoyCon 蓝牙串口遥控接收与 ASCII 帧解析器（直连 bsp_uart_t，支持 Daemon 喂狗）。
 * @addtogroup drivers
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
#define RC_JOYCON_RX_RING_SIZE 128U
#endif
#ifndef RC_JOYCON_RX_DMA_SIZE
#define RC_JOYCON_RX_DMA_SIZE 32U
#endif
#define RC_JOYCON_FRAME_CAPACITY 64U

_Static_assert((RC_JOYCON_RX_RING_SIZE & (RC_JOYCON_RX_RING_SIZE - 1U)) == 0U,
               "RC_JOYCON_RX_RING_SIZE must be a power of two");

/**
 * @brief 归一化 JoyCon 遥控快照（四通道范围 [-1, 1]，time_ms 单位 ms）。
 */
typedef struct rc_joycon_input_s {
  float left_x_norm;
  float left_y_norm;
  float right_x_norm;
  float right_y_norm;
  uint32_t time_ms;
  uint32_t sequence;
} rc_joycon_input_t;

/**
 * @brief JoyCon 接收统计与状态快照。
 */
typedef struct rc_joycon_status_s {
  bool is_started;
  uint32_t received_bytes;
  uint32_t parsed_frames;
  uint32_t invalid_frames;
  uint32_t rx_dropped_bytes;
  uint32_t rx_error_count;
  uint32_t last_frame_ms;
  uint32_t sequence;
} rc_joycon_status_t;

/**
 * @brief JoyCon 初始化配置（daemon 可选，非 NULL 时有效帧自动喂狗）。
 */
typedef struct rc_joycon_cfg_s {
  UART_HandleTypeDef *huart;
  bsp_uart_rx_mode_t rx_mode; /* NONE 时默认按 BSP_UART_RX_IT_BYTE 初始化 */
  sys_daemon_t *daemon;
} rc_joycon_cfg_t;

/**
 * @brief JoyCon 静态接收对象（禁止按值拷贝）。
 */
typedef struct rc_joycon_s {
  bsp_uart_t uart;
  sys_daemon_t *daemon;
  uint8_t rx_ring_buf[RC_JOYCON_RX_RING_SIZE];
  uint8_t rx_dma_buf[RC_JOYCON_RX_DMA_SIZE];
  char frame_text[RC_JOYCON_FRAME_CAPACITY];
  uint16_t frame_length;
  bool frame_active;
  bool frame_overflow;
  volatile bool is_started;
  rc_joycon_input_t input;
  volatile uint32_t invalid_frame_count;
  volatile uint32_t parsed_frame_count;
} rc_joycon_t;

/**
 * @brief 初始化 JoyCon 与内部静态 UART 接收缓冲（不启动接收）。
 */
bsp_status_t RC_JoyCon_Init(rc_joycon_t *rc, const rc_joycon_cfg_t *cfg);

/**
 * @brief 启动 UART 接收（仅任务上下文调用一次）。
 */
bsp_status_t RC_JoyCon_Start(rc_joycon_t *rc);

/**
 * @brief 排空接收环形缓冲、执行重挂补偿并解析完整帧（单一输入任务调用）。
 */
void RC_JoyCon_Process(rc_joycon_t *rc, uint32_t now_ms);

/**
 * @brief 向 ASCII 帧解析状态机喂入单字节（格式 `[j,lx,ly,rx,ry]`）。
 */
void RC_JoyCon_FeedByte(rc_joycon_t *rc, uint8_t byte, uint32_t now_ms);

/**
 * @brief 临界区内复制最近一次有效遥控输入快照；尚无有效帧时返回 false。
 */
bool RC_JoyCon_GetInput(const rc_joycon_t *rc, rc_joycon_input_t *out_input);

/**
 * @brief 临界区内复制接收与解析状态快照。
 */
void RC_JoyCon_GetStatus(const rc_joycon_t *rc, rc_joycon_status_t *out_status);

/** @} */

#endif /* LAYERED_CAR_RC_JOYCON_H */

/**
 * @file telem_firewater.h
 * @brief FireWater 文本遥测协议驱动（无 snprintf 浮点开销，直连 bsp_uart_t DMA 发送）。
 * @addtogroup drivers
 * @{
 */

#ifndef LAYERED_CAR_TELEM_FIREWATER_H
#define LAYERED_CAR_TELEM_FIREWATER_H

#include "bsp_def.h"
#include "bsp_uart.h"
#include "telemetry_base.h"
#include <stdint.h>

#define FIREWATER_UART_TX_CAPACITY 512U

/**
 * @brief FireWater 遥测初始化配置（channel_names 非空且不含 `,` 或 `:`，precision 0..6）。
 */
typedef struct telem_firewater_cfg_s {
  const char *name;
  UART_HandleTypeDef *huart;
  const char *const *channel_names;
  uint8_t channel_count;
  uint8_t precision;
} telem_firewater_cfg_t;

/**
 * @brief FireWater 静态遥测设备对象（base 必须为首成员；须位于普通 SRAM）。
 */
typedef struct telem_firewater_s {
  telemetry_base_t base;
  const char *const *channel_names;
  uint8_t channel_count;
  uint8_t precision;
  bsp_uart_t uart;
  telemetry_tx_state_t tx;
  uint8_t tx_buffer[FIREWATER_UART_TX_CAPACITY];
} telem_firewater_t;

/**
 * @brief 初始化 FireWater 遥测设备并绑定静态 UART DMA 发送通道。
 */
bsp_status_t Dev_Telem_FireWater_Init(telem_firewater_t *fw, const telem_firewater_cfg_t *cfg);

/** @} */

#endif /* LAYERED_CAR_TELEM_FIREWATER_H */

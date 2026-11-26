/**
 * @file telem_justfloat.h
 * @brief JustFloat 二进制小端浮点遥测协议驱动（直连 bsp_uart_t DMA 发送）。
 * @addtogroup drivers
 * @{
 */

#ifndef LAYERED_CAR_TELEM_JUSTFLOAT_H
#define LAYERED_CAR_TELEM_JUSTFLOAT_H

#include "bsp_def.h"
#include "bsp_uart.h"
#include "telemetry_base.h"
#include <stdint.h>

#define JUSTFLOAT_FRAME_TAIL_SIZE 4U
#define JUSTFLOAT_UART_TX_CAPACITY ((TELEMETRY_CHANNEL_CAPACITY * 4U) + JUSTFLOAT_FRAME_TAIL_SIZE)

/**
 * @brief JustFloat 遥测初始化配置（channel_count 范围 1..TELEMETRY_CHANNEL_CAPACITY）。
 */
typedef struct telem_justfloat_cfg_s {
  const char *name;
  UART_HandleTypeDef *huart;
  uint8_t channel_count;
} telem_justfloat_cfg_t;

/**
 * @brief JustFloat 静态遥测设备对象（base 必须为首成员；须位于普通 SRAM）。
 */
typedef struct telem_justfloat_s {
  telemetry_base_t base;
  uint8_t channel_count;
  bsp_uart_t uart;
  telemetry_tx_state_t tx;
  uint8_t tx_buffer[JUSTFLOAT_UART_TX_CAPACITY];
} telem_justfloat_t;

/**
 * @brief 初始化 JustFloat 遥测设备并绑定静态 UART DMA 发送通道。
 */
bsp_status_t Dev_Telem_JustFloat_Init(telem_justfloat_t *jf, const telem_justfloat_cfg_t *cfg);

/** @} */

#endif /* LAYERED_CAR_TELEM_JUSTFLOAT_H */

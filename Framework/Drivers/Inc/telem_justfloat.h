/**
 * @file telem_justfloat.h
 * @brief JustFloat 二进制小端浮点遥测协议驱动接口。
 * @details 属于 Framework/Drivers 设备驱动层，按 VOFA+ JustFloat 二进制协议格式
 *          （N 路 IEEE-754 float32 小端序载荷 + 4 字节帧尾 `0x00 0x00 0x80 0x7F`）
 *          打包数据，并通过 bsp_uart_t DMA 非阻塞发送。
 *
 * @addtogroup Framework_Drivers
 * @{
 */

#ifndef LAYERED_CAR_TELEM_JUSTFLOAT_H
#define LAYERED_CAR_TELEM_JUSTFLOAT_H

#include "bsp_def.h"
#include "bsp_uart.h"
#include "telemetry_base.h"
#include <stdint.h>

/** @brief JustFloat 协议固定帧尾字节数（4 字节 +Inf 哨兵）。 */
#define JUSTFLOAT_FRAME_TAIL_SIZE 4U
/** @brief JustFloat 单帧最大发送缓冲区容量（单位：字节）。 */
#define JUSTFLOAT_UART_TX_CAPACITY ((TELEMETRY_CHANNEL_CAPACITY * 4U) + JUSTFLOAT_FRAME_TAIL_SIZE)

/**
 * @brief JustFloat 遥测初始化配置结构体。
 */
typedef struct telem_justfloat_cfg_s {
  const char *name;          /**< 遥测实例调试名称字符串（非 NULL） */
  UART_HandleTypeDef *huart; /**< STM32 HAL UART 句柄指针（非 NULL） */
  uint8_t channel_count;     /**< 通道数量（范围 [1, TELEMETRY_CHANNEL_CAPACITY]） */
} telem_justfloat_cfg_t;

/**
 * @brief JustFloat 静态遥测设备对象结构体。
 * @note base 必须作为首成员；因 tx_buffer 直接供 DMA 读取，对象须位于普通 SRAM（非 CCM）。
 */
typedef struct telem_justfloat_s {
  telemetry_base_t base;                         /**< 遥测多态基类（必须为首成员） */
  uint8_t channel_count;                         /**< 固定发送通道数 */
  bsp_uart_t uart;                               /**< 底层 UART DMA 发送实例 */
  telemetry_tx_state_t tx;                       /**< 异步发送忙闲与统计状态 */
  uint8_t tx_buffer[JUSTFLOAT_UART_TX_CAPACITY]; /**< DMA 发送帧缓冲区（须位于 DMA 可访问 SRAM） */
} telem_justfloat_t;

/**
 * @brief 初始化 JustFloat 遥测设备并绑定静态 UART DMA 发送通道。
 * @param[out] jf  JustFloat 静态对象指针（非 NULL）。
 * @param[in]  cfg 初始化配置结构体指针（非 NULL）。
 * @return 初始化状态码。
 * @retval BSP_OK              参数合法且 UART 发送通道就绪。
 * @retval BSP_ERR_INVALID_ARG 参数为空或 channel_count 越界。
 * @retval 其他 bsp_status_t   底层 Telem_UartTx_Init 返回的错误码。
 * @pre 对应 UART 与 TX DMA 外设已完成 CubeMX 初始化。
 */
bsp_status_t Dev_Telem_JustFloat_Init(telem_justfloat_t *jf, const telem_justfloat_cfg_t *cfg);

#endif /* LAYERED_CAR_TELEM_JUSTFLOAT_H */

/** @} */

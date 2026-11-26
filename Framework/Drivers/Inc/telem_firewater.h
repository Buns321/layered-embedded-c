/**
 * @file telem_firewater.h
 * @brief FireWater 文本遥测协议驱动接口。
 * @details 属于 Framework/Drivers 设备驱动层，按 VOFA+ FireWater 文本格式
 *          （`name0:val0,name1:val1\n`）编码浮点通道，采用无 snprintf 的纯整数
 *          定点格式化以降低栈与 CPU 开销，并通过 bsp_uart_t DMA 非阻塞发送。
 *
 * @addtogroup Framework_Drivers
 * @{
 */

#ifndef LAYERED_CAR_TELEM_FIREWATER_H
#define LAYERED_CAR_TELEM_FIREWATER_H

#include "bsp_def.h"
#include "bsp_uart.h"
#include "telemetry_base.h"
#include <stdint.h>

#ifndef FIREWATER_UART_TX_CAPACITY
/** @brief FireWater 单帧文本发送 DMA 缓冲区容量（单位：字节）。 */
#define FIREWATER_UART_TX_CAPACITY 256U
#endif

/**
 * @brief FireWater 遥测初始化配置结构体。
 */
typedef struct telem_firewater_cfg_s {
  const char *name;                 /**< 遥测实例调试名称字符串（非 NULL） */
  UART_HandleTypeDef *huart;        /**< STM32 HAL UART 句柄指针（非 NULL） */
  const char *const *channel_names; /**< 通道名称指针数组（非空，名称不含 `,` 或 `:`） */
  uint8_t channel_count;            /**< 通道数量（范围 [1, TELEMETRY_CHANNEL_CAPACITY]） */
  uint8_t precision;                /**< 浮点输出小数位数（范围 [0, 6]） */
} telem_firewater_cfg_t;

/**
 * @brief FireWater 静态遥测设备对象结构体。
 * @note base 必须作为首成员；因 tx_buffer 直接供 DMA 读取，对象须位于普通 SRAM（非 CCM）。
 */
typedef struct telem_firewater_s {
  telemetry_base_t base;                         /**< 遥测多态基类（必须为首成员） */
  const char *const *channel_names;              /**< 通道名称数组指针 */
  uint8_t channel_count;                         /**< 通道总数 */
  uint8_t precision;                             /**< 小数精度位数（0..6） */
  bsp_uart_t uart;                               /**< 底层 UART DMA 发送实例 */
  telemetry_tx_state_t tx;                       /**< 异步发送忙闲与统计状态 */
  uint8_t tx_buffer[FIREWATER_UART_TX_CAPACITY]; /**< DMA 发送帧缓冲区（须位于 DMA 可访问 SRAM） */
} telem_firewater_t;

/**
 * @brief 初始化 FireWater 遥测设备并绑定静态 UART DMA 发送通道。
 * @param[out] fw  FireWater 静态对象指针（非 NULL）。
 * @param[in]  cfg 初始化配置结构体指针（非 NULL）。
 * @return 初始化状态码。
 * @retval BSP_OK              通道名称校验通过且 UART 发送通道就绪。
 * @retval BSP_ERR_INVALID_ARG 参数为空、通道数/精度越界或通道名含 `,` / `:`。
 * @retval 其他 bsp_status_t   底层 Telem_UartTx_Init 返回的错误码。
 * @pre 对应 UART 与 TX DMA 外设已完成 CubeMX 初始化。
 */
bsp_status_t Dev_Telem_FireWater_Init(telem_firewater_t *fw, const telem_firewater_cfg_t *cfg);

#endif /* LAYERED_CAR_TELEM_FIREWATER_H */

/** @} */

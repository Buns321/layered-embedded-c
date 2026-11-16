/**
 * @file core_board.h
 * @brief F407 四轮小车板级装配入口。
 * @addtogroup core_board
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_CORE_BOARD_H
#define LAYERED_EMBEDDED_C_CORE_BOARD_H

#include "app_car_dependencies.h"
#include "platform_def.h"

/**
 * @brief 初始化具体设备并填写小车 App 依赖。
 * @param dependencies 调用方持有的输出依赖，不得为 NULL。
 * @retval PLATFORM_STATUS_OK 板级对象和依赖已完成初始化。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT dependencies 为 NULL。
 * @retval PLATFORM_STATUS_BUSY 物理 UART 回调槽已被其他所有者占用。
 * @retval PLATFORM_STATUS_ERROR 设备初始化或启动失败。
 * @pre CubeMX 已初始化 TIM1、TIM2-TIM5、USART1-USART3、GPIO、DMA 和 NVIC。
 * @pre 仅在 RTOS 调度器启动前单线程调用。
 * @post 成功时具体设备保持静态生命周期，四轮保持禁用，WHT101 已请求 10 Hz 输出。
 * @note 重复调用返回已有依赖，不重新初始化硬件。
 */
platform_status_t Core_Board_Init(
  app_car_dependencies_t *dependencies
);

/** @} */

#endif //LAYERED_EMBEDDED_C_CORE_BOARD_H

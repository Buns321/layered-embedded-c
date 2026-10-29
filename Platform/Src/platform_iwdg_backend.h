/**
 * @file platform_iwdg_backend.h
 * @brief STM32 系列公共 IWDG 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_IWDG_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_IWDG_BACKEND_H

#include "platform_def.h"
#include "platform_iwdg.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 返回独立看门狗使用的 LSI 频率。
 */
uint32_t platform_iwdg_backend_lsi_hz(void);

/**
 * @brief 启动独立看门狗后端，不负责 CubeMX 结构性初始化。
 */
platform_status_t platform_iwdg_backend_start(
  uint32_t divisor,
  uint32_t reload
);

/**
 * @brief 刷新独立看门狗。
 */
platform_status_t platform_iwdg_backend_refresh(void);

/**
 * @brief 报告本次复位是否由独立看门狗触发。
 */
bool platform_iwdg_backend_was_reset(void);

#endif //LAYERED_EMBEDDED_C_PLATFORM_IWDG_BACKEND_H

/**
 * @file platform_wwdg_backend.h
 * @brief STM32 系列公共 WWDG 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_WWDG_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_WWDG_BACKEND_H

#include "platform_def.h"
#include "platform_wwdg.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 返回窗口看门狗使用的 APB1 时钟频率。
 */
uint32_t platform_wwdg_backend_pclk1_hz(void);

/**
 * @brief 启动窗口看门狗后端，不负责 CubeMX 结构性初始化。
 */
platform_status_t platform_wwdg_backend_start(
  uint32_t divisor,
  uint8_t window
);

/**
 * @brief 刷新窗口看门狗。
 */
platform_status_t platform_wwdg_backend_refresh(void);

/**
 * @brief 报告本次复位是否由窗口看门狗触发。
 */
bool platform_wwdg_backend_was_reset(void);

#endif //LAYERED_EMBEDDED_C_PLATFORM_WWDG_BACKEND_H

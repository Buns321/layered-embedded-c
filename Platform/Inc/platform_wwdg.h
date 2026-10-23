//
// 由 Buns_ 创建于 2026/10/22。
//

/**
 * @file platform_wwdg.h
 * @brief 窗口看门狗的启动、刷新与复位标志接口。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_WWDG_H
#define LAYERED_EMBEDDED_C_PLATFORM_WWDG_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

#if !PLATFORM_ENABLE_WWDG
#error "Platform WWDG is not enabled"
#endif

/** @brief 可用的 WWDG 身份。 */
typedef enum {
  PLATFORM_WWDG_1 = 0U,
} Platform_WWDG_Instance;

/** @brief 有效的原始窗口最小值。 */
#define PLATFORM_WWDG_WINDOW_MIN 0x40u
/** @brief 有效的原始窗口最大值。 */
#define PLATFORM_WWDG_WINDOW_MAX 0x7Fu
/** @brief 当前 PCLK1 为 42 MHz 时的保守最大超时。 */
#define PLATFORM_WWDG_MAX_TIMEOUT_MS 49u

/**
 * @brief WWDG 运行期配置。
 */
typedef struct {
  uint32_t timeout_ms; /**< 请求的超时时间，范围 1..PLATFORM_WWDG_MAX_TIMEOUT_MS。 */
  uint8_t window;      /**< 原始刷新窗口，范围 PLATFORM_WWDG_WINDOW_MIN..MAX。 */
} Platform_WWDG_Config;

/**
 * @brief 启动窗口看门狗。
 * @param wwdg WWDG 实例。
 * @param config 运行期配置。
 * @return Platform 状态。
 * @note 这是运行期策略操作，会立即启动看门狗。
 * @note 调用后无法停止 WWDG。
 * @note 应在计数器低于 window 后、到达 0x3F 前刷新。
 */
Platform_StatusTypeDef Platform_WWDG_Start(
  Platform_WWDG_Instance wwdg,
  const Platform_WWDG_Config *config
);

/**
 * @brief 刷新已启动的 WWDG。
 * @return 成功返回 Platform_OK；WWDG 尚未启动时返回 Platform_ERROR。
 */
Platform_StatusTypeDef Platform_WWDG_Refresh(
  Platform_WWDG_Instance wwdg
);

/**
 * @brief 读取并清除 WWDG 复位标志。
 * @return 最近一次复位由 WWDG 触发时返回 true。
 * @note 清除复位标志会同时清除共享的 STM32 复位标志锁存状态。
 */
bool Platform_WWDG_WasReset(Platform_WWDG_Instance wwdg);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_WWDG_H

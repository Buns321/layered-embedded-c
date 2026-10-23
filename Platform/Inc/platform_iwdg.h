//
// 由 Buns_ 创建于 2026/10/22。
//

/**
 * @file platform_iwdg.h
 * @brief 独立看门狗的启动、刷新与复位标志接口。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_IWDG_H
#define LAYERED_EMBEDDED_C_PLATFORM_IWDG_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

#if !PLATFORM_ENABLE_IWDG
#error "Platform IWDG is not enabled"
#endif

/** @brief 可用的 IWDG 身份。 */
typedef enum {
  PLATFORM_IWDG_1 = 0U,
} Platform_IWDG_Instance;

/** @brief 基于各系列典型 LSI 频率可请求的最大超时。 */
#if defined(STM32F1xx)
#define PLATFORM_IWDG_MAX_TIMEOUT_MS 26208u
#elif defined(STM32F4xx)
#define PLATFORM_IWDG_MAX_TIMEOUT_MS 32760u
#else
#error "Unsupported STM32 family for IWDG"
#endif

/**
 * @brief 启动独立看门狗。
 * @param iwdg IWDG 实例。
 * @param timeout_ms 请求的超时时间（ms），范围 1..PLATFORM_IWDG_MAX_TIMEOUT_MS。
 * @return Platform 状态。
 * @note 这是运行期策略操作，会立即启动看门狗。
 * @note 调用后无法停止 IWDG。
 * @note 超时时间会向上取整，并受 LSI 频率容差影响。
 */
Platform_StatusTypeDef Platform_IWDG_Start(
  Platform_IWDG_Instance iwdg,
  uint32_t timeout_ms
);

/**
 * @brief 刷新已启动的 IWDG。
 * @return 成功返回 Platform_OK；IWDG 尚未启动时返回 Platform_ERROR。
 */
Platform_StatusTypeDef Platform_IWDG_Refresh(
  Platform_IWDG_Instance iwdg
);

/**
 * @brief 读取并清除 IWDG 复位标志。
 * @return 最近一次复位由 IWDG 触发时返回 true。
 * @note 清除复位标志会同时清除共享的 STM32 复位标志锁存状态。
 */
bool Platform_IWDG_WasReset(Platform_IWDG_Instance iwdg);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_IWDG_H

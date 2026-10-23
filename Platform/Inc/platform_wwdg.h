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
typedef enum platform_wwdg_instance_e {
  PLATFORM_WWDG_1 = 0U,
} platform_wwdg_instance_t;

/** @brief 有效的原始窗口最小值。 */
#define PLATFORM_WWDG_WINDOW_MIN 0x40u
/** @brief 有效的原始窗口最大值。 */
#define PLATFORM_WWDG_WINDOW_MAX 0x7Fu

/**
 * @brief WWDG 运行期配置。
 */
typedef struct platform_wwdg_config_s {
  uint32_t timeout_ms; /**< 请求超时，范围 1..Platform_WWDG_GetMaxTimeoutMs()。 */
  uint8_t window;      /**< 原始刷新窗口，范围 PLATFORM_WWDG_WINDOW_MIN..MAX。 */
} platform_wwdg_config_t;

/**
 * @brief 查询当前时钟下的最大可配置超时。
 * @param wwdg WWDG 实例。
 * @return 最大超时（ms）；实例非法或时钟不可用时返回 0。
 * @note 该值取决于真实 PCLK1，不能假设 F1/F4 相同。
 */
uint32_t Platform_WWDG_GetMaxTimeoutMs(platform_wwdg_instance_t wwdg);

/**
 * @brief 启动窗口看门狗。
 * @param wwdg WWDG 实例。
 * @param config 运行期配置。
 * @retval PLATFORM_STATUS_OK 启动成功。
 * @retval PLATFORM_STATUS_BUSY WWDG 已启动。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、窗口或超时范围非法。
 * @note 这是运行期策略操作，会立即启动看门狗。
 * @note 调用后无法停止 WWDG。
 * @note 应在计数器低于 window 后、到达 0x3F 前刷新。
 */
platform_status_t Platform_WWDG_Start(
  platform_wwdg_instance_t wwdg,
  const platform_wwdg_config_t *config
);

/**
 * @brief 刷新已启动的 WWDG。
 * @retval PLATFORM_STATUS_OK 刷新成功。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法。
 * @retval PLATFORM_STATUS_ERROR WWDG 尚未启动或 HAL 刷新失败。
 */
platform_status_t Platform_WWDG_Refresh(
  platform_wwdg_instance_t wwdg
);

/**
 * @brief 读取并清除 WWDG 复位标志。
 * @return 最近一次复位由 WWDG 触发时返回 true。
 * @note 非法实例返回 false，不访问硬件。
 * @note 清除复位标志会同时清除共享的 STM32 复位标志锁存状态。
 */
bool Platform_WWDG_WasReset(platform_wwdg_instance_t wwdg);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_WWDG_H

/**
 * @file platform_wwdg_common.c
 * @brief 与芯片系列无关的 WWDG 策略与状态管理。
 * @ingroup platform
 */

#include "platform_wwdg_backend.h"
#include <assert.h>
#include <stddef.h>

#define PLATFORM_WWDG_COUNTER_START 0x7Fu
#define PLATFORM_WWDG_COUNTER_MIN   0x3Fu
#define PLATFORM_WWDG_PRESCALER_BASE 4096u

static const uint32_t s_wwdg_divisors[] = {
  1u,
  2u,
  4u,
  8u,
};

static bool s_wwdg_started;

/**
 * @brief 查询当前时钟下的最大可配置超时。
 * @param wwdg WWDG 实例。
 * @return 最大超时（ms）；实例非法或时钟不可用时返回 0。
 * @note 该值取决于真实 PCLK1，不能假设 F1/F4 相同。
 */
uint32_t Platform_WWDG_GetMaxTimeoutMs(platform_wwdg_instance_t wwdg) {
  if (wwdg != PLATFORM_WWDG_1) {
    return 0U;
  }

  const uint64_t pclk1_hz = platform_wwdg_backend_pclk1_hz();
  if (pclk1_hz == 0U) {
    return 0U;
  }

  const uint64_t counter_steps =
    PLATFORM_WWDG_COUNTER_START - PLATFORM_WWDG_COUNTER_MIN;
  const uint64_t max_cycles =
    counter_steps *
    PLATFORM_WWDG_PRESCALER_BASE *
    s_wwdg_divisors[
      sizeof(s_wwdg_divisors) / sizeof(s_wwdg_divisors[0]) - 1u
    ];

  return (uint32_t)((max_cycles * 1000U) / pclk1_hz);
}

/**
 * @brief 计算 WWDG 预分频与窗口值。
 */
static bool platform_wwdg_calculate(
  uint32_t timeout_ms,
  uint32_t *divisor
) {
  assert(divisor != NULL);

  if (
    timeout_ms == 0U ||
    timeout_ms > Platform_WWDG_GetMaxTimeoutMs(PLATFORM_WWDG_1)
  ) {
    return false;
  }

  const uint64_t pclk1_hz = platform_wwdg_backend_pclk1_hz();
  const uint64_t required_cycles =
    (pclk1_hz / 1000u) * (uint64_t)timeout_ms;
  const uint64_t counter_steps =
    PLATFORM_WWDG_COUNTER_START - PLATFORM_WWDG_COUNTER_MIN;

  for (
    uint32_t i = 0U;
    i < sizeof(s_wwdg_divisors) / sizeof(s_wwdg_divisors[0]);
    i++
  ) {
    uint64_t available_cycles =
      counter_steps *
      PLATFORM_WWDG_PRESCALER_BASE *
      s_wwdg_divisors[i];

    if (available_cycles >= required_cycles) {
      *divisor = s_wwdg_divisors[i];
      return true;
    }
  }

  return false;
}

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
) {
  if (
    wwdg != PLATFORM_WWDG_1 ||
    config == NULL ||
    config->window < PLATFORM_WWDG_WINDOW_MIN ||
    config->window > PLATFORM_WWDG_WINDOW_MAX
  ) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (s_wwdg_started) {
    return PLATFORM_STATUS_BUSY;
  }

  uint32_t divisor;
  if (!platform_wwdg_calculate(config->timeout_ms, &divisor)) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  platform_status_t status =
    platform_wwdg_backend_start(divisor, config->window);
  if (status == PLATFORM_STATUS_OK) {
    s_wwdg_started = true;
  }
  return status;
}

/**
 * @brief 刷新已启动的 WWDG。
 * @retval PLATFORM_STATUS_OK 刷新成功。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法。
 * @retval PLATFORM_STATUS_ERROR WWDG 尚未启动或 HAL 刷新失败。
 */
platform_status_t Platform_WWDG_Refresh(
  platform_wwdg_instance_t wwdg
) {
  if (wwdg != PLATFORM_WWDG_1) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (!s_wwdg_started) {
    return PLATFORM_STATUS_ERROR;
  }
  return platform_wwdg_backend_refresh();
}

/**
 * @brief 读取并清除 WWDG 复位标志。
 * @return 最近一次复位由 WWDG 触发时返回 true。
 * @note 非法实例返回 false，不访问硬件。
 * @note 清除复位标志会同时清除共享的 STM32 复位标志锁存状态。
 */
bool Platform_WWDG_WasReset(platform_wwdg_instance_t wwdg) {
  if (wwdg != PLATFORM_WWDG_1) {
    return false;
  }
  return platform_wwdg_backend_was_reset();
}

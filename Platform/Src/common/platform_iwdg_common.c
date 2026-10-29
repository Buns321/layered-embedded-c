/**
 * @file platform_iwdg_common.c
 * @brief 与芯片系列无关的 IWDG 策略与状态管理。
 * @ingroup platform
 */

#include "platform_iwdg_backend.h"
#include <assert.h>
#include <stddef.h>

#define PLATFORM_IWDG_RELOAD_MAX 0x0FFFu

static const uint32_t s_iwdg_divisors[] = {
  4u,
  8u,
  16u,
  32u,
  64u,
  128u,
  256u,
};

static bool s_iwdg_started;

/**
 * @brief 计算 IWDG 预分频与重载值。
 */
static bool platform_iwdg_calculate(
  uint32_t timeout_ms,
  uint32_t *divisor,
  uint32_t *reload
) {
  assert(divisor != NULL && reload != NULL);

  if (timeout_ms == 0U || timeout_ms > PLATFORM_IWDG_MAX_TIMEOUT_MS) {
    return false;
  }

  const uint64_t required_ticks =
    (uint64_t)timeout_ms * platform_iwdg_backend_lsi_hz() / 1000u;

  for (
    uint32_t i = 0U;
    i < sizeof(s_iwdg_divisors) / sizeof(s_iwdg_divisors[0]);
    i++
  ) {
    uint64_t loaded_ticks =
      (required_ticks + s_iwdg_divisors[i] - 1u) /
      s_iwdg_divisors[i];

    if (loaded_ticks <= (uint64_t)PLATFORM_IWDG_RELOAD_MAX + 1u) {
      *divisor = s_iwdg_divisors[i];
      *reload = (uint32_t)(loaded_ticks - 1u);
      return true;
    }
  }

  return false;
}

/**
 * @brief 启动独立看门狗。
 * @param iwdg IWDG 实例。
 * @param timeout_ms 请求的超时时间（ms），范围 1..PLATFORM_IWDG_MAX_TIMEOUT_MS。
 * @retval PLATFORM_STATUS_OK 启动成功。
 * @retval PLATFORM_STATUS_BUSY IWDG 已启动。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、超时范围或计算参数非法。
 * @note 这是运行期策略操作，会立即启动看门狗。
 * @note 调用后无法停止 IWDG。
 * @note 超时时间会向上取整，并受 LSI 频率容差影响。
 */
platform_status_t Platform_IWDG_Start(
  platform_iwdg_instance_t iwdg,
  uint32_t timeout_ms
) {
  if (iwdg != PLATFORM_IWDG_1) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (s_iwdg_started) {
    return PLATFORM_STATUS_BUSY;
  }

  uint32_t divisor;
  uint32_t reload;
  if (!platform_iwdg_calculate(timeout_ms, &divisor, &reload)) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  platform_status_t status =
    platform_iwdg_backend_start(divisor, reload);
  if (status == PLATFORM_STATUS_OK) {
    s_iwdg_started = true;
  }
  return status;
}

/**
 * @brief 刷新已启动的 IWDG。
 * @retval PLATFORM_STATUS_OK 刷新成功。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法。
 * @retval PLATFORM_STATUS_ERROR IWDG 尚未启动或 HAL 刷新失败。
 */
platform_status_t Platform_IWDG_Refresh(
  platform_iwdg_instance_t iwdg
) {
  if (iwdg != PLATFORM_IWDG_1) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (!s_iwdg_started) {
    return PLATFORM_STATUS_ERROR;
  }
  return platform_iwdg_backend_refresh();
}

/**
 * @brief 读取并清除 IWDG 复位标志。
 * @return 最近一次复位由 IWDG 触发时返回 true。
 * @note 非法实例返回 false，不访问硬件。
 * @note 清除复位标志会同时清除共享的 STM32 复位标志锁存状态。
 */
bool Platform_IWDG_WasReset(platform_iwdg_instance_t iwdg) {
  if (iwdg != PLATFORM_IWDG_1) {
    return false;
  }
  return platform_iwdg_backend_was_reset();
}

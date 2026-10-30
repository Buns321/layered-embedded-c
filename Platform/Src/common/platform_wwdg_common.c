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
 * @param wwdg WWDG 实例；当前系列只支持 PLATFORM_WWDG_1。
 * @retval 0 实例非法或无法取得 PCLK1。
 * @retval 大于0 当前时钟和最大预分频下的毫秒上限。
 * @note 该值取决于真实 PCLK1，不能假设 F1/F4 相同。
 * @note 纯查询，不启动或刷新看门狗，不分配内存。
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
 * @brief 根据超时时间选择 WWDG 预分频值。
 * @param timeout_ms 请求超时时间，单位毫秒；合法范围为 1..GetMaxTimeoutMs()。
 * @param divisor 分频值输出地址；成功时写入 1、2、4 或 8。
 * @return 找到可覆盖 timeout_ms 的分频值时返回 true；为零、过大或不可覆盖时返回 false。
 * @pre divisor 非空；Debug 构建以 assert 检查该内部不变量。
 * @post 成功时写入 divisor；失败时不修改输出。
 * @note 窗口值不参与本计算；不访问硬件或分配内存。
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
 * @param wwdg WWDG 实例；当前系列只支持 PLATFORM_WWDG_1。
 * @param config 运行期配置；timeout_ms 取 1..GetMaxTimeoutMs()，window 取
 *        PLATFORM_WWDG_WINDOW_MIN..PLATFORM_WWDG_WINDOW_MAX。
 * @retval PLATFORM_STATUS_OK 启动成功。
 * @retval PLATFORM_STATUS_BUSY WWDG 已启动。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、窗口或超时范围非法。
 * @retval PLATFORM_STATUS_ERROR HAL 配置或启动失败。
 * @note 这是运行期策略操作，会立即启动看门狗。
 * @note 调用后无法停止 WWDG。
 * @note 应在计数器低于 window 后、到达 0x3F 前刷新。
 * @warning 只允许在任务上下文配置一次；config 只在调用期间被读取，平台不保存其指针。
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
 * @param wwdg WWDG 实例；当前仅支持 PLATFORM_WWDG_1，非法值返回 PLATFORM_STATUS_INVALID_ARGUMENT。
 * @retval PLATFORM_STATUS_OK 刷新成功。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法。
 * @retval PLATFORM_STATUS_ERROR WWDG 尚未启动或 HAL 刷新失败。
 * @warning 刷新过早会触发窗口复位；刷新必须在允许窗口内执行。
 * @note 本函数不分配内存，也不改变超时或窗口配置。
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
 * @param wwdg WWDG 实例；当前仅支持 PLATFORM_WWDG_1，非法值返回 false。
 * @retval true 最近一次复位由 WWDG 触发，且标志已清除。
 * @retval false 未由 WWDG 触发复位，或实例非法。
 * @note 非法实例返回 false，不访问硬件。
 * @note 清除复位标志会同时清除共享的 STM32 复位标志锁存状态。
 * @warning 该调用具有一次性读取语义；其他复位标志消费者应在同一上下文协调。
 */
bool Platform_WWDG_WasReset(platform_wwdg_instance_t wwdg) {
  if (wwdg != PLATFORM_WWDG_1) {
    return false;
  }
  return platform_wwdg_backend_was_reset();
}

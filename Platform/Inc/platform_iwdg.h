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
typedef enum platform_iwdg_instance_e {
  PLATFORM_IWDG_1 = 0U,
} platform_iwdg_instance_t;

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
 * @param iwdg IWDG 实例；当前系列只支持 PLATFORM_IWDG_1。
 * @param timeout_ms 请求的超时时间（ms），范围 1..PLATFORM_IWDG_MAX_TIMEOUT_MS。
 * @retval PLATFORM_STATUS_OK 启动成功。
 * @retval PLATFORM_STATUS_BUSY IWDG 已启动。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、超时范围或计算参数非法。
 * @retval PLATFORM_STATUS_ERROR HAL 配置或启动失败。
 * @note 这是运行期策略操作，会立即启动看门狗。
 * @note 调用后无法停止 IWDG。
 * @note 超时时间会向上取整，并受 LSI 频率容差影响。
 * @warning 只允许在任务上下文配置一次；调用方负责与其他监控路径串行化。
 */
platform_status_t Platform_IWDG_Start(
  platform_iwdg_instance_t iwdg,
  uint32_t timeout_ms
);

/**
 * @brief 刷新已启动的 IWDG。
 * @param iwdg IWDG 实例；当前仅支持 PLATFORM_IWDG_1，非法值返回 PLATFORM_STATUS_INVALID_ARGUMENT。
 * @retval PLATFORM_STATUS_OK 刷新成功。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法。
 * @retval PLATFORM_STATUS_ERROR IWDG 尚未启动或 HAL 刷新失败。
 * @note 刷新必须早于看门狗到期；本函数不分配内存，也不改变超时配置。
 */
platform_status_t Platform_IWDG_Refresh(
  platform_iwdg_instance_t iwdg
);

/**
 * @brief 读取并清除 IWDG 复位标志。
 * @param iwdg IWDG 实例；当前仅支持 PLATFORM_IWDG_1，非法值返回 false。
 * @retval true 最近一次复位由 IWDG 触发，且标志已清除。
 * @retval false 未由 IWDG 触发复位，或实例非法。
 * @note 非法实例返回 false，不访问硬件。
 * @note 清除复位标志会同时清除共享的 STM32 复位标志锁存状态。
 * @warning 该调用具有一次性读取语义；其他复位标志消费者应在同一上下文协调。
 */
bool Platform_IWDG_WasReset(platform_iwdg_instance_t iwdg);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_IWDG_H

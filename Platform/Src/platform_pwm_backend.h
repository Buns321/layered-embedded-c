/**
 * @file platform_pwm_backend.h
 * @brief STM32 系列公共 PWM 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_PWM_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_PWM_BACKEND_H

#include "platform_def.h"
#include "platform_pwm.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 校验PWM实例是否已由板级配置启用。
 */
bool platform_pwm_backend_is_valid(platform_pwm_instance_t pwm);

/**
 * @brief 启动PWM后端，不负责 CubeMX 结构性初始化。
 */
platform_status_t platform_pwm_backend_start(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel
);

/**
 * @brief 启动 PWM 后端全部通道。
 */
platform_status_t platform_pwm_backend_start_all(
  platform_pwm_instance_t pwm
);

/**
 * @brief 停止PWM后端并清理活动状态。
 */
platform_status_t platform_pwm_backend_stop(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel
);

/**
 * @brief 停止 PWM 后端全部通道。
 */
platform_status_t platform_pwm_backend_stop_all(
  platform_pwm_instance_t pwm
);

/**
 * @brief 把比较值写入 PWM 定时器 CCR。
 */
platform_status_t platform_pwm_backend_set_compare(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
  uint32_t compare
);

/**
 * @brief 按 ARR 与 max_duty 换算并写入 PWM 比较值。
 */
platform_status_t platform_pwm_backend_set_duty(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
  uint32_t max_duty,
  uint32_t duty
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_PWM_BACKEND_H

/**
 * @file platform_pwm_common.c
 * @brief 与芯片系列无关的 PWM 参数校验与错误快照。
 * @ingroup platform
 */

#include "platform_error_internal.h"
#include "platform_pwm_backend.h"

static volatile uint32_t pwm_error[PLATFORM_PWM_INSTANCE_COUNT];

/**
 * @brief 把 PWM 实例编号转换为错误快照下标。
 */
static uint32_t platform_pwm_error_index(platform_pwm_instance_t pwm) {
  return (uint32_t)pwm;
}

/**
 * @brief 把 HAL PWM 结果转换为公共状态并记录错误。
 */
static platform_status_t platform_pwm_finish(
  platform_pwm_instance_t pwm,
  platform_status_t status
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(
      &pwm_error[platform_pwm_error_index(pwm)],
      platform_error_from_status(status)
    );
  }
  return status;
}

/**
 * @brief 校验 PWM 通道枚举。
 */
static bool platform_pwm_channel_valid(platform_pwm_channel_t channel) {
  return (uint32_t)channel <= (uint32_t)PLATFORM_PWM_CHANNEL_4;
}

/**
 * @brief 构造 PWM 非法参数状态。
 */
static platform_status_t platform_pwm_invalid(
  platform_pwm_instance_t pwm,
  bool instance_valid
) {
  if (instance_valid) {
    platform_error_record(
      &pwm_error[platform_pwm_error_index(pwm)],
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

/**
 * @brief 启动指定 PWM 定时器的单通道输出。
 * @param pwm PWM 定时器身份。
 * @param channel 通道身份。
 * @retval PLATFORM_STATUS_OK 成功。
 * @retval PLATFORM_STATUS_ERROR 失败。
 * @retval PLATFORM_STATUS_BUSY 忙。
 * @pre 对应定时器的 MX_TIMx_Init() 已完成并配置为 PWM 模式。
 */
platform_status_t Platform_PWM_Start(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel
) {
  const bool instance_valid = platform_pwm_backend_is_valid(pwm);
  if (instance_valid) {
    platform_error_reset(&pwm_error[platform_pwm_error_index(pwm)]);
  }
  if (!instance_valid || !platform_pwm_channel_valid(channel)) {
    return platform_pwm_invalid(pwm, instance_valid);
  }
  return platform_pwm_finish(
    pwm,
    platform_pwm_backend_start(pwm, channel)
  );
}

/**
 * @brief 启动指定 PWM 定时器的全部四个通道。
 * @param pwm PWM 定时器身份。
 * @retval PLATFORM_STATUS_OK 成功。
 * @retval PLATFORM_STATUS_ERROR 失败。
 * @retval PLATFORM_STATUS_BUSY 忙。
 * @pre 对应定时器的 MX_TIMx_Init() 已完成并配置为 PWM 模式。
 */
platform_status_t Platform_PWM_StartAll(platform_pwm_instance_t pwm) {
  const bool instance_valid = platform_pwm_backend_is_valid(pwm);
  if (instance_valid) {
    platform_error_reset(&pwm_error[platform_pwm_error_index(pwm)]);
  }
  if (!instance_valid) {
    return platform_pwm_invalid(pwm, false);
  }
  return platform_pwm_finish(pwm, platform_pwm_backend_start_all(pwm));
}

/**
 * @brief 停止指定 PWM 定时器的单通道输出。
 * @param pwm PWM 定时器身份。
 * @param channel 通道身份。
 * @retval PLATFORM_STATUS_OK 成功。
 * @retval PLATFORM_STATUS_ERROR 失败。
 * @retval PLATFORM_STATUS_BUSY 忙。
 */
platform_status_t Platform_PWM_Stop(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel
) {
  const bool instance_valid = platform_pwm_backend_is_valid(pwm);
  if (instance_valid) {
    platform_error_reset(&pwm_error[platform_pwm_error_index(pwm)]);
  }
  if (!instance_valid || !platform_pwm_channel_valid(channel)) {
    return platform_pwm_invalid(pwm, instance_valid);
  }
  return platform_pwm_finish(
    pwm,
    platform_pwm_backend_stop(pwm, channel)
  );
}

/**
 * @brief 停止指定 PWM 定时器的全部四个通道。
 * @param pwm PWM 定时器身份。
 * @retval PLATFORM_STATUS_OK 成功。
 * @retval PLATFORM_STATUS_ERROR 失败。
 * @retval PLATFORM_STATUS_BUSY 忙。
 */
platform_status_t Platform_PWM_StopAll(platform_pwm_instance_t pwm) {
  const bool instance_valid = platform_pwm_backend_is_valid(pwm);
  if (instance_valid) {
    platform_error_reset(&pwm_error[platform_pwm_error_index(pwm)]);
  }
  if (!instance_valid) {
    return platform_pwm_invalid(pwm, false);
  }
  return platform_pwm_finish(pwm, platform_pwm_backend_stop_all(pwm));
}

/**
 * @brief 设置比较值。
 * @param pwm PWM 定时器身份。
 * @param channel 通道身份。
 * @param compare 比较值，内部会夹紧到 ARR。
 * @retval PLATFORM_STATUS_OK 设置成功。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或通道非法。
 */
platform_status_t Platform_PWM_SetCompare(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
  uint32_t compare
) {
  const bool instance_valid = platform_pwm_backend_is_valid(pwm);
  if (instance_valid) {
    platform_error_reset(&pwm_error[platform_pwm_error_index(pwm)]);
  }
  if (!instance_valid || !platform_pwm_channel_valid(channel)) {
    return platform_pwm_invalid(pwm, instance_valid);
  }
  return platform_pwm_finish(
    pwm,
    platform_pwm_backend_set_compare(pwm, channel, compare)
  );
}

/**
 * @brief 按占空比设置比较值。
 * @param pwm PWM 定时器身份。
 * @param channel 通道身份。
 * @param max_duty 占空比量程，必须大于 0。
 * @param duty 目标占空比，内部会夹紧到 max_duty。
 * @note 实际 CCR = ARR * duty / max_duty，使用 64 位中间量避免溢出。
 * @retval PLATFORM_STATUS_OK 设置成功。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、通道或量程非法。
 */
platform_status_t Platform_PWM_SetDuty(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
  uint32_t max_duty,
  uint32_t duty
) {
  const bool instance_valid = platform_pwm_backend_is_valid(pwm);
  if (instance_valid) {
    platform_error_reset(&pwm_error[platform_pwm_error_index(pwm)]);
  }
  if (
    !instance_valid ||
    !platform_pwm_channel_valid(channel) ||
    max_duty == 0U
  ) {
    return platform_pwm_invalid(pwm, instance_valid);
  }
  return platform_pwm_finish(
    pwm,
    platform_pwm_backend_set_duty(pwm, channel, max_duty, duty)
  );
}

/** @brief 读取最近一次 PWM 操作错误快照。 */
uint32_t Platform_PWM_GetLastError(platform_pwm_instance_t pwm) {
  if (!platform_pwm_backend_is_valid(pwm)) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return pwm_error[platform_pwm_error_index(pwm)];
}

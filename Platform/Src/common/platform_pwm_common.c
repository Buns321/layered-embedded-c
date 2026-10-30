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
 * @param pwm PWM 定时器身份。
 * @return 与枚举值相同的快照下标。
 * @pre 调用方必须先通过 platform_pwm_backend_is_valid() 确认 pwm 有效。
 * @note 本函数不做范围检查，非法值会形成越界下标；仅供本文件合法路径使用。
 */
static uint32_t platform_pwm_error_index(platform_pwm_instance_t pwm) {
  return (uint32_t)pwm;
}

/**
 * @brief 把 PWM 后端结果转换为公共状态并保留失败快照。
 * @param pwm 已确认有效的 PWM 定时器身份。
 * @param status 后端返回的平台状态。
 * @return 原样返回 status，便于调用方直接返回。
 * @post status 非 OK 时把对应通用错误位合并到该实例快照；OK 不修改快照。
 * @note 仅供本文件公共操作收尾使用；不访问硬件，不分配内存。
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
 * @param channel 待校验的通道身份。
 * @return channel 为通道 1..4 时返回 true；ALL 或其他值返回 false。
 * @note 纯函数；显式拒绝 PLATFORM_PWM_CHANNEL_ALL，避免误操作固定 CCR 通道。
 */
static bool platform_pwm_channel_valid(platform_pwm_channel_t channel) {
  return (uint32_t)channel <= (uint32_t)PLATFORM_PWM_CHANNEL_4;
}

/**
 * @brief 记录 PWM 参数错误并构造统一失败状态。
 * @param pwm PWM 定时器身份。
 * @param instance_valid true 表示 pwm 可安全索引错误快照数组。
 * @return 始终返回 PLATFORM_STATUS_INVALID_ARGUMENT。
 * @post instance_valid 为 true 时记录 PLATFORM_ERROR_INVALID_ARGUMENT；
 *       否则不访问快照数组。
 * @note 仅供本文件失败路径使用；不访问 PWM 硬件，不分配内存。
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
 * @param pwm 已启用的 PWM 定时器身份。
 * @param channel 通道 1..4；不得传入 PLATFORM_PWM_CHANNEL_ALL。
 * @retval PLATFORM_STATUS_OK 通道输出已启动。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或通道非法。
 * @retval PLATFORM_STATUS_BUSY HAL 报告该定时器忙。
 * @retval PLATFORM_STATUS_ERROR HAL 启动失败。
 * @pre 对应定时器的 MX_TIMx_Init() 已完成并配置为 PWM 模式。
 * @note 同一实例的启动、停止和比较值修改必须由调用方串行化；不分配内存。
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
 * @param pwm 已启用的 PWM 定时器身份。
 * @retval PLATFORM_STATUS_OK 两个或四个已配置通道均由后端启动。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法。
 * @retval PLATFORM_STATUS_BUSY HAL 报告该定时器忙。
 * @retval PLATFORM_STATUS_ERROR HAL 启动失败。
 * @pre 对应定时器的 MX_TIMx_Init() 已完成并配置为 PWM 模式。
 * @note 启动范围由后端按定时器实际通道数决定；同一实例操作须串行化。
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
 * @param pwm 已启用的 PWM 定时器身份。
 * @param channel 通道 1..4；不得传入 PLATFORM_PWM_CHANNEL_ALL。
 * @retval PLATFORM_STATUS_OK 通道输出已停止。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或通道非法。
 * @retval PLATFORM_STATUS_BUSY HAL 报告该定时器忙。
 * @retval PLATFORM_STATUS_ERROR HAL 停止失败。
 * @note 停止不修改比较值或占空比配置；同一实例操作须由调用方串行化。
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
 * @param pwm 已启用的 PWM 定时器身份。
 * @retval PLATFORM_STATUS_OK 已配置通道输出均已停止。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法。
 * @retval PLATFORM_STATUS_BUSY HAL 报告该定时器忙。
 * @retval PLATFORM_STATUS_ERROR HAL 停止失败。
 * @note 停止范围由后端按定时器实际通道数决定；不分配内存。
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
 * @param pwm 已启用的 PWM 定时器身份。
 * @param channel 通道 1..4；不得传入 PLATFORM_PWM_CHANNEL_ALL。
 * @param compare 目标 CCR 原始计数值；超过当前 ARR 时后端夹紧到 ARR。
 * @retval PLATFORM_STATUS_OK 设置成功。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或通道非法。
 * @retval PLATFORM_STATUS_ERROR 后端写入失败。
 * @pre 定时器时钟与 ARR 已由 CubeMX 配置。
 * @note 本操作不启动通道；同一实例比较值更新须与启停操作串行化。
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
 * @param pwm 已启用的 PWM 定时器身份。
 * @param channel 通道 1..4；不得传入 PLATFORM_PWM_CHANNEL_ALL。
 * @param max_duty 占空比量程（无量纲计数），必须大于 0。
 * @param duty 目标占空比（与 max_duty 同量纲）；超过 max_duty 时夹紧。
 * @retval PLATFORM_STATUS_OK 设置成功。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、通道或量程非法。
 * @retval PLATFORM_STATUS_ERROR 后端写入失败。
 * @note 实际 CCR = ARR * min(duty, max_duty) / max_duty，使用 64 位中间量避免溢出。
 * @note 本操作不启动通道；量程仅用于换算，不存入硬件。
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

/**
 * @brief 读取最近一次 PWM 操作错误快照。
 * @param pwm PWM 定时器身份。
 * @return 该实例的 PLATFORM_ERROR_* 位掩码；非法实例返回
 *         PLATFORM_ERROR_INVALID_ARGUMENT。
 * @note 只读，不清除快照；后续公共 PWM 操作开始时会清除对应快照。
 * @note 不访问硬件更新错误；并发读写同一快照须由调用方串行化。
 */
uint32_t Platform_PWM_GetLastError(platform_pwm_instance_t pwm) {
  if (!platform_pwm_backend_is_valid(pwm)) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return pwm_error[platform_pwm_error_index(pwm)];
}

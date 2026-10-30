//
// 由 Buns_ 创建于 2026/8/23。
//

/**
 * @file platform_pwm.h
 * @brief PWM 抽象：启动/停止、比较值与占空比设置。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_PWM_H
#define LAYERED_EMBEDDED_C_PLATFORM_PWM_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

/**
 * @brief 可用的 PWM 定时器身份。
 * @note 未启用的实例不会进入该枚举。
 */
typedef enum platform_pwm_instance_e {
#if PLATFORM_ENABLE_PWM_TIM1
  PLATFORM_PWM_TIM1 = 0,
#endif
#if PLATFORM_ENABLE_PWM_TIM2
  PLATFORM_PWM_TIM2 = 2,
#endif
#if PLATFORM_ENABLE_PWM_TIM3
  PLATFORM_PWM_TIM3 = 3,
#endif
#if PLATFORM_ENABLE_PWM_TIM4
  PLATFORM_PWM_TIM4 = 4,
#endif
} platform_pwm_instance_t;

/** @brief PWM 身份槽位数量上限。 */
#define PLATFORM_PWM_INSTANCE_COUNT 5u

/**
 * @brief PWM 通道身份。
 */
typedef enum platform_pwm_channel_e {
  PLATFORM_PWM_CHANNEL_1 = 0U,
  PLATFORM_PWM_CHANNEL_2,
  PLATFORM_PWM_CHANNEL_3,
  PLATFORM_PWM_CHANNEL_4,
} platform_pwm_channel_t;

#if !PLATFORM_ENABLE_PWM_TIM1 && !PLATFORM_ENABLE_PWM_TIM2 && \
    !PLATFORM_ENABLE_PWM_TIM3 && !PLATFORM_ENABLE_PWM_TIM4
#error "At least one Platform PWM instance must be enabled"
#endif

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
);

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
platform_status_t Platform_PWM_StartAll(platform_pwm_instance_t pwm);

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
);

/**
 * @brief 停止指定 PWM 定时器的全部四个通道。
 * @param pwm 已启用的 PWM 定时器身份。
 * @retval PLATFORM_STATUS_OK 已配置通道输出均已停止。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法。
 * @retval PLATFORM_STATUS_BUSY HAL 报告该定时器忙。
 * @retval PLATFORM_STATUS_ERROR HAL 停止失败。
 * @note 停止范围由后端按定时器实际通道数决定；不分配内存。
 */
platform_status_t Platform_PWM_StopAll(platform_pwm_instance_t pwm);

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
);

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
);

/**
 * @brief 读取最近一次 PWM 操作错误快照。
 * @param pwm PWM 定时器身份。
 * @return 该实例的 PLATFORM_ERROR_* 位掩码；非法实例返回
 *         PLATFORM_ERROR_INVALID_ARGUMENT。
 * @note 只读，不清除快照；后续公共 PWM 操作开始时会清除对应快照。
 * @note 不访问硬件更新错误；并发读写同一快照须由调用方串行化。
 */
uint32_t Platform_PWM_GetLastError(platform_pwm_instance_t pwm);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_PWM_H

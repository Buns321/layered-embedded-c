/**
 * @file platform_pwm.c
 * @brief STM32F4 公共 PWM 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_hal_internal.h"
#include "platform_pwm_backend.h"
#include "stm32f4xx_hal.h"
#include "tim.h"

/**
 * @brief 把平台 PWM 实例身份映射到 CubeMX 生成的 HAL 定时器句柄。
 * @param pwm PWM 定时器身份。
 * @return 对应 TIM handle 的地址；实例未启用或超出映射时返回 NULL。
 * @note 纯映射，不读取定时器状态；返回指针归 CubeMX/Core 所有，调用方不得释放或替换。
 */
static TIM_HandleTypeDef *platform_pwm_to_hal(platform_pwm_instance_t pwm) {
  switch (pwm) {
#if PLATFORM_ENABLE_PWM_TIM1
    case PLATFORM_PWM_TIM1: return &htim1;
#endif
#if PLATFORM_ENABLE_PWM_TIM2
    case PLATFORM_PWM_TIM2: return &htim2;
#endif
#if PLATFORM_ENABLE_PWM_TIM3
    case PLATFORM_PWM_TIM3: return &htim3;
#endif
#if PLATFORM_ENABLE_PWM_TIM4
    case PLATFORM_PWM_TIM4: return &htim4;
#endif
    default: return NULL;
  }
}

/**
 * @brief 把 PWM 通道身份转换为 HAL 通道宏。
 * @param channel PWM 通道，必须是 1..4；ALL 不由本函数处理。
 * @retval 0U channel 不是 1..4，调用方应将其视为前置条件违规。
 * @note 纯映射，不访问硬件，不分配内存。
 */
static uint32_t platform_pwm_channel_to_hal(platform_pwm_channel_t channel) {
  switch (channel) {
    case PLATFORM_PWM_CHANNEL_1: return TIM_CHANNEL_1;
    case PLATFORM_PWM_CHANNEL_2: return TIM_CHANNEL_2;
    case PLATFORM_PWM_CHANNEL_3: return TIM_CHANNEL_3;
    case PLATFORM_PWM_CHANNEL_4: return TIM_CHANNEL_4;
    default: return 0U;
  }
}

/**
 * @brief 判断 PWM 实例是否映射到本系列已启用的 HAL 定时器。
 * @param pwm PWM 定时器身份。
 * @retval true 实例已启用且存在对应 HAL handle。
 * @retval false 实例未启用或超出本系列映射。
 * @note 只查编译期映射，不读取 HAL 状态；纯读取，可在任务或中断上下文调用。
 */
bool platform_pwm_backend_is_valid(platform_pwm_instance_t pwm) {
  return platform_pwm_to_hal(pwm) != NULL;
}

/**
 * @brief 启动指定 PWM 定时器的一个通道。
 * @param pwm PWM 定时器身份。
 * @param channel PWM 通道，必须是 1..4；本后端不接受 ALL。
 * @retval PLATFORM_STATUS_OK HAL 已接受启动请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT pwm 没有对应 HAL handle。
 * @retval PLATFORM_STATUS_ERROR HAL 启动失败。
 * @pre 对应 MX_TIMx_Init() 已完成且通道配置为 PWM；channel 已由公共层校验。
 * @post 成功时通道输出使能；失败时的寄存器状态由 HAL 决定。
 * @note 不分配内存、不加锁；同一 TIM 的并发启动或停止必须由调用方串行化。
 */
platform_status_t platform_pwm_backend_start(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel
) {
  TIM_HandleTypeDef *pwm_h = platform_pwm_to_hal(pwm);
  return pwm_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_TIM_PWM_Start(
                       pwm_h,
                       platform_pwm_channel_to_hal(channel)
                     ));
}

/**
 * @brief 启动指定 PWM 定时器的全部四个通道。
 * @param pwm PWM 定时器身份。
 * @retval PLATFORM_STATUS_OK HAL 已接受全部通道的启动请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT pwm 没有对应 HAL handle。
 * @retval PLATFORM_STATUS_ERROR HAL 启动失败。
 * @pre 对应 MX_TIMx_Init() 已完成且四个目标通道配置为 PWM。
 * @post 成功时四个通道输出使能；失败时的寄存器状态由 HAL 决定。
 * @note 不分配内存、不加锁；同一 TIM 的并发启动或停止必须由调用方串行化。
 */
platform_status_t platform_pwm_backend_start_all(
  platform_pwm_instance_t pwm
) {
  TIM_HandleTypeDef *pwm_h = platform_pwm_to_hal(pwm);
  return pwm_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_TIM_PWM_Start(pwm_h, TIM_CHANNEL_ALL)
                     );
}

/**
 * @brief 停止指定 PWM 定时器的一个通道。
 * @param pwm PWM 定时器身份。
 * @param channel PWM 通道，必须是 1..4；本后端不接受 ALL。
 * @retval PLATFORM_STATUS_OK HAL 已接受停止请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT pwm 没有对应 HAL handle。
 * @pre channel 已由公共层校验；调用方不再依赖该通道继续输出。
 * @post 成功时对应比较输出关闭，HAL 同时停用整个定时器并把通道状态置为 READY。
 * @note 不分配内存、不加锁；同一 TIM 的并发启动或停止必须由调用方串行化。
 */
platform_status_t platform_pwm_backend_stop(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel
) {
  TIM_HandleTypeDef *pwm_h = platform_pwm_to_hal(pwm);
  return pwm_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_TIM_PWM_Stop(
                       pwm_h,
                       platform_pwm_channel_to_hal(channel)
                     ));
}

/**
 * @brief 停止指定 PWM 定时器的全部四个通道。
 * @param pwm PWM 定时器身份。
 * @retval PLATFORM_STATUS_OK HAL 已接受全部通道的停止请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT pwm 没有对应 HAL handle。
 * @post 成功时四个比较输出关闭，HAL 同时停用整个定时器。
 * @note 不分配内存、不加锁；同一 TIM 的并发启动或停止必须由调用方串行化。
 */
platform_status_t platform_pwm_backend_stop_all(
  platform_pwm_instance_t pwm
) {
  TIM_HandleTypeDef *pwm_h = platform_pwm_to_hal(pwm);
  return pwm_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_TIM_PWM_Stop(pwm_h, TIM_CHANNEL_ALL)
                     );
}

/**
 * @brief 把计数值写入指定 PWM 通道的 CCR。
 * @param pwm PWM 定时器身份。
 * @param channel PWM 通道，必须是 1..4；本后端不接受 ALL。
 * @param compare 定时器比较计数值，范围 0..UINT32_MAX；超过 ARR 时夹紧到 ARR。
 * @retval PLATFORM_STATUS_OK 已写入目标 CCR。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT pwm 没有对应 HAL handle。
 * @pre channel 已由公共层校验；对应定时器已由 CubeMX 完成结构配置。
 * @post 指定 CCR 更新，实际生效时刻遵循该通道的预装载配置。
 * @note 快速寄存器路径，不分配内存、不等待更新事件；并发写同一 CCR 时由调用顺序决定结果。
 */
platform_status_t platform_pwm_backend_set_compare(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
  uint32_t compare
) {
  TIM_HandleTypeDef *pwm_h = platform_pwm_to_hal(pwm);
  if (pwm_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  const uint32_t autoreload = __HAL_TIM_GET_AUTORELOAD(pwm_h);
  if (compare > autoreload) {
    compare = autoreload;
  }
  __HAL_TIM_SET_COMPARE(
    pwm_h,
    platform_pwm_channel_to_hal(channel),
    compare
  );
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 按量程把占空比换算为 CCR 并写入目标通道。
 * @param pwm PWM 定时器身份。
 * @param channel PWM 通道，必须是 1..4；本后端不接受 ALL。
 * @param max_duty 占空比量程，无量纲，必须大于 0；不要求等于 ARR。
 * @param duty 目标占空比，无量纲，范围 0..max_duty；超过量程时夹紧。
 * @retval PLATFORM_STATUS_OK 已按 CCR = ARR * duty / max_duty 写入。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT pwm 没有对应 HAL handle，或 max_duty 为 0。
 * @pre channel 已由公共层校验；定时器已配置有效 ARR。
 * @post 目标 CCR 更新，计算使用 64 位中间量避免乘积溢出。
 * @note 快速寄存器路径，不分配内存；并发调用必须由调用方串行化。
 */
platform_status_t platform_pwm_backend_set_duty(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
  uint32_t max_duty,
  uint32_t duty
) {
  TIM_HandleTypeDef *pwm_h = platform_pwm_to_hal(pwm);
  if (pwm_h == NULL || max_duty == 0U) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (duty > max_duty) {
    duty = max_duty;
  }
  const uint32_t autoreload = __HAL_TIM_GET_AUTORELOAD(pwm_h);
  const uint32_t compare = (uint32_t)(
    ((uint64_t)autoreload * duty) / max_duty
  );
  return platform_pwm_backend_set_compare(pwm, channel, compare);
}

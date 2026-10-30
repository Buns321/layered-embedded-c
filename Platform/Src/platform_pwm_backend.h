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
 * @brief 判断 PWM 实例是否映射到本系列已启用的 HAL 定时器。
 * @param pwm PWM 定时器身份。
 * @retval true 实例已启用且存在对应 HAL handle。
 * @retval false 实例未启用或超出本系列映射。
 * @note 只查编译期映射，不读取 HAL 状态；纯读取，可在任务或中断上下文调用。
 */
bool platform_pwm_backend_is_valid(platform_pwm_instance_t pwm);

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
);

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
);

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
);

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
);

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
);

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
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_PWM_BACKEND_H

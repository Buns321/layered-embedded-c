/**
 * @file filter_lowpass.h
 * @brief 多通道一阶低通滤波器（支持逐通道时间常数，单位 s）。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          基于离散一阶 RC 低通传递函数 y[n] = y[n-1] + alpha * (x[n] - y[n-1])，
 *          其中 alpha = dt / (tau + dt)，按通道独立跟踪上次输出与毫秒时间戳。
 *
 * @addtogroup Framework_Control
 * @{
 */

#ifndef LAYERED_CAR_FILTER_LOWPASS_H
#define LAYERED_CAR_FILTER_LOWPASS_H

#include "filter_base.h"
#include <stdint.h>

/**
 * @brief 一阶低通滤波器静态对象（base 必须为首成员）。
 */
typedef struct filter_lowpass_s {
  filter_base_t base;                         /**< 多态基类（必须为首成员）。 */
  float last_output[FILTER_CHANNEL_CAPACITY]; /**< 各通道上一有效输出值（单位同对应输入通道）。 */
  uint32_t last_time_ms[FILTER_CHANNEL_CAPACITY]; /**< 各通道上次有效更新时间戳（单位：ms）。 */
  float time_constant_s[FILTER_CHANNEL_CAPACITY]; /**< 各通道一阶低通时间常数 tau（单位：s，须 >
                                                     0）。 */
  uint32_t initialized_mask; /**< 各通道首帧初始化完成掩码；bit[i]==1 表示已有历史状态。 */
} filter_lowpass_t;

/**
 * @brief 初始化低通滤波器并设置全部通道默认时间常数（default_time_constant_s > 0，单位 s）。
 * @param[out] filter_h                待初始化的低通滤波器指针（非 NULL）。
 * @param[in]  name                    滤波器名称字符串（非 NULL）。
 * @param[in]  default_time_constant_s 全部通道默认时间常数（单位：s，须为正有限值 > 0）。
 * @return 初始化状态码。
 * @retval FILTER_STATUS_OK               初始化成功且全部通道历史状态已复位。
 * @retval FILTER_STATUS_INVALID_ARGUMENT filter_h 或 name 为 NULL。
 * @retval FILTER_STATUS_INVALID_CONFIG   default_time_constant_s <= 0 或非有限。
 */
filter_status_t Filter_Lowpass_Init(filter_lowpass_t *filter_h, const char *name,
                                    float default_time_constant_s);

/**
 * @brief 修改单通道低通时间常数（time_constant_s > 0，单位 s；单写者调用）。
 * @param[in,out] filter_h        已初始化的低通滤波器指针（非 NULL）。
 * @param[in]     channel         目标通道索引（范围：0..FILTER_CHANNEL_CAPACITY-1）。
 * @param[in]     time_constant_s 新的时间常数（单位：s，须为正有限值 > 0）。
 * @return 设置状态码。
 * @retval FILTER_STATUS_OK               通道时间常数已更新。
 * @retval FILTER_STATUS_INVALID_ARGUMENT filter_h 为 NULL。
 * @retval FILTER_STATUS_INVALID_CONFIG   channel 越界或 time_constant_s <= 0 / 非有限。
 * @note 本接口无内部锁保护，仅限初始化阶段或与 Update 同线程/单写者调用。
 */
filter_status_t Filter_Lowpass_SetChannelTimeConstant(filter_lowpass_t *filter_h, uint8_t channel,
                                                      float time_constant_s);

#endif /* LAYERED_CAR_FILTER_LOWPASS_H */

/** @} */

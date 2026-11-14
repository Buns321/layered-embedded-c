//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file filter_lowpass.h
 * @brief 一阶低通滤波器，支持逐通道时间常数。
 * @addtogroup filter
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_FILTER_LOWPASS_H
#define LAYERED_EMBEDDED_C_FILTER_LOWPASS_H

#include "filter_base.h"
#include <stdint.h>

/**
 * @brief 一阶低通滤波器对象。
 * @note 首样本直接初始化；后续按 dt / (time_constant + dt) 计算。
 * @note time_constant_s 单位秒。
 */
typedef struct filter_lowpass_s {
  filter_base_t filter_base;

  float    last_output     [FILTER_CHANNEL_CAPACITY];
  uint32_t last_time_ms    [FILTER_CHANNEL_CAPACITY];
  float    time_constant_s [FILTER_CHANNEL_CAPACITY];
  uint32_t initialized_mask;
} filter_lowpass_t;

/**
 * @brief 初始化低通滤波器。
 * @param filter_h 待初始化的低通滤波器存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖滤波器对象。
 * @param default_time_constant_s 所有通道的初始时间常数，单位 s；必须有限且大于 0。
 * @retval FILTER_STATUS_OK 参数有效，滤波器已完成初始化。
 * @retval FILTER_STATUS_INVALID_ARGUMENT filter_h 或 name 为空。
 * @retval FILTER_STATUS_INVALID_CONFIG 默认时间常数非有限或不大于 0。
 * @pre filter_h 可写；name 在滤波器对象存活期间保持有效。
 * @post 成功时全部通道使用默认时间常数，历史输出和初始化掩码清零；失败时不修改对象。
 * @note 仅允许初始化阶段单线程调用，不得与 update/reset 并发。
 */
filter_status_t Filter_Lowpass_Init(
  filter_lowpass_t *filter_h,
  const char *name,
  float default_time_constant_s
);
/**
 * @brief 设置某个通道的时间常数。
 * @param filter_h 已初始化的低通滤波器，不能为 NULL。
 * @param channel 通道下标，必须小于 FILTER_CHANNEL_CAPACITY。
 * @param time_constant_s 新时间常数，单位 s；必须有限且大于 0。
 * @retval FILTER_STATUS_OK 时间常数已提交。
 * @retval FILTER_STATUS_INVALID_ARGUMENT filter_h 为空。
 * @retval FILTER_STATUS_INVALID_CONFIG 通道越界或时间常数非法。
 * @pre 当前没有对同一通道的并发 update。
 * @post 成功时指定通道后续更新使用新时间常数；失败时配置保持不变。
 * @note 单写者调用；不分配动态内存，修改不会自动 reset 该通道。
 */
filter_status_t Filter_Lowpass_SetChannelTimeConstant(
  filter_lowpass_t *filter_h,
  uint8_t channel,
  float time_constant_s
);

/** @} */

#endif //LAYERED_EMBEDDED_C_FILTER_LOWPASS_H

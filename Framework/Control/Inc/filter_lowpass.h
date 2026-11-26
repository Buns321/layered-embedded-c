/**
 * @file filter_lowpass.h
 * @brief 多通道一阶低通滤波器（支持逐通道时间常数，单位 s）。
 * @addtogroup control
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
  filter_base_t base;
  float last_output[FILTER_CHANNEL_CAPACITY];
  uint32_t last_time_ms[FILTER_CHANNEL_CAPACITY];
  float time_constant_s[FILTER_CHANNEL_CAPACITY];
  uint32_t initialized_mask;
} filter_lowpass_t;

/**
 * @brief 初始化低通滤波器并设置全部通道默认时间常数（default_time_constant_s > 0，单位 s）。
 */
filter_status_t Filter_Lowpass_Init(filter_lowpass_t *filter_h, const char *name,
                                    float default_time_constant_s);

/**
 * @brief 修改单通道低通时间常数（time_constant_s > 0，单位 s；单写者调用）。
 */
filter_status_t Filter_Lowpass_SetChannelTimeConstant(filter_lowpass_t *filter_h, uint8_t channel,
                                                      float time_constant_s);

/** @} */

#endif /* LAYERED_CAR_FILTER_LOWPASS_H */

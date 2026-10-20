//
// Created by Buns_ on 2026/10/20.
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
typedef struct filter_lowpass {
  filter_base_t filter_base;

  float    last_output     [FILTER_CHANNEL_CAPACITY];
  uint32_t last_time_ms    [FILTER_CHANNEL_CAPACITY];
  float    time_constant_s [FILTER_CHANNEL_CAPACITY];
  uint32_t initialized_mask;
} filter_lowpass_t;

/**
 * @brief 初始化低通滤波器。
 * @param this 对象。
 * @param name 名称。
 * @param default_time_constant_s 默认时间常数（s），必须大于 0。
 */
void Filter_Lowpass_Init(filter_lowpass_t *this, const char *name, float default_time_constant_s);
/**
 * @brief 设置某个通道的时间常数。
 * @param this 对象。
 * @param channel 通道下标，必须小于 FILTER_CHANNEL_CAPACITY。
 * @param time_constant_s 时间常数（s），必须大于 0。
 */
void Filter_Lowpass_SetChannelTimeConstant(filter_lowpass_t *this, uint8_t channel, float time_constant_s);

/** @} */

#endif //LAYERED_EMBEDDED_C_FILTER_LOWPASS_H

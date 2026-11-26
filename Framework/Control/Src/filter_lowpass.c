/**
 * @file filter_lowpass.c
 * @brief 多通道一阶低通滤波器实现。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          对每个有效通道独立计算采样间隔 dt_s，按离散一阶低通公式更新输出；
 *          首帧直接透传建立初值，零时间间隔（elapsed_ms == 0）保持上次输出不变。
 * @ingroup Framework_Control
 */

#include "filter_lowpass.h"
#include "sys_math.h"

/**
 * @brief 一阶低通滤波器虚表 init 回调：校验默认时间常数并填充全部通道。
 * @param[in,out] filter_h      低通滤波器基类指针（实际类型为 filter_lowpass_t*）。
 * @param[in]     filter_config 构造器配置（data 指向 float 默认时间常数，单位 s）。
 * @return 初始化状态码。
 */
static filter_status_t filter_lowpass_init(filter_base_t *filter_h,
                                           const filter_config_t *filter_config) {
  filter_lowpass_t *lowpass = (filter_lowpass_t *)filter_h;
  const float *default_tau_s = (const float *)filter_config->data;

  if (default_tau_s == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  if (!SYS_IS_POS_FINITE(*default_tau_s)) {
    return FILTER_STATUS_INVALID_CONFIG;
  }

  for (uint8_t i = 0U; i < FILTER_CHANNEL_CAPACITY; ++i) {
    lowpass->time_constant_s[i] = *default_tau_s;
  }
  return FILTER_STATUS_OK;
}

/**
 * @brief 一阶低通滤波器虚表 reset 回调：清除全部通道初始化掩码与历史输出/时间戳。
 * @param[in,out] filter_h 低通滤波器基类指针。
 */
static void filter_lowpass_reset(filter_base_t *filter_h) {
  filter_lowpass_t *lowpass = (filter_lowpass_t *)filter_h;
  lowpass->initialized_mask = 0U;
  for (uint8_t i = 0U; i < FILTER_CHANNEL_CAPACITY; ++i) {
    lowpass->last_output[i] = 0.0f;
    lowpass->last_time_ms[i] = 0U;
  }
}

/**
 * @brief 一阶低通滤波器虚表 update 回调：按通道独立推进离散低通滤波。
 * @param[in,out] filter_h 低通滤波器基类指针。
 * @param[in,out] sample   已预清洗的样本指针。
 */
static void filter_lowpass_update(filter_base_t *filter_h, filter_sample_t *sample) {
  filter_lowpass_t *lowpass = (filter_lowpass_t *)filter_h;

  for (uint8_t channel = 0U; channel < FILTER_CHANNEL_CAPACITY; ++channel) {
    uint32_t channel_mask = Filter_Sample_GetChannelMask(channel);
    if ((sample->valid_mask & channel_mask) == 0U) {
      continue;
    }

    float input_value = sample->value[channel];
    float tau_s = lowpass->time_constant_s[channel];

    // 首帧初始化：直接以当前输入作为稳态初值，避免从 0 起步产生阶跃衰减暂态
    if ((lowpass->initialized_mask & channel_mask) == 0U) {
      lowpass->last_output[channel] = input_value;
      lowpass->last_time_ms[channel] = sample->time_ms;
      lowpass->initialized_mask |= channel_mask;
      continue;
    }

    // 同一毫秒内重复刷新（elapsed_ms == 0）：保持上次滤波输出，不更新时间戳
    uint32_t elapsed_ms = sample->time_ms - lowpass->last_time_ms[channel];
    if (elapsed_ms == 0U) {
      sample->value[channel] = lowpass->last_output[channel];
      continue;
    }

    // 一阶 RC 低通离散系数 alpha = dt / (tau + dt)，钳位至 [0, 1] 保证无条件稳定
    float dt_s = (float)elapsed_ms * 0.001f;
    float alpha = dt_s / (tau_s + dt_s);
    alpha = SYS_CLAMP(alpha, 0.0f, 1.0f);

    float output_value =
        lowpass->last_output[channel] + (alpha * (input_value - lowpass->last_output[channel]));

    // 若计算结果非有限则清除该通道有效位并跳过状态更新，防止历史状态被污染
    if (!Filter_Sample_WriteFiniteChannel(sample, channel, output_value)) {
      continue;
    }

    lowpass->last_output[channel] = output_value;
    lowpass->last_time_ms[channel] = sample->time_ms;
  }
}

/** @brief 多通道一阶低通滤波器静态只读虚表。 */
static const filter_ops_t s_filter_ops_lowpass = {
    .init = filter_lowpass_init,
    .update = filter_lowpass_update,
    .reset = filter_lowpass_reset,
};

filter_status_t Filter_Lowpass_Init(filter_lowpass_t *filter_h, const char *name,
                                    float default_time_constant_s) {
  if (filter_h == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  const filter_config_t config = {
      .name = name,
      .data = &default_time_constant_s,
  };
  return Filter_Base_Construct(&filter_h->base, &config, &s_filter_ops_lowpass);
}

filter_status_t Filter_Lowpass_SetChannelTimeConstant(filter_lowpass_t *filter_h, uint8_t channel,
                                                      float time_constant_s) {
  if (filter_h == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  if (channel >= FILTER_CHANNEL_CAPACITY || !SYS_IS_POS_FINITE(time_constant_s)) {
    return FILTER_STATUS_INVALID_CONFIG;
  }
  filter_h->time_constant_s[channel] = time_constant_s;
  return FILTER_STATUS_OK;
}

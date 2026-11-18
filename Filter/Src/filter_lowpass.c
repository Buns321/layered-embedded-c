//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file filter_lowpass.c
 * @brief 多通道一阶低通滤波器实现。
 * @ingroup filter
 */

#include "filter_lowpass.h"
#include "system_math.h"

/**
 * @brief 校验并初始化低通具体配置。
 * @param filter_h 待构造的低通对象，不能为 NULL；首次写入前完成全部校验。
 * @param filter_config 非空基配置；data 指向 float 默认时间常数，单位 s。
 * @retval FILTER_STATUS_OK 默认时间常数有限且大于 0。
 * @retval FILTER_STATUS_INVALID_CONFIG 默认时间常数非有限或不大于 0。
 * @pre filter_config->data 指向调用期间有效的 float 时间常数。
 * @post 成功时全部通道使用默认时间常数；失败时不修改滤波器对象。
 * @note 私有构造回调，不分配动态内存。
 */
static filter_status_t filter_lowpass_init(filter_base_t *filter_h,
                                           const filter_config_t *filter_config) {
  filter_lowpass_t *filter_lowpass_h = (filter_lowpass_t *)filter_h;
  const float *default_time_constant_s = (const float *)filter_config->data;

  if (default_time_constant_s == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }

  if (!SYS_IS_POS_FINITE(*default_time_constant_s)) {
    return FILTER_STATUS_INVALID_CONFIG;
  }

  for (uint8_t i = 0; i < FILTER_CHANNEL_CAPACITY; i++) {
    filter_lowpass_h->time_constant_s[i] = *default_time_constant_s;
  }
  return FILTER_STATUS_OK;
}

/**
 * @brief 重置低通滤波器状态。
 * @param filter_h 已初始化的低通滤波器基对象，不能为 NULL。
 * @pre 当前没有并发 update；各通道时间常数保持已配置值。
 * @post initialized_mask、last_output 和 last_time_ms 全部清零，下一有效样本重新初始化滤波。
 * @note 不分配动态内存；不修改 time_constant_s。
 */
static void filter_lowpass_reset(filter_base_t *filter_h) {
  filter_lowpass_t *filter_lowpass_h = (filter_lowpass_t *)filter_h;

  filter_lowpass_h->initialized_mask = 0u;

  for (uint8_t i = 0; i < FILTER_CHANNEL_CAPACITY; i++) {
    filter_lowpass_h->last_output[i] = 0.0f;
    filter_lowpass_h->last_time_ms[i] = 0u;
  }
}

/**
 * @brief 对各通道执行一阶低通更新。
 * @param filter_h 已初始化的低通滤波器基对象，不能为 NULL。
 * @param sample base 已预复制并清洗的样本，可原地读写。
 * @pre sample->time_ms 单调递增；通道时间常数已在配置阶段校验。
 * @post 首样本直接采用输入值；后续按 dt/(tau+dt) 平滑，零时间差保持上次输出。
 * @note 非有限计算结果会清除该通道有效位；不分配动态内存，单写者调用。
 */
static void filter_lowpass_update(filter_base_t *filter_h, filter_sample_t *sample) {
  filter_lowpass_t *filter_lowpass_h = (filter_lowpass_t *)filter_h;

  for (uint8_t channel = 0; channel < FILTER_CHANNEL_CAPACITY; channel++) {
    uint32_t channel_mask = Filter_Sample_GetChannelMask(channel);
    if ((sample->valid_mask & channel_mask) == 0u) {
      continue;
    }

    float input_value = sample->value[channel];
    float time_constant_s = filter_lowpass_h->time_constant_s[channel];

    if ((filter_lowpass_h->initialized_mask & channel_mask) == 0u) {
      filter_lowpass_h->last_output[channel] = input_value;
      filter_lowpass_h->last_time_ms[channel] = sample->time_ms;
      filter_lowpass_h->initialized_mask |= channel_mask;
      continue;
    }

    uint32_t elapsed_ms = sample->time_ms - filter_lowpass_h->last_time_ms[channel];
    if (elapsed_ms == 0u) {
      sample->value[channel] = filter_lowpass_h->last_output[channel];
      continue;
    }

    float dt_s = (float)elapsed_ms * 0.001f;
    float alpha = dt_s / (time_constant_s + dt_s);
    alpha = SYS_CLAMP(alpha, 0.0f, 1.0f);

    float output_value = filter_lowpass_h->last_output[channel] +
                         alpha * (input_value - filter_lowpass_h->last_output[channel]);

    if (!Filter_Sample_WriteFiniteChannel(sample, channel, output_value)) {
      continue;
    }

    filter_lowpass_h->last_output[channel] = output_value;
    filter_lowpass_h->last_time_ms[channel] = sample->time_ms;
  }
}

static const filter_ops_t filter_ops_lowpass = {
    .init = filter_lowpass_init,
    .update = filter_lowpass_update,
    .reset = filter_lowpass_reset,
};

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
filter_status_t Filter_Lowpass_Init(filter_lowpass_t *filter_h, const char *name,
                                    float default_time_constant_s) {
  if (filter_h == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }

  const filter_config_t config = {
      .name = name,
      .data = &default_time_constant_s,
  };
  return Filter_Base_Construct(&filter_h->filter_base, &config, &filter_ops_lowpass);
}

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

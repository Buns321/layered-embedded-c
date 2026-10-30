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
#include <assert.h>

/**
 * @brief 重置低通滤波器状态。
 * @param filter_h 已初始化的低通滤波器基对象，不能为 NULL。
 * @pre 当前没有并发 update；各通道时间常数保持已配置值。
 * @post initialized_mask、last_output 和 last_time_ms 全部清零，下一有效样本重新初始化滤波。
 * @note 不分配动态内存；不修改 time_constant_s。
 */
static void filter_lowpass_reset(filter_base_t *filter_h) {
  assert(filter_h);
  filter_lowpass_t *filter_lowpass_h = (filter_lowpass_t *)filter_h;

  filter_lowpass_h->initialized_mask = 0u;

  for (uint8_t i = 0; i < FILTER_CHANNEL_CAPACITY; i++) {
    filter_lowpass_h->last_output[i]  = 0.0f;
    filter_lowpass_h->last_time_ms[i] = 0u;
  }
}

/**
 * @brief 对各通道执行一阶低通更新。
 * @param filter_h 已初始化的低通滤波器基对象，不能为 NULL。
 * @param input 只读输入样本；仅处理 valid_mask 中有效的通道。
 * @param output 调用方拥有的输出样本，base 已预复制 input；无效通道会清除对应掩码位。
 * @pre input->time_ms 单调递增；通道时间常数有限且大于 0。
 * @post 首样本直接采用输入值；后续按 dt/(tau+dt) 平滑，零时间差保持上次输出。
 * @note 非有限输入、时间常数或结果会清除该通道有效位；不分配动态内存，单写者调用。
 */
static void filter_lowpass_update(filter_base_t *filter_h, const filter_sample_t *input, filter_sample_t *output) {
  assert(filter_h && input && output);
  filter_lowpass_t *filter_lowpass_h = (filter_lowpass_t *)filter_h;

  for (uint8_t channel = 0; channel < FILTER_CHANNEL_CAPACITY; channel++) {
    uint32_t channel_mask = (uint32_t)1u << channel;
    if ((input->valid_mask & channel_mask) == 0u) {
      continue;
    }

    float input_value     = input->value[channel];
    float time_constant_s = filter_lowpass_h->time_constant_s[channel];

    assert(
      SYS_ISFINITE(input_value)     &&
      SYS_ISFINITE(time_constant_s) &&
      time_constant_s > 0.0f
    );

    if (
      !SYS_ISFINITE(input_value)     ||
      !SYS_ISFINITE(time_constant_s) ||
      time_constant_s <= 0.0f
    ) {
      output->valid_mask &= ~channel_mask;
      continue;
    }

    if ((filter_lowpass_h->initialized_mask & channel_mask) == 0u) {
      filter_lowpass_h->last_output[channel]  = input_value;
      filter_lowpass_h->last_time_ms[channel] = input->time_ms;
      filter_lowpass_h->initialized_mask     |= channel_mask;
      output->value[channel]      = input_value;
      continue;
    }

    uint32_t elapsed_ms = input->time_ms - filter_lowpass_h->last_time_ms[channel];
    if (elapsed_ms == 0u) {
      output->value[channel] = filter_lowpass_h->last_output[channel];
      continue;
    }

    float dt_s = (float)elapsed_ms * 0.001f;
    float alpha = dt_s / (time_constant_s + dt_s);
    alpha = SYS_CLAMP(alpha, 0.0f, 1.0f);

    float output_value = filter_lowpass_h->last_output[channel] +
                         alpha * (input_value - filter_lowpass_h->last_output[channel]);

    if (!SYS_ISFINITE(output_value)) {
      output->valid_mask &= ~channel_mask;
      continue;
    }

    filter_lowpass_h->last_output[channel]  = output_value;
    filter_lowpass_h->last_time_ms[channel] = input->time_ms;
    output->value[channel]      = output_value;
  }
}

static const filter_ops_t filter_ops_lowpass = {
  .update = filter_lowpass_update,
  .reset  = filter_lowpass_reset,
};

/**
 * @brief 初始化低通滤波器。
 * @param filter_h 待初始化的低通滤波器存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖滤波器对象。
 * @param default_time_constant_s 所有通道的初始时间常数，单位 s；必须有限且大于 0。
 * @pre filter_h 可写；name 在滤波器对象存活期间保持有效。
 * @post 全部通道使用默认时间常数，历史输出和初始化掩码清零；不分配动态内存。
 * @note 仅允许初始化阶段单线程调用，不得与 update/reset 并发。
 */
void Filter_Lowpass_Init(filter_lowpass_t *filter_h, const char *name, float default_time_constant_s) {
  assert(
    filter_h                                  &&
    name                                  &&
    SYS_ISFINITE(default_time_constant_s) &&
    default_time_constant_s > 0.0f
  );

  Filter_Base_Init(&filter_h->filter_base, name, &filter_ops_lowpass);

  for (uint8_t i = 0; i < FILTER_CHANNEL_CAPACITY; i++) {
    filter_h->time_constant_s[i] = default_time_constant_s;
  }

  Filter_Base_Reset(&filter_h->filter_base);
}

/**
 * @brief 设置某个通道的时间常数。
 * @param filter_h 已初始化的低通滤波器，不能为 NULL。
 * @param channel 通道下标，必须小于 FILTER_CHANNEL_CAPACITY。
 * @param time_constant_s 新时间常数，单位 s；必须有限且大于 0。
 * @pre 当前没有对同一通道的并发 update；Debug 构建用 assert 检查范围和有限性。
 * @post 指定通道后续更新使用新时间常数，已有 last_output 和 last_time_ms 保留。
 * @note 单写者调用；不分配动态内存，修改不会自动 reset 该通道。
 */
void Filter_Lowpass_SetChannelTimeConstant(filter_lowpass_t *filter_h, uint8_t channel, float time_constant_s) {
  assert(
    filter_h                              &&
    channel < FILTER_CHANNEL_CAPACITY &&
    SYS_ISFINITE(time_constant_s)     &&
    time_constant_s > 0.0f
  );
  filter_h->time_constant_s[channel] = time_constant_s;
}

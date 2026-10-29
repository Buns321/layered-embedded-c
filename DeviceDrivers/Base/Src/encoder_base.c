//
// 由 Buns_ 创建于 2026/8/30。
//

/**
 * @file encoder_base.c
 * @brief 编码器抽象接口实现。
 * @ingroup drivers_base
 */

#include "encoder_base.h"
#include  <assert.h>

/** @brief 初始化编码器抽象层并绑定操作表。 */
void Dev_Encoder_Base_Init(encoder_base_t *encoder_h, const char *name, const encoder_ops_t *ops) {
  assert(encoder_h && name && ops);
  encoder_h->ops = ops;
  encoder_h->name = name;
}

/**
 * @brief 把当前位置设为零点。
 * @note 会改变累计位置。
 */
void Dev_Encoder_Base_SetZero(encoder_base_t *encoder_h) {
  assert(encoder_h && encoder_h->ops && encoder_h->ops->set_zero);
  encoder_h->ops->set_zero(encoder_h);
}

/** @brief 读取累计位置（counts）。 */
int64_t Dev_Encoder_Base_GetPosition(encoder_base_t *encoder_h) {
  assert(encoder_h && encoder_h->ops && encoder_h->ops->get_position);
  return encoder_h->ops->get_position(encoder_h);
}

/**
 * @brief 读取自上次调用以来的增量（counts）。
 * @warning 该接口会消费增量；每个控制周期只调用一次。
 */
int32_t Dev_Encoder_Base_GetDelta(encoder_base_t *encoder_h){
  assert(encoder_h && encoder_h->ops && encoder_h->ops->get_delta);
  return encoder_h->ops->get_delta(encoder_h);
}

/**
 * @brief 重新对齐速度基线，不改变累计位置。
 */
void Dev_Encoder_Base_Resync(encoder_base_t *encoder_h){
  assert(encoder_h && encoder_h->ops && encoder_h->ops->resync);
  encoder_h->ops->resync(encoder_h);
}

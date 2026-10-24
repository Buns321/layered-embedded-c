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

void Dev_Encoder_Base_Init(encoder_base_t *encoder_h, const char *name, const encoder_ops_t *ops) {
  assert(encoder_h && name && ops);
  encoder_h->ops = ops;
  encoder_h->name = name;
}

void Dev_Encoder_Base_SetZero(encoder_base_t *encoder_h) {
  assert(encoder_h && encoder_h->ops && encoder_h->ops->set_zero);
  encoder_h->ops->set_zero(encoder_h);
}

int64_t Dev_Encoder_Base_GetPosition(encoder_base_t *encoder_h) {
  assert(encoder_h && encoder_h->ops && encoder_h->ops->get_position);
  return encoder_h->ops->get_position(encoder_h);
}

int32_t Dev_Encoder_Base_GetDelta(encoder_base_t *encoder_h){
  assert(encoder_h && encoder_h->ops && encoder_h->ops->get_delta);
  return encoder_h->ops->get_delta(encoder_h);
}

void Dev_Encoder_Base_Resync(encoder_base_t *encoder_h){
  assert(encoder_h && encoder_h->ops && encoder_h->ops->resync);
  encoder_h->ops->resync(encoder_h);
}

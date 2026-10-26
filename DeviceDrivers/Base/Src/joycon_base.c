//
// 由 Buns_ 创建于 2026/10/26。
//

/**
 * @file joycon_base.c
 * @brief JoyCon 命令设备抽象接口实现。
 * @ingroup drivers_base
 */

#include "joycon_base.h"
#include <assert.h>

void Dev_JoyCon_Base_Init(
  joycon_base_t *joycon_h,
  const char *name,
  const joycon_ops_t *ops
) {
  assert(
    joycon_h &&
    name &&
    ops &&
    ops->get_input &&
    ops->get_status
  );

  joycon_h->name = name;
  joycon_h->ops = ops;
}

bool Dev_JoyCon_Base_GetInput(
  joycon_base_t *joycon_h,
  joycon_input_t *input
) {
  assert(
    joycon_h &&
    joycon_h->ops &&
    joycon_h->ops->get_input &&
    input
  );

  return joycon_h->ops->get_input(joycon_h, input);
}

void Dev_JoyCon_Base_GetStatus(
  joycon_base_t *joycon_h,
  joycon_status_t *status
) {
  assert(
    joycon_h &&
    joycon_h->ops &&
    joycon_h->ops->get_status &&
    status
  );

  joycon_h->ops->get_status(joycon_h, status);
}

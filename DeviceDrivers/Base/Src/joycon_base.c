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

/**
 * @brief 初始化 JoyCon 抽象层并绑定静态操作表。
 * @param joycon_h 抽象对象。
 * @param name 调试名称。
 * @param ops 静态操作表，必需成员不能为空。
 * @pre 对象必须由具体 JoyCon 设备持有。
 */
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

/**
 * @brief 复制最近一帧有效 JoyCon 输入。
 * @param joycon_h 抽象对象。
 * @param input 输出快照。
 * @return 至少收到一帧有效报文时返回 true。
 * @note 并发行为由具体实现定义；上层可在其他任务调用。
 */
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

/**
 * @brief 复制 JoyCon 设备状态快照。
 * @param joycon_h 抽象对象。
 * @param status 输出状态。
 * @note 并发行为由具体实现定义；上层可在其他任务调用。
 */
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

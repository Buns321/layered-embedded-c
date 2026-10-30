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
 * @brief 绑定 JoyCon 名称与只读查询操作表。
 * @param joycon_h 调用方持有的 JoyCon 基类存储；不可为 NULL，所有权不转移。
 * @param name 只读调试名称，生命周期必须覆盖设备对象；不可为 NULL。
 * @param ops 静态操作表；不可为 NULL，get_input 与 get_status 必须可用。
 * @pre 设备尚未并发发布，name 与 ops 在设备使用期间保持有效。
 * @post 只保存绑定，不启动 UART、不解析报文，也不改变最近输入。
 * @note 仅限初始化阶段调用；不加锁且不分配内存，重复调用会覆盖已有绑定。
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
 * @brief 复制最近一帧 JoyCon 控制输入。
 * @param joycon_h 已初始化的 JoyCon 句柄；不可为 NULL，所有权不转移。
 * @param input 调用方持有的输出快照；不可为 NULL，函数覆盖其全部字段。
 * @retval true 至少发布过一帧有效报文，input 含最新快照。
 * @retval false 尚无有效报文；input 仍被覆盖为当前存储值，序列号通常为 0。
 * @pre 已绑定非空 get_input 操作；具体实现负责输入快照的并发保护。
 * @post 四轴范围为 [-1, 1]，time_ms 为解析时刻的单调毫秒时间戳。
 * @note 上层可在其他任务读取；复制是快照语义，不转移任何缓冲区所有权。
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
 * @brief 复制 JoyCon 设备运行状态。
 * @param joycon_h 已初始化的 JoyCon 句柄；不可为 NULL，所有权不转移。
 * @param status 调用方持有的输出状态；不可为 NULL，函数覆盖其全部字段。
 * @pre 已绑定非空 get_status 操作；具体实现负责统计值的并发读取。
 * @post status 包含启动、收发、解析、错误和最近帧时间统计。
 * @note 字节数与帧数单位分别为 bytes 和帧；时间字段为单调毫秒并允许回绕。
 * @warning 本层不加锁；是否可与其他任务并发调用取决于具体实现。
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

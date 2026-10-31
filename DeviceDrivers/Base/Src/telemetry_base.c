//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file telemetry_base.c
 * @brief 遥测设备抽象接口实现。
 * @ingroup drivers_base
 */

#include "telemetry_base.h"
#include "system_math.h"
#include <assert.h>
#include <stddef.h>

/**
 * @brief 绑定遥测名称与具体协议操作表。
 * @param telemetry_h 调用方持有的遥测基类存储；不可为 NULL，所有权不转移。
 * @param name 只读调试名称，生命周期必须覆盖遥测对象；不可为 NULL。
 * @param ops 静态操作表；不可为 NULL，send 与 get_status 必须可用。
 * @pre 遥测设备尚未并发发布，name 与 ops 在使用期间保持有效。
 * @post 只保存绑定，不启动传输，也不分配发送缓冲。
 * @note 仅限初始化阶段调用；不加锁且不分配内存，重复调用会覆盖已有绑定。
 */
void Dev_Telemetry_Base_Init(
  telemetry_base_t *telemetry_h,
  const char *name,
  const telemetry_ops_t *ops
) {
  assert(
    telemetry_h           &&
    name           &&
    ops            &&
    ops->send      &&
    ops->get_status
  );

  telemetry_h->name = name;
  telemetry_h->ops  = ops;
}

/**
 * @brief 校验并提交一帧遥测样本。
 * @param telemetry_h 已初始化的遥测句柄；不可为 NULL，所有权不转移。
 * @param sample 只读样本；有效通道值必须有限，valid_mask 只可置位已配置通道。
 * @retval DEV_TELEMETRY_OK 具体协议已完整接受该帧并开始发送。
 * @retval DEV_TELEMETRY_BUSY 发送通道仍被占用，本帧未被接受。
 * @retval DEV_TELEMETRY_ERROR 参数、掩码或有效通道值非法，或协议编码失败。
 * @pre 已绑定非空 send 操作；time_ms 为单调毫秒时间戳，通道单位和范围由协议约定。
 * @post 样本及其他缓冲区所有权不转移；后端只可读取或复制本次输入。
 * @note send 契约要求非阻塞；本层不加锁，同一设备的并发提交须由调用方串行化。
 */
dev_telemetry_status_t Dev_Telemetry_Base_Send(
  telemetry_base_t *telemetry_h,
  const telemetry_sample_t *sample
) {
  assert(
    telemetry_h              &&
    telemetry_h->ops         &&
    telemetry_h->ops->send   &&
    sample
  );

  if (
    telemetry_h == NULL       ||
    telemetry_h->ops == NULL  ||
    telemetry_h->ops->send == NULL ||
    sample == NULL
  ) {
    return DEV_TELEMETRY_ERROR;
  }

  if ((sample->valid_mask & ~TELEMETRY_CHANNEL_MASK) != 0u) {
    return DEV_TELEMETRY_ERROR;
  }

  for (
    uint32_t channel = 0u;
    channel < TELEMETRY_CHANNEL_CAPACITY;
    channel++
  ) {
    uint32_t mask = (uint32_t)1u << channel;
    if (
      (sample->valid_mask & mask) != 0u &&
      !SYS_ISFINITE(sample->value[channel])
    ) {
      return DEV_TELEMETRY_ERROR;
    }
  }

  return telemetry_h->ops->send(telemetry_h, sample);
}

/**
 * @brief 复制具体遥测后端的运行状态。
 * @param telemetry_h 已初始化的遥测句柄；不可为 NULL，所有权不转移。
 * @param status 调用方持有的输出状态；不可为 NULL，函数覆盖其全部字段。
 * @pre 已绑定非空 get_status 操作。
 * @post status 包含忙碌标志以及已发送、丢弃和错误帧计数。
 * @note 计数单位为帧；后端的状态读取并发能力由其自身实现保证。
 * @warning 本层不复制或保留任何发送缓冲，输出快照不可用于恢复未完成传输。
 */
void Dev_Telemetry_Base_GetStatus(
  telemetry_base_t *telemetry_h,
  telemetry_status_t *status
) {
  assert(
    telemetry_h                    &&
    telemetry_h->ops               &&
    telemetry_h->ops->get_status   &&
    status
  );

  telemetry_h->ops->get_status(telemetry_h, status);
}

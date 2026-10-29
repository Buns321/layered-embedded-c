/**
 * @file platform_can_backend.h
 * @brief STM32 系列公共 CAN 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_CAN_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_CAN_BACKEND_H

#include "platform_def.h"
#include "platform_can.h"
#include <stdbool.h>

/**
 * @brief 校验CAN实例是否已由板级配置启用。
 */
bool platform_can_backend_is_valid(platform_can_instance_t can);

/**
 * @brief 把公共 CAN 过滤规则写入后端。
 */
platform_status_t platform_can_backend_configure_filter(
  platform_can_instance_t can,
  const platform_can_filter_config_t *config
);

/**
 * @brief 启动CAN后端，不负责 CubeMX 结构性初始化。
 */
platform_status_t platform_can_backend_start(
  platform_can_instance_t can
);

/**
 * @brief 停止CAN后端并清理活动状态。
 */
platform_status_t platform_can_backend_stop(
  platform_can_instance_t can
);

/**
 * @brief 启动CAN阻塞发送。
 */
platform_status_t platform_can_backend_transmit(
  platform_can_instance_t can,
  const platform_can_frame_t *frame,
  uint32_t timeout
);

/**
 * @brief 启动CAN阻塞接收。
 */
platform_status_t platform_can_backend_receive(
  platform_can_instance_t can,
  platform_can_fifo_t fifo,
  platform_can_frame_t *frame,
  uint32_t timeout
);

/**
 * @brief 启用 CAN 发送完成或接收通知。
 */
platform_status_t platform_can_backend_activate_notification(
  platform_can_instance_t can,
  uint32_t notifications
);

/**
 * @brief 停用 CAN 发送完成或接收通知。
 */
platform_status_t platform_can_backend_deactivate_notification(
  platform_can_instance_t can,
  uint32_t notifications
);

/**
 * @brief 返回CAN后端记录的错误位。
 */
uint32_t platform_can_backend_get_error(platform_can_instance_t can);

/**
 * @brief 把 CAN 收发和错误事件分发给已注册回调。
 */
void platform_can_dispatch_event(
  platform_can_instance_t can,
  const platform_can_event_t *event
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_CAN_BACKEND_H

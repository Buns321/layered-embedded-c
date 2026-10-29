/**
 * @file platform_can_common.c
 * @brief 与芯片系列无关的 CAN 参数校验、错误管理和回调分发。
 * @ingroup platform
 */

#include "platform_can_backend.h"
#include "platform_error_internal.h"
#include "platform_validation_internal.h"
#include <stddef.h>

static volatile uint32_t can_error[PLATFORM_CAN_INSTANCE_COUNT];
static platform_can_callback_binding_t can_cb[PLATFORM_CAN_INSTANCE_COUNT];
static bool can_cb_registered[PLATFORM_CAN_INSTANCE_COUNT];

/**
 * @brief 把 HAL CAN 结果转换为公共状态并记录错误。
 */
static platform_status_t platform_can_finish(
  platform_can_instance_t can,
  platform_status_t status
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(
      &can_error[can],
      platform_error_from_status(status)
    );
  }
  if (status == PLATFORM_STATUS_TIMEOUT) {
    platform_error_record(&can_error[can], PLATFORM_CAN_ERROR_TIMEOUT);
  }
  return status;
}

/**
 * @brief 构造 CAN 非法参数状态。
 */
static platform_status_t platform_can_invalid(
  platform_can_instance_t can
) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_record(
      &can_error[can],
      PLATFORM_ERROR_INVALID_ARGUMENT | PLATFORM_CAN_ERROR_PARAMETER
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

/**
 * @brief 校验 CAN 帧字段和长度。
 */
static bool platform_can_frame_is_valid(const platform_can_frame_t *frame) {
  if (
    frame == NULL ||
    (frame->id_type != PLATFORM_CAN_ID_STANDARD &&
     frame->id_type != PLATFORM_CAN_ID_EXTENDED)
  ) {
    return false;
  }
  return platform_can_frame_values_are_valid(
    frame->id_type == PLATFORM_CAN_ID_EXTENDED,
    frame->id,
    frame->dlc
  );
}

/**
 * @brief 校验 CAN 过滤器配置。
 */
static bool platform_can_filter_is_valid(
  const platform_can_filter_config_t *config
) {
  if (
    config == NULL ||
    (config->mode != PLATFORM_CAN_FILTER_MASK &&
     config->mode != PLATFORM_CAN_FILTER_LIST) ||
    (config->fifo != PLATFORM_CAN_FIFO_0 &&
     config->fifo != PLATFORM_CAN_FIFO_1) ||
    (config->id_type != PLATFORM_CAN_ID_STANDARD &&
     config->id_type != PLATFORM_CAN_ID_EXTENDED)
  ) {
    return false;
  }

  const bool extended = config->id_type == PLATFORM_CAN_ID_EXTENDED;
  if (!platform_can_identifier_is_valid(extended, config->id)) {
    return false;
  }
  if (config->mode == PLATFORM_CAN_FILTER_LIST) {
    return platform_can_identifier_is_valid(extended, config->id2);
  }
  return platform_can_identifier_is_valid(extended, config->mask);
}

/**
 * @brief 校验 CAN 通知选择是否合法。
 */
static bool platform_can_notifications_are_valid(uint32_t notifications) {
  const uint32_t all_notifications =
    PLATFORM_CAN_NOTIFY_RX_FIFO0_PENDING |
    PLATFORM_CAN_NOTIFY_RX_FIFO1_PENDING |
    PLATFORM_CAN_NOTIFY_RX_FIFO0_FULL |
    PLATFORM_CAN_NOTIFY_RX_FIFO1_FULL |
    PLATFORM_CAN_NOTIFY_TX_MAILBOX_EMPTY |
    PLATFORM_CAN_NOTIFY_ERROR;
  return notifications != 0U &&
         (notifications & ~all_notifications) == 0U;
}

/**
 * @brief 把 CAN 收发和错误事件分发给已注册回调。
 */
void platform_can_dispatch_event(
  platform_can_instance_t can,
  const platform_can_event_t *event
) {
  if (
    (uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT ||
    event == NULL ||
    !can_cb_registered[can]
  ) {
    return;
  }
  if (can_cb[can].callback != NULL) {
    can_cb[can].callback(can, event, can_cb[can].context);
  }
}

/**
 * @brief 配置一个 32 位过滤器组。
 * @param can CAN 实例。
 * @param config 过滤器配置。
 * @pre 必须在 Platform_CAN_Start() 之前调用。
 * @note id 和 mask 是 CAN 标识符位，不是硬件寄存器原始值。
 * @retval PLATFORM_STATUS_ERROR 实例、过滤器组、模式、FIFO 或标识符非法。
 */
platform_status_t Platform_CAN_ConfigureFilter(
  platform_can_instance_t can,
  const platform_can_filter_config_t *config
) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_reset(&can_error[can]);
  }
  if (
    (uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT ||
    !platform_can_backend_is_valid(can) ||
    !platform_can_filter_is_valid(config)
  ) {
    return platform_can_invalid(can);
  }

  platform_status_t status =
    platform_can_backend_configure_filter(can, config);
  if (status == PLATFORM_STATUS_INVALID_ARGUMENT) {
    platform_can_invalid(can);
  } else {
    status = platform_can_finish(can, status);
  }
  return status;
}

/**
 * @brief 启动 CAN 外设。
 * @param can CAN 实例。
 */
platform_status_t Platform_CAN_Start(platform_can_instance_t can) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_reset(&can_error[can]);
  }
  if (!platform_can_backend_is_valid(can)) {
    return platform_can_invalid(can);
  }
  return platform_can_finish(can, platform_can_backend_start(can));
}

/**
 * @brief 停止 CAN 外设。
 * @param can CAN 实例。
 */
platform_status_t Platform_CAN_Stop(platform_can_instance_t can) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_reset(&can_error[can]);
  }
  if (!platform_can_backend_is_valid(can)) {
    return platform_can_invalid(can);
  }
  return platform_can_finish(can, platform_can_backend_stop(can));
}

/**
 * @brief 将一个经典 CAN 帧加入发送队列。
 * @param can CAN 实例。
 * @param frame 待入队的帧。
 * @param timeout 等待空闲发送邮箱的超时时间（ms）或 PLATFORM_TIMEOUT_INFINITE。
 * @retval PLATFORM_STATUS_OK 帧已入队。
 * @retval PLATFORM_STATUS_BUSY HAL 报告忙状态。
 * @retval PLATFORM_STATUS_TIMEOUT 没有邮箱变为空闲。
 * @retval PLATFORM_STATUS_ERROR 帧或外设状态非法。
 * @note 从任务上下文调用。实现轮询邮箱时不会主动休眠。
 */
platform_status_t Platform_CAN_Transmit(
  platform_can_instance_t can,
  const platform_can_frame_t *frame,
  uint32_t timeout
) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_reset(&can_error[can]);
  }
  if (
    (uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT ||
    !platform_can_backend_is_valid(can) ||
    !platform_can_frame_is_valid(frame)
  ) {
    return platform_can_invalid(can);
  }
  return platform_can_finish(
    can,
    platform_can_backend_transmit(can, frame, timeout)
  );
}

/**
 * @brief 从 FIFO 接收一帧。
 * @param can CAN 实例。
 * @param fifo 接收 FIFO。
 * @param frame 目标帧。
 * @param timeout 超时时间（ms）或 PLATFORM_TIMEOUT_INFINITE。
 * @retval PLATFORM_STATUS_OK 帧已复制到 frame。
 * @retval PLATFORM_STATUS_TIMEOUT 超时前没有收到帧。
 * @retval PLATFORM_STATUS_ERROR 参数非法或 HAL 失败。
 * @note 从任务上下文调用。实现轮询帧时不会主动休眠。
 */
platform_status_t Platform_CAN_Receive(
  platform_can_instance_t can,
  platform_can_fifo_t fifo,
  platform_can_frame_t *frame,
  uint32_t timeout
) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_reset(&can_error[can]);
  }
  if (
    (uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT ||
    !platform_can_backend_is_valid(can) ||
    (fifo != PLATFORM_CAN_FIFO_0 && fifo != PLATFORM_CAN_FIFO_1) ||
    frame == NULL
  ) {
    return platform_can_invalid(can);
  }
  return platform_can_finish(
    can,
    platform_can_backend_receive(can, fifo, frame, timeout)
  );
}

/**
 * @brief 启用选定的 CAN 中断通知。
 * @param can CAN 实例。
 * @param notifications platform_can_notification_t 位掩码。
 */
platform_status_t Platform_CAN_ActivateNotification(
  platform_can_instance_t can,
  uint32_t notifications
) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_reset(&can_error[can]);
  }
  if (
    (uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT ||
    !platform_can_backend_is_valid(can) ||
    !platform_can_notifications_are_valid(notifications)
  ) {
    return platform_can_invalid(can);
  }
  return platform_can_finish(
    can,
    platform_can_backend_activate_notification(can, notifications)
  );
}

/**
 * @brief 禁用选定的 CAN 中断通知。
 * @param can CAN 实例。
 * @param notifications platform_can_notification_t 位掩码。
 */
platform_status_t Platform_CAN_DeactivateNotification(
  platform_can_instance_t can,
  uint32_t notifications
) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_reset(&can_error[can]);
  }
  if (
    (uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT ||
    !platform_can_backend_is_valid(can) ||
    !platform_can_notifications_are_valid(notifications)
  ) {
    return platform_can_invalid(can);
  }
  return platform_can_finish(
    can,
    platform_can_backend_deactivate_notification(can, notifications)
  );
}

/**
 * @brief 读取最近一次 CAN 操作错误快照。
 * @param can CAN 实例。
 * @return `platform_can_error_t` 与 `PLATFORM_ERROR_*` 位掩码的组合。
 * @note 参数错误也会写入通用 `PARAMETER` 位；读取不清除快照。
 */
uint32_t Platform_CAN_GetLastError(platform_can_instance_t can) {
  if ((uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT) {
    return PLATFORM_ERROR_INVALID_ARGUMENT | PLATFORM_CAN_ERROR_PARAMETER;
  }
  return can_error[can] | platform_can_backend_get_error(can);
}

/**
 * @brief 注册 CAN 事件回调。
 * @param can CAN 实例。
 * @param callbacks 回调与上下文，内容会被复制。
 * @note 应在初始化阶段、启用通知前调用一次。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调非法。
 */
platform_status_t Platform_CAN_RegisterCallback(
  platform_can_instance_t can,
  const platform_can_callback_binding_t *callbacks
) {
  if (
    (uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_can_backend_is_valid(can)
  ) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (can_cb_registered[can]) {
    return PLATFORM_STATUS_BUSY;
  }

  can_cb[can] = *callbacks;
  can_cb_registered[can] = true;
  return PLATFORM_STATUS_OK;
}

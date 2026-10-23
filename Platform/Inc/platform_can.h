//
// 由 Buns_ 创建于 2026/10/23。
//

/**
 * @file platform_can.h
 * @brief 不依赖 HAL 类型的经典 CAN 接口。
 * @addtogroup platform
 * @{
 * @note 本接口不包含 CAN FD。
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_CAN_H
#define LAYERED_EMBEDDED_C_PLATFORM_CAN_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

#if !PLATFORM_ENABLE_CAN1 && !PLATFORM_ENABLE_CAN2
#error "At least one Platform CAN instance must be enabled"
#endif

/** @brief 可用的经典 CAN 实例。 */
typedef enum platform_can_instance_e {
#if PLATFORM_ENABLE_CAN1
  PLATFORM_CAN_1 = 0U,
#endif
#if PLATFORM_ENABLE_CAN2
  PLATFORM_CAN_2 = 1U,
#endif
} platform_can_instance_t;

/** @brief CAN 身份槽位数量上限。 */
#define PLATFORM_CAN_INSTANCE_COUNT 2U

/** @brief CAN 标识符格式。 */
typedef enum platform_can_id_type_e {
  PLATFORM_CAN_ID_STANDARD = 0U,
  PLATFORM_CAN_ID_EXTENDED,
} platform_can_id_type_t;

/** @brief CAN 接收 FIFO 选择。 */
typedef enum platform_can_fifo_e {
  PLATFORM_CAN_FIFO_0 = 0U,
  PLATFORM_CAN_FIFO_1,
} platform_can_fifo_t;

/** @brief 经典 CAN 帧。 */
typedef struct platform_can_frame_s {
  uint32_t id;
  uint8_t dlc;
  platform_can_id_type_t id_type;
  bool remote;
  uint8_t data[8];
} platform_can_frame_t;

/** @brief 过滤器匹配模式。 */
typedef enum platform_can_filter_mode_e {
  PLATFORM_CAN_FILTER_MASK = 0U,
  PLATFORM_CAN_FILTER_LIST,
} platform_can_filter_mode_t;

/**
 * @brief 32 位过滤器配置。
 * @note id 和 mask 只包含 CAN 标识符位。标准帧过滤器使用 bit 10:0，扩展帧
 *       过滤器使用 bit 28:0。实现层负责把 IDE 和 RTR 字段加入硬件布局。
 * @note 掩码模式下，mask 是接受掩码；列表模式下，两个标识符分别是 id 和
 *       id2，二者共用 id_type 和 remote。
 */
typedef struct platform_can_filter_config_s {
  uint32_t bank;
  uint32_t id;
  uint32_t mask;
  uint32_t id2;
  platform_can_filter_mode_t mode;
  platform_can_fifo_t fifo;
  platform_can_id_type_t id_type;
  bool remote;
  bool enabled;
} platform_can_filter_config_t;

/** @brief CAN 中断通知位。 */
typedef enum platform_can_notification_e {
  PLATFORM_CAN_NOTIFY_RX_FIFO0_PENDING = (1u << 0),
  PLATFORM_CAN_NOTIFY_RX_FIFO1_PENDING = (1u << 1),
  PLATFORM_CAN_NOTIFY_RX_FIFO0_FULL    = (1u << 2),
  PLATFORM_CAN_NOTIFY_RX_FIFO1_FULL    = (1u << 3),
  PLATFORM_CAN_NOTIFY_TX_MAILBOX_EMPTY = (1u << 4),
  PLATFORM_CAN_NOTIFY_ERROR            = (1u << 5),
} platform_can_notification_t;

/** @brief CAN 错误标志，可同时置位多个位。 */
typedef enum platform_can_error_e {
  PLATFORM_CAN_ERROR_NONE                = 0u,
  PLATFORM_CAN_ERROR_WARNING             = (1u << 0),
  PLATFORM_CAN_ERROR_PASSIVE             = (1u << 1),
  PLATFORM_CAN_ERROR_BUS_OFF             = (1u << 2),
  PLATFORM_CAN_ERROR_STUFF               = (1u << 3),
  PLATFORM_CAN_ERROR_FORM                = (1u << 4),
  PLATFORM_CAN_ERROR_ACK                 = (1u << 5),
  PLATFORM_CAN_ERROR_BIT_RECESSIVE       = (1u << 6),
  PLATFORM_CAN_ERROR_BIT_DOMINANT        = (1u << 7),
  PLATFORM_CAN_ERROR_CRC                 = (1u << 8),
  PLATFORM_CAN_ERROR_RX_FIFO0_OVERRUN    = (1u << 9),
  PLATFORM_CAN_ERROR_RX_FIFO1_OVERRUN    = (1u << 10),
  PLATFORM_CAN_ERROR_TX_ARBITRATION_LOST = (1u << 11),
  PLATFORM_CAN_ERROR_TX_FAILURE          = (1u << 12),
  PLATFORM_CAN_ERROR_TIMEOUT             = (1u << 13),
  PLATFORM_CAN_ERROR_STATE               = (1u << 14),
  PLATFORM_CAN_ERROR_PARAMETER           = (1u << 15),
  PLATFORM_CAN_ERROR_INTERNAL            = (1u << 16),
  PLATFORM_CAN_ERROR_UNKNOWN             = (1u << 31),
} platform_can_error_t;

/** @brief 中断事件类型。 */
typedef enum platform_can_event_type_e {
  PLATFORM_CAN_EVENT_TX_COMPLETE = 0U,
  PLATFORM_CAN_EVENT_TX_ABORT_COMPLETE,
  PLATFORM_CAN_EVENT_RX_PENDING,
  PLATFORM_CAN_EVENT_RX_FULL,
  PLATFORM_CAN_EVENT_ERROR,
} platform_can_event_type_t;

/**
 * @brief 传递给已注册回调的中断事件。
 * @note 事件及其帧数据仅在回调执行期间有效。返回前应复制所需数据。
 */
typedef struct platform_can_event_s {
  platform_can_event_type_t type;
  uint8_t mailbox;
  platform_can_fifo_t fifo;
  platform_can_frame_t frame;
  uint32_t error;
} platform_can_event_t;

typedef void (*platform_can_callback_t)(
  platform_can_instance_t can,
  const platform_can_event_t *event,
  void *context
);

/** @brief CAN 回调所有者。 */
typedef struct platform_can_callback_binding_s {
  platform_can_callback_t callback;
  void *context;
} platform_can_callback_binding_t;

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
);

/**
 * @brief 启动 CAN 外设。
 * @param can CAN 实例。
 */
platform_status_t Platform_CAN_Start(platform_can_instance_t can);

/**
 * @brief 停止 CAN 外设。
 * @param can CAN 实例。
 */
platform_status_t Platform_CAN_Stop(platform_can_instance_t can);

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
);

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
);

/**
 * @brief 启用选定的 CAN 中断通知。
 * @param can CAN 实例。
 * @param notifications platform_can_notification_t 位掩码。
 */
platform_status_t Platform_CAN_ActivateNotification(
  platform_can_instance_t can,
  uint32_t notifications
);

/**
 * @brief 禁用选定的 CAN 中断通知。
 * @param can CAN 实例。
 * @param notifications platform_can_notification_t 位掩码。
 */
platform_status_t Platform_CAN_DeactivateNotification(
  platform_can_instance_t can,
  uint32_t notifications
);

/**
 * @brief 读取最近一次 CAN 操作错误快照。
 * @param can CAN 实例。
 * @return `platform_can_error_t` 与 `PLATFORM_COMMON_ERROR_*` 位掩码的组合。
 * @note 参数错误也会写入通用 `PARAMETER` 位；读取不清除快照。
 */
uint32_t Platform_CAN_GetLastError(platform_can_instance_t can);

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
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_CAN_H

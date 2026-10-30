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
 * @param can CAN 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param config 过滤器配置；不得为 NULL。掩码模式读取 id 和 mask，列表模式读取 id 和 id2；
 *        bank 范围由具体 STM32 系列和 CAN 实例决定，remote 决定是否匹配 RTR 帧。
 * @retval PLATFORM_STATUS_OK 过滤器组已写入。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、过滤器组、模式、FIFO 或标识符非法；硬件不变。
 * @retval PLATFORM_STATUS_ERROR HAL 配置过滤器失败。
 * @pre 必须在 Platform_CAN_Start() 之前调用。
 * @post 成功时配置立即写入硬件并保留到后续重新配置或复位；config 内容不会被保存。
 * @warning 标准标识符范围为 0..0x7FF，扩展标识符范围为 0..0x1FFFFFFF；
 *          id、id2 和 mask 只包含标识符位，不能传硬件寄存器原始布局。
 * @note 不分配内存；调用开始时清除实例错误快照，参数非法时同时记录 PARAMETER 位。
 */
platform_status_t Platform_CAN_ConfigureFilter(
  platform_can_instance_t can,
  const platform_can_filter_config_t *config
);

/**
 * @brief 启动 CAN 外设。
 * @param can CAN 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK CAN 已进入运行状态。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告外设仍处于忙状态。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件启动失败。
 * @pre MX_CANx_Init() 已完成，过滤器已按需要在启动前配置。
 * @post 成功时可收发；bus-off 不会由 Platform 层自动恢复，调用方须显式 Stop、重配并 Start。
 * @note 不执行 CubeMX 结构性初始化；调用开始时清除软件错误快照，不分配内存。
 */
platform_status_t Platform_CAN_Start(platform_can_instance_t can);

/**
 * @brief 停止 CAN 外设。
 * @param can CAN 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK CAN 已停止。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告外设仍处于忙状态。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件停止失败。
 * @pre 调用方已启动 CAN，或确认该实例可接受停止请求。
 * @post 成功时不再接受新的邮箱发送或 FIFO 接收；已进入中断的事件可能仍在分发。
 * @note 不等待在途回调结束，不释放缓冲区，不修改 CubeMX 配置；不分配内存。
 */
platform_status_t Platform_CAN_Stop(platform_can_instance_t can);

/**
 * @brief 将一个经典 CAN 帧加入发送队列。
 * @param can CAN 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param frame 待入队的帧；不得为 NULL。id 标准帧为 0..0x7FF、扩展帧为 0..0x1FFFFFFF，
 *        dlc 为 0..8，data 至少包含 dlc 个字节。
 * @param timeout 等待空闲发送邮箱的超时时间，单位 ms；0 表示立即检查，
 *        PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 帧已入队。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、帧字段或长度非法；不访问发送邮箱。
 * @retval PLATFORM_STATUS_BUSY HAL 报告忙状态。
 * @retval PLATFORM_STATUS_TIMEOUT 没有邮箱变为空闲。
 * @retval PLATFORM_STATUS_ERROR HAL 或外设拒绝发送请求。
 * @pre CAN 已启动，frame 指向的对象在调用期间有效。
 * @post 返回 OK 只表示帧已复制到硬件发送邮箱，不表示总线发送成功或已被其他节点确认。
 * @note 只能从任务上下文调用；实现轮询邮箱时不会主动休眠。帧入队后本层不再引用其存储。
 */
platform_status_t Platform_CAN_Transmit(
  platform_can_instance_t can,
  const platform_can_frame_t *frame,
  uint32_t timeout
);

/**
 * @brief 从 FIFO 接收一帧。
 * @param can CAN 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param fifo 接收 FIFO；取 PLATFORM_CAN_FIFO_0 或 PLATFORM_CAN_FIFO_1。
 * @param frame 输出帧；不得为 NULL，成功时覆盖 id、dlc、id_type、remote 和 data。
 * @param timeout 等待 FIFO 非空的超时时间，单位 ms；0 表示立即检查，
 *        PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 帧已复制到 frame。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、FIFO 或 frame 非法；不访问 FIFO。
 * @retval PLATFORM_STATUS_TIMEOUT 超时前没有收到帧。
 * @retval PLATFORM_STATUS_ERROR HAL 读取 FIFO 失败。
 * @pre CAN 已启动，frame 指向可写对象。
 * @post 成功时消费一个 FIFO 报文；超时 frame 保持不变，HAL 读取失败时不保证内容完整。
 * @note 只能从任务上下文调用；实现轮询 FIFO 时不会主动休眠，不分配内存。
 */
platform_status_t Platform_CAN_Receive(
  platform_can_instance_t can,
  platform_can_fifo_t fifo,
  platform_can_frame_t *frame,
  uint32_t timeout
);

/**
 * @brief 启用选定的 CAN 中断通知。
 * @param can CAN 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param notifications 非零的 `platform_can_notification_t` 位掩码；不得包含未定义位。
 * @retval PLATFORM_STATUS_OK 指定通知已启用。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或通知位掩码非法；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告外设忙。
 * @retval PLATFORM_STATUS_ERROR HAL 拒绝通知配置。
 * @pre CAN 和 NVIC 已由 CubeMX 初始化；如需事件，回调已在启用通知前注册。
 * @post 成功后相应事件可由 HAL 中断回调分发；事件及帧数据只在回调期间有效。
 * @note 不分配内存、不等待在途回调；调用开始时清除软件错误快照。
 */
platform_status_t Platform_CAN_ActivateNotification(
  platform_can_instance_t can,
  uint32_t notifications
);

/**
 * @brief 禁用选定的 CAN 中断通知。
 * @param can CAN 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param notifications 非零的 `platform_can_notification_t` 位掩码；不得包含未定义位。
 * @retval PLATFORM_STATUS_OK 指定通知已禁用。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或通知位掩码非法；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告外设忙。
 * @retval PLATFORM_STATUS_ERROR HAL 拒绝通知配置。
 * @pre 调用方已启用对应通知，或确认该实例可接受停用请求。
 * @post 成功后不再产生对应新中断，但已挂起的中断仍可能执行一次回调。
 * @note 不等待在途回调结束；不分配内存，调用开始时清除软件错误快照。
 */
platform_status_t Platform_CAN_DeactivateNotification(
  platform_can_instance_t can,
  uint32_t notifications
);

/**
 * @brief 读取最近一次 CAN 操作错误快照。
 * @param can CAN 实例；当前仅有效枚举值可读取后端错误。
 * @retval PLATFORM_ERROR_INVALID_ARGUMENT|PLATFORM_CAN_ERROR_PARAMETER 实例越界。
 * @post 合法实例返回软件错误位与 HAL 错误映射的按位或；非法实例不访问快照。
 * @note 参数错误也会写入 CAN PARAMETER 位；读取不清除快照，不触发 bus-off 恢复。
 */
uint32_t Platform_CAN_GetLastError(platform_can_instance_t can);

/**
 * @brief 注册 CAN 事件回调。
 * @param can CAN 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param callbacks 待复制的回调与上下文；不得为 NULL，callback 成员可为 NULL 以禁用通知。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调非法。
 * @pre callbacks 指向的对象在调用期间有效；每个实例仅允许成功注册一次。
 * @post 成功时复制 callback 和 context，后续发送、接收或错误事件调用该回调。
 * @warning 回调在中断上下文执行；event 及其中 frame/data 只在回调期间有效，禁止保存指针或阻塞。
 * @note 应在初始化阶段、启用通知前注册；context 的生命周期由调用方维护。
 */
platform_status_t Platform_CAN_RegisterCallback(
  platform_can_instance_t can,
  const platform_can_callback_binding_t *callbacks
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_CAN_H

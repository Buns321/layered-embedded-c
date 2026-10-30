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
 * @param can 已通过后端校验的 CAN 实例；用于选择软件错误槽位。
 * @param status 后端返回的公共操作状态。
 * @return 原样返回 status，便于调用方直接转交。
 * @pre can 必须小于 PLATFORM_CAN_INSTANCE_COUNT，否则会越界访问错误数组。
 * @post status 非 OK 时记录对应 PLATFORM_ERROR_* 位；TIMEOUT 额外记录 CAN_TIMEOUT。
 * @note 内部辅助函数；不分配内存、不阻塞，快照并发限制由 platform_error_record() 决定。
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
 * @param can 待报告的 CAN 实例；允许越界值。
 * @return 始终返回 PLATFORM_STATUS_INVALID_ARGUMENT。
 * @post can 位于槽位范围内时记录 INVALID_ARGUMENT 与 CAN_PARAMETER；越界值不访问数组。
 * @note 内部辅助函数；不访问 CAN 硬件、不分配内存、不阻塞。
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
 * @param frame 待校验的经典 CAN 帧；允许为 NULL。
 * @retval true frame 非 NULL，id_type 已定义，标识符适配标准/扩展格式，且 dlc 为 0..8。
 * @retval false frame 为 NULL 或任一字段越界。
 * @post 纯函数；不修改输入对象或模块状态。
 * @note remote 标志不改变长度校验；data 的存储长度由失调用方保证。内部辅助函数，不分配内存。
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
 * @param config 待校验的过滤器配置；允许为 NULL。
 * @retval true mode、fifo、id_type 已定义，且 id 以及当前模式使用的 mask 或 id2 在范围内。
 * @retval false config 为 NULL 或任一字段非法。
 * @post 纯函数；不修改配置对象、不访问 CAN 硬件。
 * @note 掩码模式只校验 id 和 mask，列表模式只校验 id 和 id2；bank 范围由后端继续校验。
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
 * @param notifications `platform_can_notification_t` 位掩码。
 * @retval true notifications 非零，且所有置位均属于已定义通知。
 * @retval false notifications 为零或包含未定义位。
 * @post 纯函数；不修改参数或模块状态。
 * @note 内部辅助函数；不访问硬件、不分配内存。
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
 * @param can CAN 实例；非法或未注册时无操作。
 * @param event 事件对象；可为 NULL，非 NULL 时只在本次回调期间有效。
 * @post 已注册且 callback 非 NULL 时同步调用一次；不保存 event 或 frame 指针。
 * @warning 由 HAL 中断回调调用，执行上下文为中断；不得阻塞或分配内存。
 * @note 接收事件中的 frame/data 仅在回调期间有效；context 由注册阶段保存。
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
 * @param can CAN 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK CAN 已进入运行状态。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告外设仍处于忙状态。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件启动失败。
 * @pre MX_CANx_Init() 已完成，过滤器已按需要在启动前配置。
 * @post 成功时可收发；bus-off 不会由 Platform 层自动恢复，调用方须显式 Stop、重配并 Start。
 * @note 不执行 CubeMX 结构性初始化；调用开始时清除软件错误快照，不分配内存。
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
 * @param can CAN 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK CAN 已停止。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告外设仍处于忙状态。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件停止失败。
 * @pre 调用方已启动 CAN，或确认该实例可接受停止请求。
 * @post 成功时不再接受新的邮箱发送或 FIFO 接收；已进入中断的事件可能仍在分发。
 * @note 不等待在途回调结束，不释放缓冲区，不修改 CubeMX 配置；不分配内存。
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
 * @param can CAN 实例；当前仅有效枚举值可读取后端错误。
 * @retval PLATFORM_ERROR_INVALID_ARGUMENT|PLATFORM_CAN_ERROR_PARAMETER 实例越界。
 * @post 合法实例返回软件错误位与 HAL 错误映射的按位或；非法实例不访问快照。
 * @note 参数错误也会写入 CAN PARAMETER 位；读取不清除快照，不触发 bus-off 恢复。
 */
uint32_t Platform_CAN_GetLastError(platform_can_instance_t can) {
  if ((uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT) {
    return PLATFORM_ERROR_INVALID_ARGUMENT | PLATFORM_CAN_ERROR_PARAMETER;
  }
  return can_error[can] | platform_can_backend_get_error(can);
}

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

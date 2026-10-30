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
 * @brief 查询 CAN 实例是否已映射到板级启用的 HAL 句柄。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @retval true 实例已启用且映射到 HAL 句柄。
 * @retval false 枚举值越界、实例未启用或缺少 HAL 句柄。
 * @post 只执行映射查询，不访问 CAN 外设、不修改错误快照。
 * @note 可在任务或中断上下文调用；本查询不阻塞且不分配动态内存。
 */
bool platform_can_backend_is_valid(platform_can_instance_t can);

/**
 * @brief 把一个 32 位 CAN 过滤器组配置写入硬件。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @param config 过滤器配置；调用方拥有对象，本层只读取而不保存指针。
 * @retval PLATFORM_STATUS_OK 过滤器组已按配置写入。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或过滤器组编号不适用于该实例。
 * @retval PLATFORM_STATUS_ERROR HAL 配置过滤器失败。
 * @pre 必须在 Platform_CAN_Start() 之前调用；config 非 NULL，mode、fifo、id_type、id、id2/mask
 *      已由公共层校验。
 * @post 成功返回后 bank 的 32 位掩码或列表规则生效；config 不保留，调用后可释放其存储。
 * @note 标识符字段是 CAN ID 位，不是 HAL 寄存器原始值；CAN1 使用 bank 0..13，启用 CAN2 时使用 14..27。
 */
platform_status_t platform_can_backend_configure_filter(
  platform_can_instance_t can,
  const platform_can_filter_config_t *config
);

/**
 * @brief 启动 CAN 外设并允许收发。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @retval PLATFORM_STATUS_OK CAN 已进入运行状态。
 * @retval PLATFORM_STATUS_BUSY HAL 报告外设仍处于忙状态。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件启动失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界、未启用或没有 HAL 句柄。
 * @pre MX_CANx_Init() 已完成，过滤器已按需要在启动前配置。
 * @post 成功后 CAN 可收发；bus-off 不会由本层自动恢复，调用方须显式 Stop、重配并 Start。
 * @note 本函数不执行 CubeMX 结构性初始化，也不清理由 HAL 记录的错误快照。
 */
platform_status_t platform_can_backend_start(
  platform_can_instance_t can
);

/**
 * @brief 停止 CAN 外设并终止新的硬件收发活动。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @retval PLATFORM_STATUS_OK CAN 已停止。
 * @retval PLATFORM_STATUS_BUSY HAL 报告外设仍处于忙状态。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件停止失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界、未启用或没有 HAL 句柄。
 * @post 成功后不再接受新的邮箱发送或 FIFO 接收；已有中断事件可能仍在分发途中。
 * @note 不等待在途回调结束，不释放缓冲区，也不修改 CubeMX 配置；不分配动态内存。
 */
platform_status_t platform_can_backend_stop(
  platform_can_instance_t can
);

/**
 * @brief 等待空闲邮箱后将一帧经典 CAN 加入发送队列。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @param frame 待发送帧；调用方拥有，非 NULL；id、id_type、dlc 和 remote 必须由公共层校验，
 *              data 至少包含 dlc 字节。
 * @param timeout 等待空闲邮箱的超时时间，单位毫秒；0 表示立即检查，
 *                PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 帧已复制到空闲发送邮箱。
 * @retval PLATFORM_STATUS_BUSY HAL 报告外设忙。
 * @retval PLATFORM_STATUS_TIMEOUT 没有邮箱在 timeout 内变为空闲。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件拒绝发送请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或没有 HAL 句柄。
 * @pre CAN 已启动，frame 非 NULL 且字段已由公共层校验，本函数在任务上下文调用。
 * @post 返回 OK 只表示帧已入队，不表示总线发送完成；发送完成后由中断事件报告。
 * @note 实现轮询邮箱且不主动休眠；帧数据在入队返回后不再被本层引用。
 */
platform_status_t platform_can_backend_transmit(
  platform_can_instance_t can,
  const platform_can_frame_t *frame,
  uint32_t timeout
);

/**
 * @brief 阻塞等待指定 FIFO 并读取一帧经典 CAN。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @param fifo 接收 FIFO，取 PLATFORM_CAN_FIFO_0 或 PLATFORM_CAN_FIFO_1。
 * @param frame 目标帧；调用方拥有，非 NULL；成功时覆盖其中的 id、dlc、id_type、remote 和 data。
 * @param timeout 等待 FIFO 非空的超时时间，单位毫秒；0 表示立即检查，
 *                PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 一帧已复制到 frame。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内没有可接收帧，frame 保持不变。
 * @retval PLATFORM_STATUS_ERROR HAL 读取 FIFO 失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或没有 HAL 句柄。
 * @pre CAN 已启动，frame 非 NULL，fifo 为 PLATFORM_CAN_FIFO_0/1，且本函数在任务上下文调用。
 * @post 成功时消费一个 FIFO 报文；HAL 读取失败时 frame 内容不保证完整。
 * @note 实现轮询 FIFO 且不主动休眠；不得在中断上下文调用。
 */
platform_status_t platform_can_backend_receive(
  platform_can_instance_t can,
  platform_can_fifo_t fifo,
  platform_can_frame_t *frame,
  uint32_t timeout
);

/**
 * @brief 启用选定的 CAN 收发、FIFO 或错误中断通知。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @param notifications platform_can_notification_t 的非零位掩码；公共层保证只含已定义通知位。
 * @retval PLATFORM_STATUS_OK 指定通知已启用。
 * @retval PLATFORM_STATUS_BUSY HAL 报告外设忙。
 * @retval PLATFORM_STATUS_ERROR HAL 拒绝通知配置。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或没有 HAL 句柄。
 * @pre CAN 和 NVIC 已由 CubeMX 初始化；notifications 非零且仅含已定义通知位；如需事件，
 *      公共层回调已在初始化阶段注册。
 * @post 成功后相应 HAL 中断回调可被触发；回调运行在中断上下文，事件及帧仅在回调期间有效。
 * @note notifications 的无定义位被忽略，不会扩展为任意 HAL 中断；本函数不分配动态内存。
 */
platform_status_t platform_can_backend_activate_notification(
  platform_can_instance_t can,
  uint32_t notifications
);

/**
 * @brief 停用选定的 CAN 收发、FIFO 或错误中断通知。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @param notifications platform_can_notification_t 的非零位掩码；公共层保证只含已定义通知位。
 * @retval PLATFORM_STATUS_OK 指定通知已停用。
 * @retval PLATFORM_STATUS_BUSY HAL 报告外设忙。
 * @retval PLATFORM_STATUS_ERROR HAL 拒绝通知配置。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或没有 HAL 句柄。
 * @pre notifications 非零且仅含已定义通知位；公共层已完成实例和掩码校验。
 * @post 成功后不再产生对应的新中断，但已挂起的中断仍可能执行一次回调。
 * @note 不等待在途回调结束；本函数不分配动态内存。
 */
platform_status_t platform_can_backend_deactivate_notification(
  platform_can_instance_t can,
  uint32_t notifications
);

/**
 * @brief 读取并映射 CAN 后端当前错误位。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @return platform_can_error_t 位掩码；可同时包含多个错误位，NONE 表示 HAL 未报告错误，
 *         实例非法时返回 PARAMETER。
 * @post 只读取 HAL 错误快照，不清除错误、不改变总线状态，也不停止外设。
 * @note 可在任务或中断上下文调用；读取不会触发 bus-off 恢复。
 */
uint32_t platform_can_backend_get_error(platform_can_instance_t can);

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
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_CAN_BACKEND_H

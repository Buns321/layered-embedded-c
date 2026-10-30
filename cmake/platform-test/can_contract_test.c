/**
 * @file can_contract_test.c
 * @brief CAN Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_can_backend.h"
#include "platform_can.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static platform_status_t s_transmit_status;
static platform_status_t s_filter_status;
static uint32_t s_backend_error;
static uint32_t s_event_count;
static platform_can_event_t s_last_event;

/**
 * @brief 查询测试替身接受的 CAN 实例。
 * @param can CAN 实例枚举值；测试替身只接受 PLATFORM_CAN_1。
 * @retval true can 等于 PLATFORM_CAN_1。
 * @retval false can 为其他枚举值。
 * @post 只比较枚举值，不解析 HAL 句柄、不访问 CAN 外设，也不修改全局状态。
 * @note 这是测试替身；生产后端是否启用实例以及句柄是否有效的规则不由本函数模拟。
 */
bool platform_can_backend_is_valid(platform_can_instance_t can) {
  return can == PLATFORM_CAN_1;
}

/**
 * @brief 返回测试注入的 CAN 过滤器配置状态。
 * @param can CAN 实例枚举值；本替身忽略该参数，不校验实例或过滤器组编号。
 * @param config 过滤器配置；本替身忽略该参数，不读取字段，也不保存指针。
 * @retval PLATFORM_STATUS_OK s_filter_status 注入为 OK；未配置硬件。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT s_filter_status 注入为 INVALID_ARGUMENT；
 *         本替身不额外校验参数。
 * @post s_filter_status 不变，config 指向的对象不变；不写 CAN 过滤器寄存器。
 * @note 测试只注入 OK 与 INVALID_ARGUMENT；生产后端还可能返回 ERROR，并执行真实过滤规则配置。
 */
platform_status_t platform_can_backend_configure_filter(
  platform_can_instance_t can,
  const platform_can_filter_config_t *config
) {
  (void)can;
  (void)config;
  return s_filter_status;
}

/**
 * @brief 模拟启动 CAN 外设并固定报告成功。
 * @param can CAN 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动 CAN，也未改变总线状态。
 * @post 不访问 CAN 寄存器、不使能收发，也不修改错误快照。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_can_backend_start(
  platform_can_instance_t can
) {
  (void)can;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟停止 CAN 外设并固定报告成功。
 * @param can CAN 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @retval PLATFORM_STATUS_OK 固定返回，未停止任何真实收发活动。
 * @post 不访问 CAN 寄存器，不取消在途中断，也不修改错误快照。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_can_backend_stop(
  platform_can_instance_t can
) {
  (void)can;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 返回测试注入的 CAN 发送状态。
 * @param can CAN 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @param frame 待发送帧；本替身忽略该参数，不读取字段，也不复制数据。
 * @param timeout 等待空闲邮箱的超时时间，单位毫秒；本替身忽略该参数且不等待。
 * @return s_transmit_status 的当前值；测试只注入 PLATFORM_STATUS_OK 或
 *         PLATFORM_STATUS_TIMEOUT，本替身不将帧放入邮箱。
 * @post s_transmit_status 和 frame 指向的对象均不变，不访问 CAN 总线或错误快照。
 * @note 这是测试替身；生产后端还可能返回 BUSY、ERROR 或 INVALID_ARGUMENT。
 */
platform_status_t platform_can_backend_transmit(
  platform_can_instance_t can,
  const platform_can_frame_t *frame,
  uint32_t timeout
) {
  (void)can;
  (void)frame;
  (void)timeout;
  return s_transmit_status;
}

/**
 * @brief 模拟接收 CAN 帧并固定报告成功，但不写入目标帧。
 * @param can CAN 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @param fifo 接收 FIFO；本替身忽略该参数，不读取 FIFO 状态。
 * @param frame 目标帧；本替身不读写该对象，调用后内容保持不变。
 * @param timeout 等待 FIFO 非空的超时时间，单位毫秒；本替身忽略该参数且不等待。
 * @retval PLATFORM_STATUS_OK 固定返回，未消费报文，也未初始化 frame。
 * @post frame 内容不变，不访问 CAN FIFO、总线或中断状态。
 * @note 测试替身不会返回生产后端声明的 TIMEOUT、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_can_backend_receive(
  platform_can_instance_t can,
  platform_can_fifo_t fifo,
  platform_can_frame_t *frame,
  uint32_t timeout
) {
  (void)can;
  (void)fifo;
  (void)frame;
  (void)timeout;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启用 CAN 通知并固定报告成功。
 * @param can CAN 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @param notifications 通知位掩码；本替身忽略该参数，不修改 NVIC 或 HAL 中断使能。
 * @retval PLATFORM_STATUS_OK 固定返回，但不会使能任何真实通知。
 * @post 不改变中断配置，后续也不会由本替身产生回调。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_can_backend_activate_notification(
  platform_can_instance_t can,
  uint32_t notifications
) {
  (void)can;
  (void)notifications;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟停用 CAN 通知并固定报告成功。
 * @param can CAN 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @param notifications 通知位掩码；本替身忽略该参数，不修改 NVIC 或 HAL 中断使能。
 * @retval PLATFORM_STATUS_OK 固定返回，但不会停用任何真实通知。
 * @post 不改变中断配置，也不等待在途回调。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_can_backend_deactivate_notification(
  platform_can_instance_t can,
  uint32_t notifications
) {
  (void)can;
  (void)notifications;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 返回测试注入的 CAN 错误位。
 * @param can CAN 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @return s_backend_error 的当前值；测试直接写入 platform_can_error_t 位掩码，
 *         本替身不添加 PARAMETER 位，也不映射 HAL 错误。
 * @post 只读取 s_backend_error，不清除错误、不改变总线状态，也不停止外设。
 * @note 这是测试替身；生产后端的错误映射和非法实例处理不由本函数模拟。
 */
uint32_t platform_can_backend_get_error(platform_can_instance_t can) {
  (void)can;
  return s_backend_error;
}

/**
 * @brief 记录 CAN 事件并保存测试事件快照。
 * @param can 回调实例；断言其必须等于 PLATFORM_CAN_1。
 * @param event 非空事件指针；函数按值复制整个事件，调用后不保留该指针。
 * @param context 回调上下文；断言其必须等于 &s_event_count。
 * @post s_event_count 增加 1，s_last_event 更新为 *event。
 * @warning 任一断言失败都会终止进程；真实回调可能位于中断上下文，
 *          因此不得在此阻塞、分配内存或执行长耗时操作。
 */
static void can_callback(
  platform_can_instance_t can,
  const platform_can_event_t *event,
  void *context
) {
  assert(can == PLATFORM_CAN_1);
  assert(event != NULL);
  assert(context == &s_event_count);
  s_event_count++;
  s_last_event = *event;
}

/**
 * @brief 验证 CAN 帧参数校验、错误位和事件分发。
 * @post 断言覆盖合法发送、DLC 越界的通用与模块错误位、注入超时、
 *       过滤器配置的后端拒绝、回调重复注册和错误事件复制。
 * @note 使用本文件中的 CAN 后端替身，不访问真实总线、邮箱或中断；
 *       任一断言失败会终止进程。
 */
static void test_can_contract(void) {
  s_transmit_status = PLATFORM_STATUS_OK;
  s_filter_status = PLATFORM_STATUS_OK;
  s_backend_error = PLATFORM_CAN_ERROR_NONE;
  s_event_count = 0U;

  platform_can_frame_t frame = {
    .id = 0x123U,
    .dlc = 8U,
    .id_type = PLATFORM_CAN_ID_STANDARD,
    .remote = false,
  };
  assert(Platform_CAN_Transmit(PLATFORM_CAN_1, &frame, 10U) == PLATFORM_STATUS_OK);

  frame.dlc = 9U;
  assert(Platform_CAN_Transmit(PLATFORM_CAN_1, &frame, 10U) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_CAN_GetLastError(PLATFORM_CAN_1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);
  assert((Platform_CAN_GetLastError(PLATFORM_CAN_1) &
          PLATFORM_CAN_ERROR_PARAMETER) != 0U);

  frame.dlc = 8U;
  s_transmit_status = PLATFORM_STATUS_TIMEOUT;
  assert(Platform_CAN_Transmit(PLATFORM_CAN_1, &frame, 1U) ==
         PLATFORM_STATUS_TIMEOUT);
  assert((Platform_CAN_GetLastError(PLATFORM_CAN_1) &
          PLATFORM_ERROR_TIMEOUT) != 0U);
  assert((Platform_CAN_GetLastError(PLATFORM_CAN_1) &
          PLATFORM_CAN_ERROR_TIMEOUT) != 0U);

  const platform_can_filter_config_t filter = {
    .bank = 0U,
    .id = 0x123U,
    .mask = 0x7FFU,
    .mode = PLATFORM_CAN_FILTER_MASK,
    .fifo = PLATFORM_CAN_FIFO_0,
    .id_type = PLATFORM_CAN_ID_STANDARD,
    .remote = false,
    .enabled = true,
  };
  assert(Platform_CAN_ConfigureFilter(PLATFORM_CAN_1, &filter) == PLATFORM_STATUS_OK);

  s_filter_status = PLATFORM_STATUS_INVALID_ARGUMENT;
  assert(Platform_CAN_ConfigureFilter(PLATFORM_CAN_1, &filter) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_CAN_GetLastError(PLATFORM_CAN_1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);

  const platform_can_callback_binding_t callbacks = {
    .callback = can_callback,
    .context = &s_event_count,
  };
  assert(Platform_CAN_RegisterCallback(PLATFORM_CAN_1, &callbacks) ==
         PLATFORM_STATUS_OK);
  assert(Platform_CAN_RegisterCallback(PLATFORM_CAN_1, &callbacks) ==
         PLATFORM_STATUS_BUSY);

  const platform_can_event_t event = {
    .type = PLATFORM_CAN_EVENT_ERROR,
    .error = PLATFORM_CAN_ERROR_BUS_OFF,
  };
  platform_can_dispatch_event(PLATFORM_CAN_1, &event);
  assert(s_event_count == 1U);
  assert(s_last_event.type == PLATFORM_CAN_EVENT_ERROR);
  assert(s_last_event.error == PLATFORM_CAN_ERROR_BUS_OFF);
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 * @retval 0 CAN 参数、错误和事件契约均通过。
 * @post 进程以成功状态退出，不保留动态资源或硬件句柄。
 * @warning 任一契约断言失败都会终止进程，因此失败时不会返回状态码。
 */
int main(void) {
  test_can_contract();
  return 0;
}

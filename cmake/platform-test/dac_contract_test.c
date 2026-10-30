/**
 * @file dac_contract_test.c
 * @brief DAC Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_dac_backend.h"
#include "platform_dac.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t s_backend_error;
static uint32_t s_event_count;
static platform_dac_event_t s_last_event;

/**
 * @brief 查询测试替身接受的 DAC 实例。
 * @param dac DAC 实例枚举值；测试替身只接受 PLATFORM_DAC_1。
 * @retval true dac 等于 PLATFORM_DAC_1。
 * @retval false dac 为其他枚举值。
 * @post 只比较枚举值，不解析 HAL 句柄、不访问 DAC 外设，也不修改全局状态。
 * @note 这是测试替身；生产后端是否启用实例以及句柄是否有效的规则不由本函数模拟。
 */
bool platform_dac_backend_is_valid(platform_dac_instance_t dac) {
  return dac == PLATFORM_DAC_1;
}

/**
 * @brief 模拟设置 DAC 输出码值并固定报告成功。
 * @param dac DAC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @param channel 输出通道；本替身忽略该参数，不校验通道编号。
 * @param alignment 输出数值对齐方式；本替身忽略该参数，不检查是否与配置一致。
 * @param value 原始模拟码值；本替身忽略该参数，不检查范围和单位。
 * @retval PLATFORM_STATUS_OK 固定返回，未写 DAC 数据保持寄存器。
 * @post 不改变 DAC 输出、触发状态或全局测试状态。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_dac_backend_set_value(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel,
  platform_dac_alignment_t alignment,
  uint32_t value
) {
  (void)dac;
  (void)channel;
  (void)alignment;
  (void)value;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 DAC 通道并固定报告成功。
 * @param dac DAC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @param channel 输出通道；本替身忽略该参数，不校验通道编号。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动真实通道。
 * @post 不修改 DAC 触发、输出或引脚状态。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_dac_backend_start(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  (void)dac;
  (void)channel;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟停止 DAC 通道并固定报告成功。
 * @param dac DAC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @param channel 输出通道；本替身忽略该参数，不校验通道编号。
 * @retval PLATFORM_STATUS_OK 固定返回，未停止真实通道。
 * @post 不修改 DAC 触发、输出或引脚状态。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_dac_backend_stop(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  (void)dac;
  (void)channel;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 DAC DMA 输出并固定报告成功。
 * @param dac DAC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @param channel 输出通道；本替身忽略该参数，不校验通道编号。
 * @param alignment 样本对齐方式；本替身忽略该参数，不检查是否与配置一致。
 * @param data 原始 uint32_t 样本数组；本替身不读取、不写入，也不保存该指针。
 * @param length 样本数；本替身忽略该参数，不检查是否大于 0。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动 DMA，也未读取 data。
 * @post data 内容和长度均不变，不产生完成或错误事件。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_dac_backend_start_dma(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel,
  platform_dac_alignment_t alignment,
  const uint32_t *data,
  uint32_t length
) {
  (void)dac;
  (void)channel;
  (void)alignment;
  (void)data;
  (void)length;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟停止 DAC DMA 输出并固定报告成功。
 * @param dac DAC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @param channel 输出通道；本替身忽略该参数，不校验通道编号。
 * @retval PLATFORM_STATUS_OK 固定返回，未停止真实 DMA 传输。
 * @post 不修改 DMA 控制器或 DAC 状态，也不取消任何在途事件。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_dac_backend_stop_dma(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  (void)dac;
  (void)channel;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 返回测试注入的 DAC 错误位。
 * @param dac DAC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @return s_backend_error 的当前值；测试直接写入 platform_dac_error_t 位掩码，
 *         本替身不添加 UNKNOWN 位，也不映射 HAL 错误。
 * @post 只读取 s_backend_error，不清除错误、不停止通道，也不访问 DAC 寄存器。
 * @note 这是测试替身；生产后端的错误映射和非法实例处理不由本函数模拟。
 */
uint32_t platform_dac_backend_get_error(platform_dac_instance_t dac) {
  (void)dac;
  return s_backend_error;
}

/**
 * @brief 记录 DAC 分发器送达的测试事件。
 * @param dac 触发事件的 DAC 实例；断言必须为 PLATFORM_DAC_1。
 * @param event 非空事件对象；仅在本次回调期间有效，整体复制到 s_last_event。
 * @param context 注册时传入的上下文；断言必须为 &s_event_count。
 * @pre 必须通过 platform_dac_dispatch_event 或等价方式提供合法 dac、event 和 context。
 * @post s_event_count 加一，s_last_event 保存本次事件的完整副本。
 * @warning 任一 assert 失败都会终止测试进程；该测试在任务上下文同步调用回调，
 *          不模拟真实中断时序或硬件副作用。
 * @note 测试回调不阻塞、不分配内存，也不访问 DAC/DMA 寄存器。
 */
static void dac_callback(
  platform_dac_instance_t dac,
  const platform_dac_event_t *event,
  void *context
) {
  assert(dac == PLATFORM_DAC_1);
  assert(event != NULL);
  assert(context == &s_event_count);
  s_event_count++;
  s_last_event = *event;
}

/**
 * @brief 验证 DAC 参数校验、回调所有权和事件分发。
 * @post 成功返回时，8 位越界码值与空 DMA 缓冲区均被公共层拒绝；首次回调注册
 *       成功、重复注册返回 BUSY；一次事件分发后 s_event_count 为 1，且
 *       s_last_event 保存 UNDERRUN、通道 1 和 DMA_UNDERRUN 错误位。
 * @warning 任一 assert 失败都会终止测试进程，不返回可恢复的错误状态。
 * @note 后端均为本文件测试桩，不访问 DAC、DMA 或真实中断硬件。
 */
static void test_dac_contract(void) {
  s_backend_error = PLATFORM_DAC_ERROR_NONE;
  s_event_count = 0U;

  assert(Platform_DAC_SetValue(
           PLATFORM_DAC_1,
           PLATFORM_DAC_CHANNEL_1,
           PLATFORM_DAC_ALIGN_8_BIT_RIGHT,
           0xFFU
         ) == PLATFORM_STATUS_OK);
  assert(Platform_DAC_SetValue(
           PLATFORM_DAC_1,
           PLATFORM_DAC_CHANNEL_1,
           PLATFORM_DAC_ALIGN_8_BIT_RIGHT,
           0x100U
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_DAC_GetLastError(PLATFORM_DAC_1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);

  const uint32_t samples[2] = {1U, 2U};
  assert(Platform_DAC_StartDMA(
           PLATFORM_DAC_1,
           PLATFORM_DAC_CHANNEL_2,
           PLATFORM_DAC_ALIGN_12_BIT_RIGHT,
           samples,
           2U
         ) == PLATFORM_STATUS_OK);
  assert(Platform_DAC_StartDMA(
           PLATFORM_DAC_1,
           PLATFORM_DAC_CHANNEL_2,
           PLATFORM_DAC_ALIGN_12_BIT_RIGHT,
           NULL,
           2U
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);

  const platform_dac_callback_binding_t callbacks = {
    .callback = dac_callback,
    .context = &s_event_count,
  };
  assert(Platform_DAC_RegisterCallback(PLATFORM_DAC_1, &callbacks) ==
         PLATFORM_STATUS_OK);
  assert(Platform_DAC_RegisterCallback(PLATFORM_DAC_1, &callbacks) ==
         PLATFORM_STATUS_BUSY);

  s_backend_error = PLATFORM_DAC_ERROR_DMA_UNDERRUN;
  const platform_dac_event_t event = {
    .type = PLATFORM_DAC_EVENT_UNDERRUN,
    .channel = PLATFORM_DAC_CHANNEL_1,
    .error = s_backend_error,
  };
  platform_dac_dispatch_event(PLATFORM_DAC_1, &event);
  assert(s_event_count == 1U);
  assert(s_last_event.type == PLATFORM_DAC_EVENT_UNDERRUN);
  assert(s_last_event.channel == PLATFORM_DAC_CHANNEL_1);
  assert(s_last_event.error == PLATFORM_DAC_ERROR_DMA_UNDERRUN);
}

/**
 * @brief 运行 DAC 契约测试并返回进程状态。
 * @return 固定返回 0；只有所有内层断言通过才会到达该返回语句。
 * @post 测试全局状态保留 DAC 契约用例最后一次分发的事件和计数。
 * @warning 任一 assert 失败都会终止进程，因此不会返回非零退出码。
 * @note 仅供 CTest 启动本测试可执行文件时调用。
 */
int main(void) {
  test_dac_contract();
  return 0;
}

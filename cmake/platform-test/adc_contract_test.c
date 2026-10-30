/**
 * @file adc_contract_test.c
 * @brief ADC Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_adc_backend.h"
#include "platform_adc.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool s_supports_dma;
static platform_status_t s_poll_status;
static uint32_t s_value;
static uint32_t s_backend_error;
static uint32_t s_event_count;

/**
 * @brief 查询测试替身接受的 ADC 实例。
 * @param adc ADC 实例枚举值；测试替身只接受 PLATFORM_ADC_1。
 * @retval true adc 等于 PLATFORM_ADC_1。
 * @retval false adc 为其他枚举值。
 * @post 只比较枚举值，不解析 HAL 句柄、不访问外设，也不修改任何全局状态。
 * @note 这是测试替身；生产后端是否启用实例以及句柄是否有效的规则不由本函数模拟。
 */
bool platform_adc_backend_is_valid(platform_adc_instance_t adc) {
  return adc == PLATFORM_ADC_1;
}

/**
 * @brief 返回测试注入的 DMA 能力标志。
 * @param adc ADC 实例枚举值；本替身忽略该参数，不校验实例或 DMA 映射。
 * @retval true s_supports_dma 当前为 true。
 * @retval false s_supports_dma 当前为 false。
 * @post 只读取 s_supports_dma，不启动 DMA，也不修改错误快照。
 * @note 测试通过直接写 s_supports_dma 注入能力；生产后端的真实能力由 MCU 和实例映射决定。
 */
bool platform_adc_backend_supports_dma(platform_adc_instance_t adc) {
  (void)adc;
  return s_supports_dma;
}

/**
 * @brief 模拟轮询模式转换启动并固定报告成功。
 * @param adc ADC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @retval PLATFORM_STATUS_OK 固定返回，未检查任何硬件状态。
 * @post 不启动转换、不读写 ADC 寄存器，也不改变测试全局状态。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_adc_backend_start(platform_adc_instance_t adc) {
  (void)adc;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟轮询模式转换停止并固定报告成功。
 * @param adc ADC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @retval PLATFORM_STATUS_OK 固定返回，未检查任何硬件状态。
 * @post 不停止真实转换、不读写 ADC 寄存器，也不改变测试全局状态。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_adc_backend_stop(platform_adc_instance_t adc) {
  (void)adc;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 返回测试注入的轮询完成状态。
 * @param adc ADC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @param timeout 超时时间，单位毫秒；本替身忽略该参数，不等待也不检查是否为零或无限值。
 * @return s_poll_status 的当前值；测试只注入 PLATFORM_STATUS_OK 或
 *         PLATFORM_STATUS_TIMEOUT，函数不访问 ADC 或 HAL。
 * @post 只读取 s_poll_status，不读取数据寄存器、不停止转换，也不修改错误快照。
 * @note 这是测试替身；生产后端的超时和硬件错误行为不由本函数模拟。
 */
platform_status_t platform_adc_backend_poll(
  platform_adc_instance_t adc,
  uint32_t timeout
) {
  (void)adc;
  (void)timeout;
  return s_poll_status;
}

/**
 * @brief 将测试注入的原始 ADC 码值复制到调用方存储。
 * @param adc ADC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @param value 输出指针，接收 s_value 的当前值；调用方拥有存储；本替身不检查 NULL。
 * @retval PLATFORM_STATUS_OK 固定返回，并已将 s_value 写入 *value。
 * @pre value 非 NULL 且指向至少一个可写 uint32_t；违反时解引用导致未定义行为。
 * @post s_value 不变，*value 被覆盖；不读取 ADC 寄存器、不等待转换，也不修改错误快照。
 * @warning 测试替身不执行参数校验，NULL 指针会直接崩溃而不是返回 INVALID_ARGUMENT。
 */
platform_status_t platform_adc_backend_get_value(
  platform_adc_instance_t adc,
  uint32_t *value
) {
  (void)adc;
  *value = s_value;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 ADC 中断转换并固定报告成功。
 * @param adc ADC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @retval PLATFORM_STATUS_OK 固定返回，未检查任何硬件状态。
 * @post 不启动转换、不使能中断，也不会产生完成或错误事件。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_adc_backend_start_it(
  platform_adc_instance_t adc
) {
  (void)adc;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟停止 ADC 中断转换并固定报告成功。
 * @param adc ADC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @retval PLATFORM_STATUS_OK 固定返回，未检查任何硬件状态。
 * @post 不修改真实中断使能位，不等待或取消任何回调。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_adc_backend_stop_it(
  platform_adc_instance_t adc
) {
  (void)adc;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 ADC DMA 转换并固定报告成功。
 * @param adc ADC 实例枚举值；本替身忽略该参数，不校验实例或 DMA 能力。
 * @param data DMA 目标数组；本替身不读取、不写入，也不保存该指针。
 * @param length 转换次数；本替身忽略该参数，不检查是否大于 0。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动 DMA，也未初始化 data。
 * @post data 内容和长度均不变，不产生 DMA 完成或半完成事件。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_adc_backend_start_dma(
  platform_adc_instance_t adc,
  uint32_t *data,
  uint32_t length
) {
  (void)adc;
  (void)data;
  (void)length;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟停止 ADC DMA 转换并固定报告成功。
 * @param adc ADC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @retval PLATFORM_STATUS_OK 固定返回，未检查任何硬件状态。
 * @post 不修改 DMA 控制器或 ADC 状态，也不终止任何真实传输。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_adc_backend_stop_dma(
  platform_adc_instance_t adc
) {
  (void)adc;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 返回测试注入的 ADC 错误位。
 * @param adc ADC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @return s_backend_error 的当前值；测试直接写入 platform_adc_error_t 位掩码，
 *         本替身不添加 UNKNOWN 位，也不映射 HAL 错误。
 * @post 只读取 s_backend_error，不清除错误、不停止转换，也不访问 ADC 寄存器。
 * @note 这是测试替身；生产后端的错误映射和非法实例处理不由本函数模拟。
 */
uint32_t platform_adc_backend_get_error(platform_adc_instance_t adc) {
  (void)adc;
  return s_backend_error;
}

/**
 * @brief 统计 ADC 事件回调的调用次数。
 * @param adc 回调实例；断言其必须等于 PLATFORM_ADC_1。
 * @param event 事件指针；断言其非空，但本回调不读取事件内容。
 * @param context 回调上下文；断言其必须等于 &s_event_count。
 * @post 每次调用将 s_event_count 增加 1，不保存事件，也不转发硬件资源。
 * @warning 任一断言失败都会终止进程；真实回调可能位于中断上下文，
 *          因此不得在此阻塞、分配内存或执行长耗时操作。
 */
static void adc_callback(
  platform_adc_instance_t adc,
  const platform_adc_event_t *event,
  void *context
) {
  assert(adc == PLATFORM_ADC_1);
  assert(event != NULL);
  assert(context == &s_event_count);
  s_event_count++;
}

/**
 * @brief 验证 ADC 参数校验、事件分发和错误路径。
 * @post 覆盖同步读取、空输出指针、DMA 不支持/支持、轮询超时、
 *       回调重复注册和错误事件分发；相关计数与错误位已断言。
 * @note 使用本文件中的 ADC 后端替身，不访问真实外设或中断；
 *       任一断言失败会终止进程，函数不会以失败状态正常返回。
 */
static void test_adc_contract(void) {
  s_supports_dma = false;
  s_poll_status = PLATFORM_STATUS_OK;
  s_value = 0x5A5U;
  s_backend_error = PLATFORM_ADC_ERROR_NONE;
  s_event_count = 0U;

  uint32_t value = 0U;
  assert(Platform_ADC_Read(PLATFORM_ADC_1, &value, 10U) == PLATFORM_STATUS_OK);
  assert(value == s_value);
  assert(Platform_ADC_GetValue(PLATFORM_ADC_1, &value) == PLATFORM_STATUS_OK);
  assert(Platform_ADC_GetValue(PLATFORM_ADC_1, NULL) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);

  uint32_t dma_data[2] = {0};
  assert(Platform_ADC_StartDMA(PLATFORM_ADC_1, dma_data, 2U) ==
         PLATFORM_STATUS_UNSUPPORTED);
  assert((Platform_ADC_GetLastError(PLATFORM_ADC_1) &
          PLATFORM_ERROR_UNSUPPORTED) != 0U);

  s_supports_dma = true;
  assert(Platform_ADC_StartDMA(PLATFORM_ADC_1, dma_data, 2U) == PLATFORM_STATUS_OK);

  s_poll_status = PLATFORM_STATUS_TIMEOUT;
  assert(Platform_ADC_Read(PLATFORM_ADC_1, &value, 1U) == PLATFORM_STATUS_TIMEOUT);
  assert((Platform_ADC_GetLastError(PLATFORM_ADC_1) &
          PLATFORM_ERROR_TIMEOUT) != 0U);

  const platform_adc_callback_binding_t callbacks = {
    .callback = adc_callback,
    .context = &s_event_count,
  };
  assert(Platform_ADC_RegisterCallback(PLATFORM_ADC_1, &callbacks) ==
         PLATFORM_STATUS_OK);
  assert(Platform_ADC_RegisterCallback(PLATFORM_ADC_1, &callbacks) ==
         PLATFORM_STATUS_BUSY);

  s_backend_error = PLATFORM_ADC_ERROR_OVERRUN;
  platform_adc_dispatch_event(PLATFORM_ADC_1, PLATFORM_ADC_EVENT_ERROR);
  assert(s_event_count == 1U);
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 * @retval 0 所有契约断言均通过。
 * @post 进程以成功状态退出；测试不启动硬件，也不保留资源。
 * @warning 任一契约断言失败都会终止进程，因此不会返回失败状态码。
 */
int main(void) {
  test_adc_contract();
  return 0;
}

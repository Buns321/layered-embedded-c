/**
 * @file spi_contract_test.c
 * @brief SPI Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_spi_backend.h"
#include "platform_spi.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static platform_spi_data_width_t s_data_width;
static platform_status_t s_transmit_status;
static uint32_t s_backend_error;
static uint32_t s_event_count;

/**
 * @brief 查询测试替身接受的 SPI 实例。
 * @param spi SPI 实例编号；测试替身只接受 PLATFORM_SPI_1。
 * @retval true spi 等于 PLATFORM_SPI_1。
 * @retval false spi 为其他枚举值。
 * @post 只比较枚举值，不解析 HAL 句柄、不访问外设，也不修改全局状态。
 * @note 这是测试替身；生产后端的实例启用和句柄映射规则不由本函数模拟。
 */
bool platform_spi_backend_is_valid(platform_spi_instance_t spi) {
  return spi == PLATFORM_SPI_1;
}

/**
 * @brief 将测试注入的 SPI 数据宽度复制到调用方存储。
 * @param spi SPI 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param data_width 输出参数，接收 s_data_width 的当前值；本替身不检查 NULL。
 * @retval PLATFORM_STATUS_OK 固定返回，并已将 s_data_width 写入 *data_width。
 * @pre data_width 非 NULL 且指向可写存储；违反时解引用导致未定义行为。
 * @post s_data_width 不变，*data_width 被覆盖；不访问 SPI 句柄或总线。
 * @warning 测试替身不执行参数校验，NULL data_width 会直接崩溃而不是返回 INVALID_ARGUMENT。
 */
platform_status_t platform_spi_backend_get_data_width(
  platform_spi_instance_t spi,
  platform_spi_data_width_t *data_width
) {
  (void)spi;
  *data_width = s_data_width;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 返回测试注入的 SPI 阻塞发送状态。
 * @param spi SPI 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param data 发送缓冲区；本替身不读取，也不保存该指针。
 * @param size 发送字节数；本替身忽略该参数，不检查范围或 16 位长度约束。
 * @param timeout 超时时间，单位 ms；本替身忽略该参数且不等待。
 * @return s_transmit_status 的当前值；测试只注入 PLATFORM_STATUS_OK 或
 *         PLATFORM_STATUS_ERROR，本替身不访问 SPI 总线。
 * @post s_transmit_status 和 data 指向的内容均不变，不访问片选或 SPI 寄存器。
 * @note 这是测试替身；生产后端还可能返回 BUSY、TIMEOUT 或 INVALID_ARGUMENT。
 */
platform_status_t platform_spi_backend_transmit(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)spi;
  (void)data;
  (void)size;
  (void)timeout;
  return s_transmit_status;
}

/**
 * @brief 模拟 SPI 阻塞接收并固定报告成功，但不写入缓冲区。
 * @param spi SPI 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param data 接收缓冲区；本替身不读取、不写入，也不保存该指针。
 * @param size 接收字节数；本替身忽略该参数，不检查范围或 16 位长度约束。
 * @param timeout 超时时间，单位 ms；本替身忽略该参数且不等待。
 * @retval PLATFORM_STATUS_OK 固定返回，未接收数据，data 内容不变。
 * @post data 内容和长度均不变，不访问 SPI 总线、片选或寄存器。
 * @note 测试替身不会返回生产后端声明的 ERROR、BUSY、TIMEOUT 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_spi_backend_receive(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)spi;
  (void)data;
  (void)size;
  (void)timeout;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟 SPI 阻塞全双工传输并固定报告成功，但不搬运数据。
 * @param spi SPI 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param tx_data 发送缓冲区；本替身不读取，也不保存该指针。
 * @param rx_data 接收缓冲区；本替身不读取、不写入，也不保存该指针。
 * @param size 每个方向的传输字节数；本替身忽略该参数，不检查范围或对齐。
 * @param timeout 超时时间，单位 ms；本替身忽略该参数且不等待。
 * @retval PLATFORM_STATUS_OK 固定返回，未发送或接收数据。
 * @post tx_data 和 rx_data 内容均不变，不访问 SPI 总线或片选。
 * @note 测试替身不会返回生产后端声明的 ERROR、BUSY、TIMEOUT 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_spi_backend_transmit_receive(
  platform_spi_instance_t spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size,
  uint32_t timeout
) {
  (void)spi;
  (void)tx_data;
  (void)rx_data;
  (void)size;
  (void)timeout;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 SPI 中断发送并固定报告成功。
 * @param spi SPI 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param data 发送缓冲区；本替身不读取，也不保存该指针。
 * @param size 发送字节数；本替身忽略该参数，不检查范围或对齐。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动中断发送。
 * @post 不修改缓冲区、NVIC 或 SPI 状态，也不会产生完成或错误事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_spi_backend_transmit_it(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size
) {
  (void)spi;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 SPI 中断接收并固定报告成功。
 * @param spi SPI 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param data 接收缓冲区；本替身不读取、不写入，也不保存该指针。
 * @param size 接收字节数；本替身忽略该参数，不检查范围或对齐。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动中断接收。
 * @post 不修改缓冲区、NVIC 或 SPI 状态，也不会产生完成或错误事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_spi_backend_receive_it(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size
) {
  (void)spi;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 SPI 中断全双工传输并固定报告成功。
 * @param spi SPI 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param tx_data 发送缓冲区；本替身不读取，也不保存该指针。
 * @param rx_data 接收缓冲区；本替身不读取、不写入，也不保存该指针。
 * @param size 每个方向的传输字节数；本替身忽略该参数，不检查范围或对齐。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动中断传输。
 * @post 两个缓冲区均不变，不修改 NVIC 或 SPI 状态，也不会产生完成事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_spi_backend_transmit_receive_it(
  platform_spi_instance_t spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
) {
  (void)spi;
  (void)tx_data;
  (void)rx_data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 SPI DMA 发送并固定报告成功。
 * @param spi SPI 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param data 发送缓冲区；本替身不读取，也不保存该指针。
 * @param size 发送字节数；本替身忽略该参数，不检查范围或对齐。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动 DMA 发送。
 * @post 不修改缓冲区、DMA 控制器或 SPI 状态，也不会产生完成或错误事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_spi_backend_transmit_dma(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size
) {
  (void)spi;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 SPI DMA 接收并固定报告成功。
 * @param spi SPI 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param data 接收缓冲区；本替身不读取、不写入，也不保存该指针。
 * @param size 接收字节数；本替身忽略该参数，不检查范围或对齐。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动 DMA 接收。
 * @post 不修改缓冲区、DMA 控制器或 SPI 状态，也不会产生完成或错误事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_spi_backend_receive_dma(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size
) {
  (void)spi;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 SPI DMA 全双工传输并固定报告成功。
 * @param spi SPI 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param tx_data 发送缓冲区；本替身不读取，也不保存该指针。
 * @param rx_data 接收缓冲区；本替身不读取、不写入，也不保存该指针。
 * @param size 每个方向的传输字节数；本替身忽略该参数，不检查范围或对齐。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动 DMA 传输。
 * @post 两个缓冲区均不变，不修改 DMA 控制器或 SPI 状态，也不会产生完成事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_spi_backend_transmit_receive_dma(
  platform_spi_instance_t spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
) {
  (void)spi;
  (void)tx_data;
  (void)rx_data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟同步中止 SPI 传输并固定报告成功。
 * @param spi SPI 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @retval PLATFORM_STATUS_OK 固定返回，未中止任何真实传输。
 * @post 不修改 SPI 状态、DMA 状态或 HAL 错误码，调用方仍不能据此假定硬件已停止。
 * @note 测试替身不会返回生产后端声明的 ERROR、BUSY 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_spi_backend_abort(platform_spi_instance_t spi) {
  (void)spi;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 SPI 异步中止并固定报告成功。
 * @param spi SPI 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动真实中止流程。
 * @post 不修改 SPI 或 DMA 状态，也不会产生 ABORT_COMPLETE 事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_spi_backend_abort_it(
  platform_spi_instance_t spi
) {
  (void)spi;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 返回测试注入的 SPI 错误位。
 * @param spi SPI 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @return s_backend_error 的当前值；测试直接写入 platform_spi_error_t 位掩码，
 *         本替身不添加 UNKNOWN 位，也不映射 HAL 错误。
 * @post 只读取 s_backend_error，不清除错误、不改变传输状态，也不访问 SPI 寄存器。
 * @note 这是测试替身；生产后端的错误映射和非法实例处理不由本函数模拟。
 */
uint32_t platform_spi_backend_get_error(platform_spi_instance_t spi) {
  (void)spi;
  return s_backend_error;
}

/**
 * @brief 记录 SPI 完成或错误事件。
 * @param spi 事件所属 SPI 实例；断言必须为 PLATFORM_SPI_1。
 * @param event 平台事件快照；断言非空，但函数不检查事件类型或错误值。
 * @param context 注册上下文；断言必须等于 &s_event_count。
 * @pre 通过 Platform_SPI_RegisterCallback 注册，且 context 生命周期仍有效。
 * @post s_event_count 加一；其他测试状态不变。
 * @warning 实例、事件指针或上下文不符合预期时 assert 会终止进程。
 * @note 测试由 platform_spi_dispatch_event 同步调用；硬件路径通常来自 ISR，需保持短小。
 */
static void spi_callback(
  platform_spi_instance_t spi,
  const platform_spi_event_t *event,
  void *context
) {
  assert(spi == PLATFORM_SPI_1);
  assert(event != NULL);
  assert(context == &s_event_count);
  s_event_count++;
}

/**
 * @brief 验证 SPI 参数校验、收发和事件分发。
 * @pre SPI 公共实现与测试替身已链接，s_data_width、s_transmit_status、
 *      s_backend_error 和 s_event_count 可写。
 * @post 已覆盖 16 位奇数长度拒绝、HAL 错误映射、成功发送、回调去重和错误事件；
 *       结束时 s_data_width 为 8 位、s_backend_error 为 OVR、s_event_count 为 1。
 * @warning 使用 assert 校验每一步；任一失败会调用 abort，进程不会正常返回。
 * @note 测试替身不搬运 SPI 数据，也不启动真实中断或 DMA。
 */
static void test_spi_contract(void) {
  const uint8_t buffer[4] = {0U, 1U, 2U, 3U};
  s_transmit_status = PLATFORM_STATUS_OK;
  s_backend_error = PLATFORM_SPI_ERROR_NONE;
  s_event_count = 0U;

  platform_spi_data_width_t width;
  s_data_width = PLATFORM_SPI_DATA_WIDTH_16_BIT;
  assert(Platform_SPI_GetDataWidth(PLATFORM_SPI_1, &width) == PLATFORM_STATUS_OK);
  assert(width == PLATFORM_SPI_DATA_WIDTH_16_BIT);
  assert(Platform_SPI_Transmit(PLATFORM_SPI_1, buffer, 3U, 10U) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_SPI_GetLastError(PLATFORM_SPI_1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);

  s_data_width = PLATFORM_SPI_DATA_WIDTH_8_BIT;
  s_transmit_status = PLATFORM_STATUS_ERROR;
  assert(Platform_SPI_Transmit(PLATFORM_SPI_1, buffer, 3U, 10U) ==
         PLATFORM_STATUS_ERROR);
  assert((Platform_SPI_GetLastError(PLATFORM_SPI_1) &
          PLATFORM_ERROR_HAL) != 0U);

  s_transmit_status = PLATFORM_STATUS_OK;
  assert(Platform_SPI_Transmit(PLATFORM_SPI_1, buffer, 3U, 10U) ==
         PLATFORM_STATUS_OK);

  const platform_spi_callback_binding_t callbacks = {
    .callback = spi_callback,
    .context = &s_event_count,
  };
  assert(Platform_SPI_RegisterCallback(PLATFORM_SPI_1, &callbacks) ==
         PLATFORM_STATUS_OK);
  assert(Platform_SPI_RegisterCallback(PLATFORM_SPI_1, &callbacks) ==
         PLATFORM_STATUS_BUSY);

  s_backend_error = PLATFORM_SPI_ERROR_OVR;
  platform_spi_dispatch_event(PLATFORM_SPI_1, PLATFORM_SPI_EVENT_ERROR);
  assert(s_event_count == 1U);
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 * @retval 0 test_spi_contract 的全部断言通过。
 * @pre SPI 测试替身已链接且全局观察状态可写。
 * @post 测试替身状态保留；没有动态内存、线程或真实 SPI 资源。
 * @warning 断言失败会直接终止进程，不会返回非零退出码。
 */
int main(void) {
  test_spi_contract();
  return 0;
}

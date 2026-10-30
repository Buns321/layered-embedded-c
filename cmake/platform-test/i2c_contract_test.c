/**
 * @file i2c_contract_test.c
 * @brief I2C Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_i2c_backend.h"
#include "platform_i2c.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static platform_status_t s_transmit_status;
static platform_status_t s_probe_status;
static uint32_t s_backend_error;
static uint32_t s_event_count;
static uint32_t s_probe_trials;

/**
 * @brief 查询测试替身接受的 I2C 实例。
 * @param i2c 平台 I2C 实例编号；测试替身只接受 PLATFORM_I2C_1。
 * @retval true i2c 等于 PLATFORM_I2C_1。
 * @retval false i2c 为其他枚举值。
 * @post 只比较枚举值，不解析 HAL 句柄、不访问总线，也不修改全局状态。
 * @note 这是测试替身；生产后端的实例启用和句柄映射规则不由本函数模拟。
 */
bool platform_i2c_backend_is_valid(platform_i2c_instance_t i2c) {
  return i2c == PLATFORM_I2C_1;
}

/**
 * @brief 返回测试注入的 I2C 阻塞发送状态。
 * @param i2c 平台 I2C 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param address 7 位从设备地址；本替身忽略该参数，不检查范围。
 * @param data 发送缓冲区；本替身不读取，也不保存该指针。
 * @param size 发送字节数；本替身忽略该参数，不检查范围。
 * @param timeout 超时时间，单位 ms；本替身忽略该参数且不等待。
 * @return s_transmit_status 的当前值；测试只注入 PLATFORM_STATUS_OK 或
 *         PLATFORM_STATUS_ERROR，本替身不访问 I2C 总线。
 * @post s_transmit_status 和 data 指向的内容均不变，不产生总线时序或错误快照。
 * @note 这是测试替身；生产后端还可能返回 BUSY、TIMEOUT 或 INVALID_ARGUMENT。
 */
platform_status_t platform_i2c_backend_master_transmit(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
  (void)timeout;
  return s_transmit_status;
}

/**
 * @brief 模拟 I2C 阻塞接收并固定报告成功，但不写入缓冲区。
 * @param i2c 平台 I2C 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param address 7 位从设备地址；本替身忽略该参数，不检查范围。
 * @param data 接收缓冲区；本替身不读取、不写入，也不保存该指针。
 * @param size 接收字节数；本替身忽略该参数，不检查范围。
 * @param timeout 超时时间，单位 ms；本替身忽略该参数且不等待。
 * @retval PLATFORM_STATUS_OK 固定返回，未接收数据，data 内容不变。
 * @post data 内容和长度均不变，不产生总线时序或错误快照。
 * @note 测试替身不会返回生产后端声明的 BUSY、TIMEOUT、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_i2c_backend_master_receive(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
  (void)timeout;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟 I2C 存储器写并固定报告成功，但不发送任何字节。
 * @param i2c 平台 I2C 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param address 7 位从设备地址；本替身忽略该参数，不检查范围。
 * @param mem_address 器件内部存储或寄存器地址；本替身忽略该参数。
 * @param mem_address_size 存储地址宽度（字节）；本替身忽略该参数，不检查 1/2 限制。
 * @param data 发送缓冲区；本替身不读取，也不保存该指针。
 * @param size 写入字节数；本替身忽略该参数，不检查范围。
 * @param timeout 超时时间，单位 ms；本替身忽略该参数且不等待。
 * @retval PLATFORM_STATUS_OK 固定返回，未写器件，也未产生停止条件。
 * @post data 内容和长度均不变，不访问 I2C 总线或器件存储。
 * @note 测试替身不会返回生产后端声明的 BUSY、TIMEOUT、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_i2c_backend_mem_write(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)i2c;
  (void)address;
  (void)mem_address;
  (void)mem_address_size;
  (void)data;
  (void)size;
  (void)timeout;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟 I2C 存储器读并固定报告成功，但不写入缓冲区。
 * @param i2c 平台 I2C 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param address 7 位从设备地址；本替身忽略该参数，不检查范围。
 * @param mem_address 器件内部存储或寄存器地址；本替身忽略该参数。
 * @param mem_address_size 存储地址宽度（字节）；本替身忽略该参数，不检查 1/2 限制。
 * @param data 接收缓冲区；本替身不读取、不写入，也不保存该指针。
 * @param size 读取字节数；本替身忽略该参数，不检查范围。
 * @param timeout 超时时间，单位 ms；本替身忽略该参数且不等待。
 * @retval PLATFORM_STATUS_OK 固定返回，未读取器件，data 内容不变。
 * @post data 内容和长度均不变，不访问 I2C 总线或器件存储。
 * @note 测试替身不会返回生产后端声明的 BUSY、TIMEOUT、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_i2c_backend_mem_read(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)i2c;
  (void)address;
  (void)mem_address;
  (void)mem_address_size;
  (void)data;
  (void)size;
  (void)timeout;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 返回测试注入的探测状态，并记录原始 trials 值。
 * @param i2c 平台 I2C 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param address 7 位从设备地址；本替身忽略该参数，不检查范围。
 * @param trials 探测尝试次数；原样写入 s_probe_trials，本替身不检查是否为零。
 * @param timeout 每次探测尝试的超时时间，单位 ms；本替身忽略该参数且不等待。
 * @return s_probe_status 的当前值；测试只注入 PLATFORM_STATUS_OK 或
 *         PLATFORM_STATUS_TIMEOUT，本替身不访问 I2C 总线。
 * @post s_probe_trials 等于 trials，s_probe_status 不变；不产生地址阶段时序。
 * @note 这是测试替身；生产后端还可能返回 BUSY、ERROR 或 INVALID_ARGUMENT。
 */
platform_status_t platform_i2c_backend_probe(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint32_t trials,
  uint32_t timeout
) {
  (void)i2c;
  (void)address;
  (void)timeout;
  s_probe_trials = trials;
  return s_probe_status;
}

/**
 * @brief 模拟启动 I2C 中断发送并固定报告成功。
 * @param i2c 平台 I2C 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param address 7 位从设备地址；本替身忽略该参数，不检查范围。
 * @param data 发送缓冲区；本替身不读取，也不保存该指针。
 * @param size 发送字节数；本替身忽略该参数，不检查范围。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动中断发送。
 * @post 不修改缓冲区、NVIC 或 I2C 状态，也不会产生完成或错误事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_i2c_backend_master_transmit_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 I2C 中断接收并固定报告成功。
 * @param i2c 平台 I2C 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param address 7 位从设备地址；本替身忽略该参数，不检查范围。
 * @param data 接收缓冲区；本替身不读取、不写入，也不保存该指针。
 * @param size 接收字节数；本替身忽略该参数，不检查范围。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动中断接收。
 * @post 不修改缓冲区、NVIC 或 I2C 状态，也不会产生完成或错误事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_i2c_backend_master_receive_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 I2C 存储器中断写并固定报告成功。
 * @param i2c 平台 I2C 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param address 7 位从设备地址；本替身忽略该参数，不检查范围。
 * @param mem_address 器件内部存储或寄存器地址；本替身忽略该参数。
 * @param mem_address_size 存储地址宽度（字节）；本替身忽略该参数，不检查 1/2 限制。
 * @param data 发送缓冲区；本替身不读取，也不保存该指针。
 * @param size 写入字节数；本替身忽略该参数，不检查范围。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动中断写。
 * @post 不修改缓冲区、NVIC 或 I2C 状态，也不会产生完成或错误事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_i2c_backend_mem_write_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)mem_address;
  (void)mem_address_size;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 I2C 存储器中断读并固定报告成功。
 * @param i2c 平台 I2C 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param address 7 位从设备地址；本替身忽略该参数，不检查范围。
 * @param mem_address 器件内部存储或寄存器地址；本替身忽略该参数。
 * @param mem_address_size 存储地址宽度（字节）；本替身忽略该参数，不检查 1/2 限制。
 * @param data 接收缓冲区；本替身不读取、不写入，也不保存该指针。
 * @param size 读取字节数；本替身忽略该参数，不检查范围。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动中断读。
 * @post 不修改缓冲区、NVIC 或 I2C 状态，也不会产生完成或错误事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_i2c_backend_mem_read_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)mem_address;
  (void)mem_address_size;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 I2C DMA 发送并固定报告成功。
 * @param i2c 平台 I2C 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param address 7 位从设备地址；本替身忽略该参数，不检查范围。
 * @param data 发送缓冲区；本替身不读取，也不保存该指针。
 * @param size 发送字节数；本替身忽略该参数，不检查范围。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动 DMA 发送。
 * @post 不修改缓冲区、DMA 控制器或 I2C 状态，也不会产生完成或错误事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_i2c_backend_master_transmit_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 I2C DMA 接收并固定报告成功。
 * @param i2c 平台 I2C 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param address 7 位从设备地址；本替身忽略该参数，不检查范围。
 * @param data 接收缓冲区；本替身不读取、不写入，也不保存该指针。
 * @param size 接收字节数；本替身忽略该参数，不检查范围。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动 DMA 接收。
 * @post 不修改缓冲区、DMA 控制器或 I2C 状态，也不会产生完成或错误事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_i2c_backend_master_receive_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 I2C 存储器 DMA 写并固定报告成功。
 * @param i2c 平台 I2C 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param address 7 位从设备地址；本替身忽略该参数，不检查范围。
 * @param mem_address 器件内部存储或寄存器地址；本替身忽略该参数。
 * @param mem_address_size 存储地址宽度（字节）；本替身忽略该参数，不检查 1/2 限制。
 * @param data 发送缓冲区；本替身不读取，也不保存该指针。
 * @param size 写入字节数；本替身忽略该参数，不检查范围。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动 DMA 写。
 * @post 不修改缓冲区、DMA 控制器或 I2C 状态，也不会产生完成或错误事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_i2c_backend_mem_write_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)mem_address;
  (void)mem_address_size;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 I2C 存储器 DMA 读并固定报告成功。
 * @param i2c 平台 I2C 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param address 7 位从设备地址；本替身忽略该参数，不检查范围。
 * @param mem_address 器件内部存储或寄存器地址；本替身忽略该参数。
 * @param mem_address_size 存储地址宽度（字节）；本替身忽略该参数，不检查 1/2 限制。
 * @param data 接收缓冲区；本替身不读取、不写入，也不保存该指针。
 * @param size 读取字节数；本替身忽略该参数，不检查范围。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动 DMA 读。
 * @post 不修改缓冲区、DMA 控制器或 I2C 状态，也不会产生完成或错误事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_i2c_backend_mem_read_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)mem_address;
  (void)mem_address_size;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟请求 I2C 中断中止并固定报告成功。
 * @param i2c 平台 I2C 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param address 7 位从设备地址；本替身忽略该参数，不检查范围。
 * @retval PLATFORM_STATUS_OK 固定返回，未中止任何真实传输。
 * @post 不修改 I2C 状态，也不会产生 ABORT_COMPLETE 事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_i2c_backend_abort_it(
  platform_i2c_instance_t i2c,
  uint16_t address
) {
  (void)i2c;
  (void)address;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 返回测试注入的 I2C 错误位。
 * @param i2c 平台 I2C 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @return s_backend_error 的当前值；测试直接写入 platform_i2c_error_t 位掩码，
 *         本替身不添加 UNKNOWN 位，也不映射 HAL 错误。
 * @post 只读取 s_backend_error，不清除错误、不改变传输状态，也不访问 I2C 寄存器。
 * @note 这是测试替身；生产后端的错误映射和非法实例处理不由本函数模拟。
 */
uint32_t platform_i2c_backend_get_error(platform_i2c_instance_t i2c) {
  (void)i2c;
  return s_backend_error;
}

/**
 * @brief 记录 I2C 完成或错误事件。
 * @param i2c 事件所属 I2C 实例；断言必须为 PLATFORM_I2C_1。
 * @param event 平台事件快照；断言非空，但函数不检查事件类型或错误值。
 * @param context 注册上下文；断言必须等于 &s_event_count。
 * @pre 通过 Platform_I2C_RegisterCallback 注册，且 context 生命周期仍有效。
 * @post s_event_count 加一；其他测试状态不变。
 * @warning 实例、事件指针或上下文不符合预期时 assert 会终止进程。
 * @note 测试由 platform_i2c_dispatch_event 同步调用；硬件路径通常来自 ISR，需保持短小。
 */
static void i2c_callback(
  platform_i2c_instance_t i2c,
  const platform_i2c_event_t *event,
  void *context
) {
  assert(i2c == PLATFORM_I2C_1);
  assert(event != NULL);
  assert(context == &s_event_count);
  s_event_count++;
}

/**
 * @brief 验证 I2C 参数校验、地址和事件分发。
 * @pre I2C 公共实现与测试替身已链接，s_transmit_status、s_probe_status、
 *      s_backend_error、s_event_count 和 s_probe_trials 可写。
 * @post 已覆盖地址越界、寄存器宽度非法、探测次数为零、超时传播、HAL 错误映射、
 *       回调重复注册和错误事件分发；结束时 s_backend_error 为 AF、s_event_count 为 1。
 * @warning 使用 assert 校验每一步；任一失败会调用 abort，进程不会正常返回。
 * @note 测试替身不访问真实 I2C 总线，也不搬运接收数据。
 */
static void test_i2c_contract(void) {
  uint8_t byte = 0U;
  s_transmit_status = PLATFORM_STATUS_OK;
  s_probe_status = PLATFORM_STATUS_OK;
  s_backend_error = PLATFORM_I2C_ERROR_NONE;
  s_event_count = 0U;
  s_probe_trials = 0U;

  assert(Platform_I2C_MasterTransmit(
           PLATFORM_I2C_1,
           0x80U,
           &byte,
           1U,
           10U
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_I2C_GetLastError(PLATFORM_I2C_1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);

  assert(Platform_I2C_MemWrite(
           PLATFORM_I2C_1,
           0x50U,
           0U,
           3U,
           &byte,
           1U,
           10U
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);

  assert(Platform_I2C_Probe(
           PLATFORM_I2C_1,
           0x80U,
           1U,
           10U
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert(Platform_I2C_Probe(
           PLATFORM_I2C_1,
           0x38U,
           0U,
           10U
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert(Platform_I2C_Probe(
           PLATFORM_I2C_1,
           0x38U,
           2U,
           10U
         ) == PLATFORM_STATUS_OK);
  assert(s_probe_trials == 2U);

  s_probe_status = PLATFORM_STATUS_TIMEOUT;
  assert(Platform_I2C_Probe(
           PLATFORM_I2C_1,
           0x3DU,
           2U,
           10U
         ) == PLATFORM_STATUS_TIMEOUT);

  s_transmit_status = PLATFORM_STATUS_ERROR;
  assert(Platform_I2C_MasterTransmit(
           PLATFORM_I2C_1,
           0x50U,
           &byte,
           1U,
           10U
         ) == PLATFORM_STATUS_ERROR);
  assert((Platform_I2C_GetLastError(PLATFORM_I2C_1) &
          PLATFORM_ERROR_HAL) != 0U);

  s_transmit_status = PLATFORM_STATUS_OK;
  assert(Platform_I2C_MasterTransmit(
           PLATFORM_I2C_1,
           0x50U,
           &byte,
           1U,
           10U
         ) == PLATFORM_STATUS_OK);

  const platform_i2c_callback_binding_t callbacks = {
    .callback = i2c_callback,
    .context = &s_event_count,
  };
  assert(Platform_I2C_RegisterCallback(PLATFORM_I2C_1, &callbacks) ==
         PLATFORM_STATUS_OK);
  assert(Platform_I2C_RegisterCallback(PLATFORM_I2C_1, &callbacks) ==
         PLATFORM_STATUS_BUSY);

  s_backend_error = PLATFORM_I2C_ERROR_AF;
  platform_i2c_dispatch_event(PLATFORM_I2C_1, PLATFORM_I2C_EVENT_ERROR);
  assert(s_event_count == 1U);
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 * @retval 0 test_i2c_contract 的全部断言通过。
 * @pre I2C 测试替身已链接且全局观察状态可写。
 * @post 测试替身状态保留；没有动态内存、线程或真实 I2C 资源。
 * @warning 断言失败会直接终止进程，不会返回非零退出码。
 */
int main(void) {
  test_i2c_contract();
  return 0;
}

/**
 * @file platform_i2c_common.c
 * @brief 与芯片系列无关的 I2C 参数校验、错误管理和回调分发。
 * @ingroup platform
 */

#include "platform_error_internal.h"
#include "platform_i2c_backend.h"
#include <stddef.h>

static volatile uint32_t i2c_error[PLATFORM_I2C_INSTANCE_COUNT];
static platform_i2c_callback_binding_t i2c_cb[PLATFORM_I2C_INSTANCE_COUNT];
static bool i2c_cb_registered[PLATFORM_I2C_INSTANCE_COUNT];

/**
 * @brief 把 HAL I2C 结果转换为公共状态并记录错误。
 * @param i2c 已通过后端校验的 I2C 实例；用于选择软件错误槽位。
 * @param status 后端返回的公共操作状态。
 * @return 原样返回 status，便于调用方直接转交。
 * @pre i2c 必须小于 PLATFORM_I2C_INSTANCE_COUNT，否则会越界访问错误数组。
 * @post status 非 OK 时，将 platform_error_from_status(status) 写入该实例错误快照；
 *       成功时不修改快照。
 * @note 内部辅助函数；不分配内存、不阻塞，快照并发限制由 platform_error_record() 决定。
 */
static platform_status_t platform_i2c_finish(
  platform_i2c_instance_t i2c,
  platform_status_t status
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(
      &i2c_error[i2c],
      platform_error_from_status(status)
    );
  }
  return status;
}

/**
 * @brief 构造 I2C 非法参数状态。
 * @param i2c 待报告的 I2C 实例；允许越界值。
 * @return 始终返回 PLATFORM_STATUS_INVALID_ARGUMENT。
 * @post i2c 位于槽位范围内时记录 PLATFORM_ERROR_INVALID_ARGUMENT；越界值不访问数组。
 * @note 内部辅助函数；不访问 I2C 硬件、不分配内存、不阻塞。
 */
static platform_status_t platform_i2c_invalid(
  platform_i2c_instance_t i2c
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_record(
      &i2c_error[i2c],
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

/**
 * @brief 校验 I2C 实例、地址、缓冲区和长度参数。
 * @param i2c 待校验的 I2C 实例；必须启用并映射到 HAL 句柄。
 * @param address 7 位器件地址，必须为 0..0x7F。
 * @param data 数据缓冲区指针；必须非 NULL，访问权限由具体传输方向决定。
 * @param size 字节数，必须大于 0。
 * @retval true 实例、地址、缓冲区和长度均满足要求。
 * @retval false 任一字段越界或 data 为 NULL。
 * @post 纯查询；不访问总线、不修改输入对象或模块状态。
 * @note 本函数不校验 mem_address_size；存储器接口另行调用 platform_i2c_mem_size_valid()。
 */
static bool platform_i2c_validate(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const void *data,
  uint16_t size
) {
  return (uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT &&
         platform_i2c_backend_is_valid(i2c) &&
         address <= 0x7FU &&
         data != NULL &&
         size != 0U;
}

/**
 * @brief 校验 I2C 寄存器地址宽度。
 * @param mem_address_size 器件内部存储地址宽度，单位字节。
 * @retval true mem_address_size 为 1 或 2。
 * @retval false 其他宽度。
 * @post 纯函数；不修改参数或模块状态。
 * @note 内部辅助函数；不访问硬件、不分配内存。
 */
static bool platform_i2c_mem_size_valid(uint8_t mem_address_size) {
  return mem_address_size == 1U || mem_address_size == 2U;
}

/**
 * @brief 把 I2C 完成、错误和中止事件分发给已注册回调。
 * @param i2c I2C 实例；非法或未注册时无操作。
 * @param type 事件类型；ERROR 事件附带后端当前错误位，其他事件错误位为 NONE。
 * @post 已注册且 callback 非 NULL 时同步调用一次；事件对象只在回调期间有效。
 * @warning 由 HAL 中断回调调用，执行上下文为中断；不得阻塞或分配内存。
 * @note 不保留事件或缓冲区指针；一个物理实例只有一个回调所有者。
 */
void platform_i2c_dispatch_event(
  platform_i2c_instance_t i2c,
  platform_i2c_event_type_t type
) {
  if (
    (uint32_t)i2c >= PLATFORM_I2C_INSTANCE_COUNT ||
    !i2c_cb_registered[i2c]
  ) {
    return;
  }

  platform_i2c_event_t event = {
    .type = type,
    .error = type == PLATFORM_I2C_EVENT_ERROR
               ? platform_i2c_backend_get_error(i2c)
               : PLATFORM_I2C_ERROR_NONE,
  };
  if (i2c_cb[i2c].callback != NULL) {
    i2c_cb[i2c].callback(i2c, &event, i2c_cb[i2c].context);
  }
}

/**
 * @brief 阻塞式主机发送。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param data 发送缓冲区；不得为 NULL，调用期间只读且至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @param timeout 等待完成或总线事件的超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 整段数据发送完成并产生停止条件。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例或总线正忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成。
 * @retval PLATFORM_STATUS_ERROR 从设备 NACK、总线错误或其他 HAL 执行失败。
 * @pre MX_I2Cx_Init() 已成功完成，且同一实例没有并发的阻塞或异步传输。
 * @post 成功时 data 内容不变；本层不保留指针，失败时总线状态由 HAL 决定。
 * @warning 阻塞期间调用任务不能继续执行，只能在任务上下文调用；data 必须覆盖整个调用。
 * @note 不分配内存，硬件会发出起始、地址、数据和停止时序。
 */
platform_status_t Platform_I2C_MasterTransmit(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (!platform_i2c_validate(i2c, address, data, size)) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_master_transmit(
      i2c,
      address,
      data,
      size,
      timeout
    )
  );
}

/**
 * @brief 阻塞式主机接收。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param data 接收缓冲区；不得为 NULL，调用期间可写且容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @param timeout 等待完成或总线事件的超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 整段数据接收完成并产生停止条件。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例或总线正忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成。
 * @retval PLATFORM_STATUS_ERROR 从设备 NACK、总线错误或其他 HAL 执行失败。
 * @pre MX_I2Cx_Init() 已成功完成，且同一实例没有并发的阻塞或异步传输。
 * @post 成功时 data 前 size 字节包含从设备数据；失败时内容可能已部分更新，不得视为有效。
 * @warning 阻塞期间调用任务不能继续执行，只能在任务上下文调用；data 必须覆盖整个调用。
 * @note 不分配内存，硬件会发出起始、地址、读取和停止时序。
 */
platform_status_t Platform_I2C_MasterReceive(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (!platform_i2c_validate(i2c, address, data, size)) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_master_receive(
      i2c,
      address,
      data,
      size,
      timeout
    )
  );
}

/**
 * @brief 阻塞式存储器写。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2。
 * @param data 发送缓冲区；不得为 NULL，调用期间只读且至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @param timeout 等待完成或总线事件的超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 内部地址和整段数据发送完成并产生停止条件。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、存储地址宽度、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例或总线正忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成。
 * @retval PLATFORM_STATUS_ERROR 从设备 NACK、总线错误或其他 HAL 执行失败。
 * @pre MX_I2Cx_Init() 已成功完成，且同一实例没有并发的阻塞或异步传输。
 * @post 成功时 data 内容不变；本层不保留指针，失败时总线状态由 HAL 决定。
 * @warning 只能在任务上下文调用；data 必须覆盖整个调用，多个任务不得并发使用同一实例。
 * @note 不分配内存；硬件先发送内部地址，再发送数据，传输序列由 HAL 实现。
 */
platform_status_t Platform_I2C_MemWrite(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (
    !platform_i2c_validate(i2c, address, data, size) ||
    !platform_i2c_mem_size_valid(mem_address_size)
  ) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_mem_write(
      i2c,
      address,
      mem_address,
      mem_address_size,
      data,
      size,
      timeout
    )
  );
}

/**
 * @brief 阻塞式存储器读。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2。
 * @param data 接收缓冲区；不得为 NULL，调用期间可写且容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @param timeout 等待完成或总线事件的超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 内部地址发送和整段数据接收完成并产生停止条件。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、存储地址宽度、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例或总线正忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成。
 * @retval PLATFORM_STATUS_ERROR 从设备 NACK、总线错误或其他 HAL 执行失败。
 * @pre MX_I2Cx_Init() 已成功完成，且同一实例没有并发的阻塞或异步传输。
 * @post 成功时 data 前 size 字节包含从设备数据；失败时内容可能已部分更新，不得视为有效。
 * @warning 只能在任务上下文调用；data 必须覆盖整个调用，多个任务不得并发使用同一实例。
 * @note 不分配内存；硬件先写内部地址，再产生重复起始条件读取数据。
 */
platform_status_t Platform_I2C_MemRead(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (
    !platform_i2c_validate(i2c, address, data, size) ||
    !platform_i2c_mem_size_valid(mem_address_size)
  ) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_mem_read(
      i2c,
      address,
      mem_address,
      mem_address_size,
      data,
      size,
      timeout
    )
  );
}

/**
 * @brief 探测 I2C 设备是否响应。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param trials 探测尝试次数，必须大于 0。
 * @param timeout 每次尝试的超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @pre MX_I2Cx_Init() 已成功完成。
 * @retval PLATFORM_STATUS_OK 设备在任一尝试中响应。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址或尝试次数非法。
 * @retval PLATFORM_STATUS_TIMEOUT 设备未在超时时间内响应。
 * @retval PLATFORM_STATUS_BUSY 总线忙。
 * @retval PLATFORM_STATUS_ERROR 探测执行失败。
 * @post 只读取地址应答结果，不读写设备寄存器，也不保留任何指针。
 * @warning 只能在任务上下文调用；同一实例不得与其他传输并发。
 * @note 不分配内存；每次尝试使用独立 timeout，具体重试顺序由 HAL 决定。
 */
platform_status_t Platform_I2C_Probe(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint32_t trials,
  uint32_t timeout
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (
    (uint32_t)i2c >= PLATFORM_I2C_INSTANCE_COUNT ||
    !platform_i2c_backend_is_valid(i2c) ||
    address > 0x7FU ||
    trials == 0U
  ) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_probe(i2c, address, trials, timeout)
  );
}

/**
 * @brief 中断方式主机发送。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param data 发送缓冲区；不得为 NULL，至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受中断发送请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例或总线正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或总线无法启动请求；本次请求不会产生完成事件。
 * @pre MX_I2Cx_Init() 已完成，NVIC 已配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受，最终结果通过完成、错误或中止事件通知。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且内容不变；回调在中断上下文执行。
 * @note 异步接口没有内部超时，不分配内存；同一实例同一时刻只能有一个 IT/DMA 传输。
 */
platform_status_t Platform_I2C_MasterTransmitIT(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (!platform_i2c_validate(i2c, address, data, size)) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_master_transmit_it(i2c, address, data, size)
  );
}

/**
 * @brief 中断方式主机接收。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param data 接收缓冲区；不得为 NULL，容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受中断接收请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例或总线正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或总线无法启动请求；本次请求不会产生完成事件。
 * @pre MX_I2Cx_Init() 已完成，NVIC 已配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成前不得读取完整 data，错误或中止时内容可能只更新一部分。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且不得并发访问；回调在中断上下文执行。
 * @note 异步接口没有内部超时，不分配内存；同一实例同一时刻只能有一个 IT/DMA 传输。
 */
platform_status_t Platform_I2C_MasterReceiveIT(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (!platform_i2c_validate(i2c, address, data, size)) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_master_receive_it(i2c, address, data, size)
  );
}

/**
 * @brief 中断方式存储器写。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2。
 * @param data 发送缓冲区；不得为 NULL，至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受中断存储写请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、存储地址宽度、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例或总线正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或总线无法启动请求；本次请求不会产生完成事件。
 * @pre MX_I2Cx_Init() 已完成，NVIC 已配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受，最终结果通过存储写完成、错误或中止事件通知。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且内容不变；回调在中断上下文执行。
 * @note 异步接口没有内部超时，不分配内存；同一实例同一时刻只能有一个 IT/DMA 传输。
 */
platform_status_t Platform_I2C_MemWriteIT(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (
    !platform_i2c_validate(i2c, address, data, size) ||
    !platform_i2c_mem_size_valid(mem_address_size)
  ) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_mem_write_it(
      i2c,
      address,
      mem_address,
      mem_address_size,
      data,
      size
    )
  );
}

/**
 * @brief 中断方式存储器读。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2。
 * @param data 接收缓冲区；不得为 NULL，容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受中断存储读请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、存储地址宽度、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例或总线正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或总线无法启动请求；本次请求不会产生完成事件。
 * @pre MX_I2Cx_Init() 已完成，NVIC 已配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成前不得读取完整 data，错误或中止时内容可能只更新一部分。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且不得并发访问；回调在中断上下文执行。
 * @note 异步接口没有内部超时，不分配内存；同一实例同一时刻只能有一个 IT/DMA 传输。
 */
platform_status_t Platform_I2C_MemReadIT(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (
    !platform_i2c_validate(i2c, address, data, size) ||
    !platform_i2c_mem_size_valid(mem_address_size)
  ) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_mem_read_it(
      i2c,
      address,
      mem_address,
      mem_address_size,
      data,
      size
    )
  );
}

/**
 * @brief DMA 方式主机发送。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param data 发送缓冲区；不得为 NULL，至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 发送请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例、总线或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求；本次请求不会产生完成事件。
 * @pre MX_I2Cx_Init() 和 CubeMX DMA 配置已完成；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受，最终结果通过完成、错误或中止事件通知。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且内容不变；回调在中断上下文执行。
 * @note DMA 传输没有内部超时，不分配内存；同一实例同一时刻只能有一个 IT/DMA 传输。
 */
platform_status_t Platform_I2C_MasterTransmitDMA(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (!platform_i2c_validate(i2c, address, data, size)) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_master_transmit_dma(i2c, address, data, size)
  );
}

/**
 * @brief DMA 方式主机接收。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param data 接收缓冲区；不得为 NULL，容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 接收请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例、总线或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求；本次请求不会产生完成事件。
 * @pre MX_I2Cx_Init() 和 CubeMX DMA 配置已完成；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成前不得读取完整 data，错误或中止时内容可能只更新一部分。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且不得并发访问；回调在中断上下文执行。
 * @note DMA 传输没有内部超时，不分配内存；同一实例同一时刻只能有一个 IT/DMA 传输。
 */
platform_status_t Platform_I2C_MasterReceiveDMA(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (!platform_i2c_validate(i2c, address, data, size)) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_master_receive_dma(i2c, address, data, size)
  );
}

/**
 * @brief DMA 方式存储器写。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2。
 * @param data 发送缓冲区；不得为 NULL，至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 存储写请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、存储地址宽度、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例、总线或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求；本次请求不会产生完成事件。
 * @pre MX_I2Cx_Init() 和对应 DMA 通道已完成配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受，最终结果通过存储写完成、错误或中止事件通知。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且内容不变；回调在中断上下文执行。
 * @note DMA 传输没有内部超时，不分配内存；同一实例同一时刻只能有一个 IT/DMA 传输。
 */
platform_status_t Platform_I2C_MemWriteDMA(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (
    !platform_i2c_validate(i2c, address, data, size) ||
    !platform_i2c_mem_size_valid(mem_address_size)
  ) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_mem_write_dma(
      i2c,
      address,
      mem_address,
      mem_address_size,
      data,
      size
    )
  );
}

/**
 * @brief DMA 方式存储器读。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2。
 * @param data 接收缓冲区；不得为 NULL，容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 存储读请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、存储地址宽度、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例、总线或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求；本次请求不会产生完成事件。
 * @pre MX_I2Cx_Init() 和对应 DMA 通道已完成配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成前不得读取完整 data，错误或中止时内容可能只更新一部分。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且不得并发访问；回调在中断上下文执行。
 * @note DMA 传输没有内部超时，不分配内存；同一实例同一时刻只能有一个 IT/DMA 传输。
 */
platform_status_t Platform_I2C_MemReadDMA(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (
    !platform_i2c_validate(i2c, address, data, size) ||
    !platform_i2c_mem_size_valid(mem_address_size)
  ) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_mem_read_dma(
      i2c,
      address,
      mem_address,
      mem_address_size,
      data,
      size
    )
  );
}

/**
 * @brief 使用中断方式中止当前主机传输。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 当前传输使用的 7 位器件地址，范围 0x00..0x7F。
 * @retval PLATFORM_STATUS_OK 中止请求已接受。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或地址非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例或总线忙。
 * @retval PLATFORM_STATUS_ERROR HAL 拒绝中止或无法启动中止序列。
 * @pre 目标实例存在进行中的异步传输，且调用发生在任务上下文。
 * @post 返回 OK 只表示中止已排队；最终结果通过 ABORT_COMPLETE 事件通知。
 * @warning 在完成、错误或中止事件处理结束前，不得复用缓冲区或启动新传输；
 *          中止完成回调在中断上下文执行。
 * @note 不分配内存、不等待硬件停止；同一实例同一时刻只能有一个 IT/DMA 传输或中止序列。
 */
platform_status_t Platform_I2C_AbortIT(
  platform_i2c_instance_t i2c,
  uint16_t address
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (
    (uint32_t)i2c >= PLATFORM_I2C_INSTANCE_COUNT ||
    !platform_i2c_backend_is_valid(i2c) ||
    address > 0x7FU
  ) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_abort_it(i2c, address)
  );
}

/**
 * @brief 读取最近一次 HAL I2C 错误快照。
 * @param i2c I2C 实例；当前仅有效枚举值可读取后端错误。
 * @retval PLATFORM_ERROR_INVALID_ARGUMENT i2c 越界。
 * @post 合法实例返回软件错误位与 HAL 错误映射的按位或；非法实例不访问快照。
 * @note 只读，不清除软件或 HAL 错误；不停止传输、不修改缓冲区，不分配内存。
 */
uint32_t Platform_I2C_GetLastError(platform_i2c_instance_t i2c) {
  if ((uint32_t)i2c >= PLATFORM_I2C_INSTANCE_COUNT) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return i2c_error[i2c] | platform_i2c_backend_get_error(i2c);
}

/**
 * @brief 注册 I2C 回调。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param callbacks 待复制的回调集合；不得为 NULL，callback 成员可为 NULL 以禁用通知。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调集合非法。
 * @pre callbacks 指向的对象在调用期间有效；每个实例仅允许成功注册一次。
 * @post 成功时复制 callback 和 context，后续完成、错误或中止事件调用该回调。
 * @warning 回调在中断上下文执行；事件对象只在回调期间有效，禁止阻塞或分配内存。
 * @note 应在初始化阶段、启动 IT/DMA 传输前注册；context 的生命周期须覆盖所有使用过程。
 */
platform_status_t Platform_I2C_RegisterCallback(
  platform_i2c_instance_t i2c,
  const platform_i2c_callback_binding_t *callbacks
) {
  if (
    (uint32_t)i2c >= PLATFORM_I2C_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_i2c_backend_is_valid(i2c)
  ) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (i2c_cb_registered[i2c]) {
    return PLATFORM_STATUS_BUSY;
  }

  i2c_cb[i2c] = *callbacks;
  i2c_cb_registered[i2c] = true;
  return PLATFORM_STATUS_OK;
}

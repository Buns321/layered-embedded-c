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
 */
static bool platform_i2c_mem_size_valid(uint8_t mem_address_size) {
  return mem_address_size == 1U || mem_address_size == 2U;
}

/**
 * @brief 把 I2C 完成、错误和中止事件分发给已注册回调。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址，范围 0x00..0x7F。
 * @param data 发送缓冲区。
 * @param size 字节数。
 * @param timeout 超时时间（ms）。
 * @pre MX_I2Cx_Init() 已成功完成。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param data 接收缓冲区。
 * @param size 字节数。
 * @param timeout 超时时间（ms）。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param mem_address 器件内部存储地址。
 * @param mem_address_size 存储地址宽度（字节），通常为 1 或 2。
 * @param data 发送缓冲区。
 * @param size 字节数。
 * @param timeout 超时时间（ms）。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param mem_address 器件内部存储地址。
 * @param mem_address_size 存储地址宽度（字节），通常为 1 或 2。
 * @param data 接收缓冲区。
 * @param size 字节数。
 * @param timeout 超时时间（ms）。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址，范围 0x00..0x7F。
 * @param trials 探测尝试次数，必须大于 0。
 * @param timeout 每次尝试的超时时间（ms）。
 * @pre MX_I2Cx_Init() 已成功完成。
 * @retval PLATFORM_STATUS_OK 设备在任一尝试中响应。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址或尝试次数非法。
 * @retval PLATFORM_STATUS_TIMEOUT 设备未在超时时间内响应。
 * @retval PLATFORM_STATUS_BUSY 总线忙。
 * @retval PLATFORM_STATUS_ERROR 探测执行失败。
 * @note 该接口不读写设备寄存器，只验证地址应答。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param data 发送缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，缓冲区必须保持有效且内容不变。完成回调
 *       在中断上下文执行。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param data 接收缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，缓冲区必须保持有效。完成回调在中断上下文
 *       执行。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param mem_address 器件内部存储地址。
 * @param mem_address_size 存储地址宽度（字节）。
 * @param data 发送缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，数据缓冲区必须保持有效。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param mem_address 器件内部存储地址。
 * @param mem_address_size 存储地址宽度（字节）。
 * @param data 接收缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，数据缓冲区必须保持有效。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param data 发送缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，缓冲区必须保持有效且内容不变。完成回调
 *       在中断上下文执行。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param data 接收缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，缓冲区必须保持有效。完成回调在中断上下文
 *       执行。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param mem_address 器件内部存储地址。
 * @param mem_address_size 存储地址宽度（字节）。
 * @param data 发送缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，数据缓冲区必须保持有效。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param mem_address 器件内部存储地址。
 * @param mem_address_size 存储地址宽度（字节）。
 * @param data 接收缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，数据缓冲区必须保持有效。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @note 中止完成回调在中断上下文执行。只有在完成或错误处理结束后，才能
 *       启动新的传输。
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
 * @param i2c I2C 实例。
 * @return platform_i2c_error_t 位掩码。
 */
uint32_t Platform_I2C_GetLastError(platform_i2c_instance_t i2c) {
  if ((uint32_t)i2c >= PLATFORM_I2C_INSTANCE_COUNT) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return i2c_error[i2c] | platform_i2c_backend_get_error(i2c);
}

/**
 * @brief 注册 I2C 回调。
 * @param i2c I2C 实例。
 * @param callbacks 待复制的回调集合。
 * @note 结构体会被复制。context 的生命周期必须覆盖所有使用过程。
 * @note 应在初始化阶段、启动 IT/DMA 传输前调用一次。一个物理实例在整个
 *       生命周期内只能有一个回调所有者。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调集合非法。
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

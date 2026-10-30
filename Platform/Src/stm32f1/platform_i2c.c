/**
 * @file platform_i2c.c
 * @brief STM32F1 公共 I2C 实现的 HAL 后端。
 * @ingroup platform
 */

#include "i2c.h"
#include "platform_hal_internal.h"
#include "platform_i2c_backend.h"
#include "stm32f1xx_hal.h"

/**
 * @brief 把平台 I2C 实例编号转换为 F1 的静态 HAL 句柄。
 * @param i2c 平台 I2C 实例编号；F1 只映射编译期启用的 I2C1 和 I2C2。
 * @return 指向 CubeMX HAL 句柄的指针，由 CubeMX 拥有；未启用或越界时返回 NULL。
 * @note 纯映射，不初始化外设、不访问总线；调用方不得释放返回指针。
 */
static I2C_HandleTypeDef *platform_i2c_to_hal(platform_i2c_instance_t i2c) {
  switch (i2c) {
#if PLATFORM_ENABLE_I2C1
    case PLATFORM_I2C_1: return &hi2c1;
#endif
#if PLATFORM_ENABLE_I2C2
    case PLATFORM_I2C_2: return &hi2c2;
#endif
    default: return NULL;
  }
}

/**
 * @brief 将 F1 HAL I2C 句柄指针识别为平台实例编号。
 * @param i2c_h HAL 回调传入的句柄；允许为 NULL，函数不取得其所有权。
 * @param i2c 调用方提供的输出参数；成功时写入实例编号，失败时保持原值不变。
 * @retval true 非空句柄匹配已启用的 I2C1 或 I2C2，且已写入 *i2c。
 * @retval false i2c_h 或 i2c 为 NULL，或句柄不对应本编译单元启用的实例。
 * @pre i2c 非 NULL 且可写；i2c_h 若为 HAL 回调参数则应由 HAL 保证在调用期间有效。
 * @post 只有成功路径修改 *i2c；函数不访问总线或修改 HAL 状态。
 * @note 纯指针识别，不分配内存，可在中断回调上下文调用。
 */
static bool platform_hal_to_i2c(
  I2C_HandleTypeDef *i2c_h,
  platform_i2c_instance_t *i2c
) {
  if (i2c_h == NULL || i2c == NULL) {
    return false;
  }
#if PLATFORM_ENABLE_I2C1
  if (i2c_h == &hi2c1) {
    *i2c = PLATFORM_I2C_1;
    return true;
  }
#endif
#if PLATFORM_ENABLE_I2C2
  if (i2c_h == &hi2c2) {
    *i2c = PLATFORM_I2C_2;
    return true;
  }
#endif
  return false;
}

/**
 * @brief 把 7 位 I2C 从设备地址转换为 HAL 的 8 位地址格式。
 * @param address 7 位从设备地址，合法范围 0x00..0x7F；本函数不检查范围。
 * @return address 左移一位后的 8 位地址形式，最低位 R/W 固定为零。
 * @warning 超出 7 位范围的输入仍会被移位并截断为 uint16_t，结果无效；公共层负责先校验。
 * @note 纯计算，不访问总线或硬件。
 */
static uint16_t platform_i2c_hal_address(uint16_t address) {
  return (uint16_t)(address << 1U);
}

/**
 * @brief 把 F1 HAL I2C 错误位映射为平台错误位。
 * @param hal_error HAL_I2C_ERROR_* 位掩码；编译期未定义的位不会被单独翻译。
 * @return PLATFORM_I2C_ERROR_* 位掩码；存在未识别的 HAL 位时额外置 UNKNOWN。
 * @note 纯位映射，不读取 HAL 句柄、不清除错误，也不改变传输状态。
 */
static uint32_t platform_i2c_map_error(uint32_t hal_error) {
  uint32_t error = PLATFORM_I2C_ERROR_NONE;
  if ((hal_error & HAL_I2C_ERROR_BERR) != 0U) {
    error |= PLATFORM_I2C_ERROR_BERR;
  }
  if ((hal_error & HAL_I2C_ERROR_ARLO) != 0U) {
    error |= PLATFORM_I2C_ERROR_ARLO;
  }
  if ((hal_error & HAL_I2C_ERROR_AF) != 0U) {
    error |= PLATFORM_I2C_ERROR_AF;
  }
  if ((hal_error & HAL_I2C_ERROR_OVR) != 0U) {
    error |= PLATFORM_I2C_ERROR_OVR;
  }
  if ((hal_error & HAL_I2C_ERROR_DMA) != 0U) {
    error |= PLATFORM_I2C_ERROR_DMA;
  }
  if ((hal_error & HAL_I2C_ERROR_TIMEOUT) != 0U) {
    error |= PLATFORM_I2C_ERROR_TIMEOUT;
  }
#ifdef HAL_I2C_ERROR_SIZE
  if ((hal_error & HAL_I2C_ERROR_SIZE) != 0U) {
    error |= PLATFORM_I2C_ERROR_SIZE;
  }
#endif
#ifdef HAL_I2C_ERROR_DMA_PARAM
  if ((hal_error & HAL_I2C_ERROR_DMA_PARAM) != 0U) {
    error |= PLATFORM_I2C_ERROR_DMA_PARAM;
  }
#endif

  uint32_t known_errors =
    HAL_I2C_ERROR_BERR |
    HAL_I2C_ERROR_ARLO |
    HAL_I2C_ERROR_AF |
    HAL_I2C_ERROR_OVR |
    HAL_I2C_ERROR_DMA |
    HAL_I2C_ERROR_TIMEOUT;
#ifdef HAL_I2C_ERROR_SIZE
  known_errors |= HAL_I2C_ERROR_SIZE;
#endif
#ifdef HAL_I2C_ERROR_DMA_PARAM
  known_errors |= HAL_I2C_ERROR_DMA_PARAM;
#endif
  if ((hal_error & ~known_errors) != 0U) {
    error |= PLATFORM_I2C_ERROR_UNKNOWN;
  }
  return error;
}

/**
 * @brief 判断 I2C 实例身份是否已启用且可映射到 HAL 句柄。
 * @param i2c 平台 I2C 实例编号；有效范围由实例总数和板级启用宏共同决定。
 * @retval true i2c 位于有效槽位且当前系列存在对应 HAL 句柄。
 * @retval false i2c 越界、未启用或本系列没有对应句柄。
 * @note 纯查询，不访问总线、不启动传输或修改错误快照；可在任务或中断上下文调用。
 */
bool platform_i2c_backend_is_valid(platform_i2c_instance_t i2c) {
  return (uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT &&
         platform_i2c_to_hal(i2c) != NULL;
}

/**
 * @brief 以阻塞方式向 7 位 I2C 从设备发送一段字节。
 * @param i2c 已启用的平台实例；本后端将其映射到静态 HAL 句柄。
 * @param address 7 位从设备地址，范围 0x00..0x7F；内部左移一位后交给 HAL。
 * @param data 发送缓冲区，由调用方拥有；函数只读 size 字节且不保留指针。
 * @param size 发送字节数，范围 1..UINT16_MAX；公共层拒绝零长度。
 * @param timeout 等待完成或总线事件的超时时间（ms）；PLATFORM_TIMEOUT_INFINITE 表示交由 HAL 无限等待。
 * @retval PLATFORM_STATUS_OK 整段数据发送完成并产生停止条件。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT i2c 无法映射到已启用 HAL 句柄；其他参数由公共层预检。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例或总线忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成。
 * @retval PLATFORM_STATUS_ERROR 从设备 NACK、总线错误或其他 HAL 执行失败。
 * @pre 公共 I2C 层已校验实例、地址、缓冲区和长度，且调用发生在任务上下文。
 * @post data 内容不变；本后端不缓存指针，公共层负责记录返回状态对应的错误快照。
 * @warning 阻塞期间调用任务不能继续执行，不得在中断上下文调用；同一实例同一时刻只能有一个传输。
 * @note 不分配内存，超时完全由 timeout 控制；硬件会发出起始、地址、数据和停止时序。
 */
platform_status_t platform_i2c_backend_master_transmit(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Master_Transmit(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       (uint8_t *)data,
                       size,
                       timeout
                     ));
}

/**
 * @brief 以阻塞方式从 7 位 I2C 从设备接收一段字节。
 * @param i2c 已启用的平台实例；本后端将其映射到静态 HAL 句柄。
 * @param address 7 位从设备地址，范围 0x00..0x7F；内部左移一位后交给 HAL。
 * @param data 接收缓冲区，由调用方拥有；成功时至少写入 size 字节且不保留指针。
 * @param size 接收字节数，范围 1..UINT16_MAX；公共层拒绝零长度。
 * @param timeout 等待完成或总线事件的超时时间（ms）；PLATFORM_TIMEOUT_INFINITE 表示交由 HAL 无限等待。
 * @retval PLATFORM_STATUS_OK 整段数据接收完成并产生停止条件。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT i2c 无法映射到已启用 HAL 句柄；其他参数由公共层预检。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例或总线忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成。
 * @retval PLATFORM_STATUS_ERROR 从设备 NACK、总线错误或其他 HAL 执行失败。
 * @pre 公共 I2C 层已校验实例、地址、缓冲区和长度，且调用发生在任务上下文。
 * @post 成功时 data 前 size 字节包含从设备数据；失败时内容可能已部分更新，调用方不得视为有效。
 * @warning 阻塞期间调用任务不能继续执行，不得在中断上下文调用；同一实例同一时刻只能有一个传输。
 * @note 不分配内存，超时完全由 timeout 控制；硬件会发出起始、地址、读取和停止时序。
 */
platform_status_t platform_i2c_backend_master_receive(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Master_Receive(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       data,
                       size,
                       timeout
                     ));
}

/**
 * @brief 以阻塞方式先发送器件内部存储地址，再向该位置写入一段字节。
 * @param i2c 已启用的平台实例；本后端将其映射到静态 HAL 句柄。
 * @param address 7 位从设备地址，范围 0x00..0x7F；内部左移一位后交给 HAL。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址，宽度由 mem_address_size 决定。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2；公共层负责校验。
 * @param data 发送缓冲区，由调用方拥有；函数只读 size 字节且不保留指针。
 * @param size 写入字节数，范围 1..UINT16_MAX；公共层拒绝零长度。
 * @param timeout 等待完成或总线事件的超时时间（ms）；PLATFORM_TIMEOUT_INFINITE 表示交由 HAL 无限等待。
 * @retval PLATFORM_STATUS_OK 存储地址和数据均已写入并产生停止条件。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT i2c 无法映射到已启用 HAL 句柄；其他参数由公共层预检。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例或总线忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成。
 * @retval PLATFORM_STATUS_ERROR 从设备 NACK、总线错误或其他 HAL 执行失败。
 * @pre 公共 I2C 层已校验实例、地址、存储地址宽度、缓冲区和长度，且调用发生在任务上下文。
 * @post data 内容不变；器件可能已接收部分数据，失败后的重试策略由设备层负责。
 * @warning 阻塞期间调用任务不能继续执行，不得在中断上下文调用；同一实例同一时刻只能有一个传输。
 * @note 不分配内存；HAL 按 mem_address_size 发出内部地址字节，然后发送数据字节。
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
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Mem_Write(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       mem_address,
                       mem_address_size,
                       (uint8_t *)data,
                       size,
                       timeout
                     ));
}

/**
 * @brief 以阻塞方式先发送器件内部存储地址，再从该位置读取一段字节。
 * @param i2c 已启用的平台实例；本后端将其映射到静态 HAL 句柄。
 * @param address 7 位从设备地址，范围 0x00..0x7F；内部左移一位后交给 HAL。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址，宽度由 mem_address_size 决定。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2；公共层负责校验。
 * @param data 接收缓冲区，由调用方拥有；成功时至少写入 size 字节且不保留指针。
 * @param size 读取字节数，范围 1..UINT16_MAX；公共层拒绝零长度。
 * @param timeout 等待完成或总线事件的超时时间（ms）；PLATFORM_TIMEOUT_INFINITE 表示交由 HAL 无限等待。
 * @retval PLATFORM_STATUS_OK 已发送存储地址并完整读取数据、产生停止条件。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT i2c 无法映射到已启用 HAL 句柄；其他参数由公共层预检。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例或总线忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成。
 * @retval PLATFORM_STATUS_ERROR 从设备 NACK、总线错误或其他 HAL 执行失败。
 * @pre 公共 I2C 层已校验实例、地址、存储地址宽度、缓冲区和长度，且调用发生在任务上下文。
 * @post 成功时 data 前 size 字节包含目标存储区数据；失败时内容可能已部分更新，不得视为有效。
 * @warning 阻塞期间调用任务不能继续执行，不得在中断上下文调用；同一实例同一时刻只能有一个传输。
 * @note 不分配内存；HAL 以写入阶段发送内部地址，再以重复起始或停止后的读取阶段获取数据。
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
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Mem_Read(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       mem_address,
                       mem_address_size,
                       data,
                       size,
                       timeout
                     ));
}

/**
 * @brief 通过地址探测确认 7 位从设备是否应答，不访问器件寄存器。
 * @param i2c 已启用的平台实例；本后端将其映射到静态 HAL 句柄。
 * @param address 7 位从设备地址，范围 0x00..0x7F；内部左移一位后交给 HAL。
 * @param trials 探测尝试次数；本后端不校验，0 或极大值原样交给 HAL，调用方应传大于零的值。
 * @param timeout 每次探测尝试等待应答的超时时间（ms）；PLATFORM_TIMEOUT_INFINITE 表示交由 HAL 无限等待。
 * @retval PLATFORM_STATUS_OK 至少一次探测收到从设备 ACK。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT i2c 无法映射到已启用 HAL 句柄；其他参数由公共层预检。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例或总线忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在给定超时内没有收到可判定的应答。
 * @retval PLATFORM_STATUS_ERROR 所有尝试均以 NACK、总线错误或其他 HAL 失败结束。
 * @pre 公共 I2C 层已校验实例和地址，且调用发生在任务上下文。
 * @post 不读写数据缓冲区，也不保留指针；探测只产生地址阶段的总线时序。
 * @warning 阻塞期间调用任务不能继续执行，不得在中断上下文调用；同一实例同一时刻只能有一个传输。
 * @note 不分配内存；trials 和 timeout 的逐次语义由 HAL_I2C_IsDeviceReady() 实现决定。
 */
platform_status_t platform_i2c_backend_probe(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint32_t trials,
  uint32_t timeout
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_IsDeviceReady(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       trials,
                       timeout
                     ));
}

/**
 * @brief 启动中断方式的主机发送；返回成功只表示 HAL 已接受请求。
 * @param i2c 已启用的平台实例；本后端将其映射到静态 HAL 句柄。
 * @param address 7 位从设备地址，范围 0x00..0x7F；内部左移一位后交给 HAL。
 * @param data 发送缓冲区，由调用方拥有；HAL 在最终完成、错误或中止事件前保留该指针。
 * @param size 发送字节数，范围 1..UINT16_MAX；公共层拒绝零长度。
 * @retval PLATFORM_STATUS_OK 请求已被 HAL 接受，传输在中断上下文继续。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT i2c 无法映射到已启用 HAL 句柄；其他参数由公共层预检。
 * @retval PLATFORM_STATUS_BUSY 该实例已有活动 IT/DMA 传输或总线忙。
 * @retval PLATFORM_STATUS_ERROR HAL 无法启动传输；本次请求不会产生完成事件。
 * @pre 公共 I2C 层已校验参数，且调用发生在任务上下文；回调所有者应在启动前注册。
 * @post HAL 保留 data 指针且不复制数据；完成、错误和中止事件由平台事件分发唯一终结本次请求。
 * @warning data 必须保持有效直到最终事件，期间修改内容会导致发送数据不确定；回调在 ISR 执行，不得阻塞。
 * @note IT 传输没有内部超时；上层若需超时必须自行跟踪并显式调用中止。
 */
platform_status_t platform_i2c_backend_master_transmit_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Master_Transmit_IT(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       (uint8_t *)data,
                       size
                     ));
}

/**
 * @brief 启动中断方式的主机接收；返回成功只表示 HAL 已接受请求。
 * @param i2c 已启用的平台实例；本后端将其映射到静态 HAL 句柄。
 * @param address 7 位从设备地址，范围 0x00..0x7F；内部左移一位后交给 HAL。
 * @param data 接收缓冲区，由调用方拥有；HAL 在最终完成、错误或中止事件前保留该指针。
 * @param size 接收字节数，范围 1..UINT16_MAX；公共层拒绝零长度。
 * @retval PLATFORM_STATUS_OK 请求已被 HAL 接受，传输在中断上下文继续。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT i2c 无法映射到已启用 HAL 句柄；其他参数由公共层预检。
 * @retval PLATFORM_STATUS_BUSY 该实例已有活动 IT/DMA 传输或总线忙。
 * @retval PLATFORM_STATUS_ERROR HAL 无法启动传输；本次请求不会产生完成事件。
 * @pre 公共 I2C 层已校验参数，且调用发生在任务上下文；回调所有者应在启动前注册。
 * @post HAL 保留 data 指针且不复制目标缓冲区；最终事件前不得读取完整结果。
 * @warning data 必须保持有效且不得并发访问直到最终事件；错误中止时内容可能只更新一部分。
 * @note IT 传输没有内部超时；上层若需超时必须自行跟踪并显式调用中止。
 */
platform_status_t platform_i2c_backend_master_receive_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Master_Receive_IT(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       data,
                       size
                     ));
}

/**
 * @brief 启动中断方式的器件存储地址写入；返回成功只表示 HAL 已接受请求。
 * @param i2c 已启用的平台实例；本后端将其映射到静态 HAL 句柄。
 * @param address 7 位从设备地址，范围 0x00..0x7F；内部左移一位后交给 HAL。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址，宽度由 mem_address_size 决定。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2；公共层负责校验。
 * @param data 发送缓冲区，由调用方拥有；HAL 在最终事件前保留该指针。
 * @param size 写入字节数，范围 1..UINT16_MAX；公共层拒绝零长度。
 * @retval PLATFORM_STATUS_OK 请求已被 HAL 接受，传输在中断上下文继续。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT i2c 无法映射到已启用 HAL 句柄；其他参数由公共层预检。
 * @retval PLATFORM_STATUS_BUSY 该实例已有活动 IT/DMA 传输或总线忙。
 * @retval PLATFORM_STATUS_ERROR HAL 无法启动传输；本次请求不会产生完成事件。
 * @pre 公共 I2C 层已校验参数，且调用发生在任务上下文；回调所有者应在启动前注册。
 * @post HAL 保留 data 指针且不复制数据；完成、错误和中止事件由平台事件分发唯一终结本次请求。
 * @warning data 必须保持有效直到最终事件，期间不得修改；回调在 ISR 执行，不得阻塞。
 * @note IT 传输没有内部超时；内部地址和数据按 HAL 的存储传输序列发送。
 */
platform_status_t platform_i2c_backend_mem_write_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Mem_Write_IT(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       mem_address,
                       mem_address_size,
                       (uint8_t *)data,
                       size
                     ));
}

/**
 * @brief 启动中断方式的器件存储地址读取；返回成功只表示 HAL 已接受请求。
 * @param i2c 已启用的平台实例；本后端将其映射到静态 HAL 句柄。
 * @param address 7 位从设备地址，范围 0x00..0x7F；内部左移一位后交给 HAL。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址，宽度由 mem_address_size 决定。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2；公共层负责校验。
 * @param data 接收缓冲区，由调用方拥有；HAL 在最终事件前保留该指针。
 * @param size 读取字节数，范围 1..UINT16_MAX；公共层拒绝零长度。
 * @retval PLATFORM_STATUS_OK 请求已被 HAL 接受，传输在中断上下文继续。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT i2c 无法映射到已启用 HAL 句柄；其他参数由公共层预检。
 * @retval PLATFORM_STATUS_BUSY 该实例已有活动 IT/DMA 传输或总线忙。
 * @retval PLATFORM_STATUS_ERROR HAL 无法启动传输；本次请求不会产生完成事件。
 * @pre 公共 I2C 层已校验参数，且调用发生在任务上下文；回调所有者应在启动前注册。
 * @post HAL 保留 data 指针且不复制目标缓冲区；最终事件前不得读取完整结果。
 * @warning data 必须保持有效且不得并发访问直到最终事件；错误中止时内容可能只更新一部分。
 * @note IT 传输没有内部超时；内部地址和数据按 HAL 的存储传输序列处理。
 */
platform_status_t platform_i2c_backend_mem_read_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Mem_Read_IT(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       mem_address,
                       mem_address_size,
                       data,
                       size
                     ));
}

/**
 * @brief 启动 DMA 方式的主机发送；返回成功只表示 HAL 已接受请求。
 * @param i2c 已启用的平台实例；本后端将其映射到静态 HAL 句柄。
 * @param address 7 位从设备地址，范围 0x00..0x7F；内部左移一位后交给 HAL。
 * @param data 发送缓冲区，由调用方拥有；DMA 在最终完成、错误或中止事件前持续读取该内存。
 * @param size 发送字节数，范围 1..UINT16_MAX；公共层拒绝零长度。
 * @retval PLATFORM_STATUS_OK 请求已被 HAL 接受，DMA 传输已启动。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT i2c 无法映射到已启用 HAL 句柄；其他参数由公共层预检。
 * @retval PLATFORM_STATUS_BUSY 该实例已有活动 IT/DMA 传输或总线忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求；本次请求不会产生完成事件。
 * @pre 公共 I2C 层已校验参数，且调用发生在任务上下文；DMA 通道和 NVIC 已由 CubeMX 配置。
 * @post HAL 保留 data 指针且不复制数据；完成、错误和中止事件由平台事件分发唯一终结本次请求。
 * @warning data 必须在最终事件前保持有效且内容不变；回调在 ISR 执行，不得阻塞或释放缓冲区。
 * @note DMA 传输没有内部超时；上层若需超时必须自行跟踪并显式中止。
 */
platform_status_t platform_i2c_backend_master_transmit_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Master_Transmit_DMA(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       (uint8_t *)data,
                       size
                     ));
}

/**
 * @brief 启动 DMA 方式的主机接收；返回成功只表示 HAL 已接受请求。
 * @param i2c 已启用的平台实例；本后端将其映射到静态 HAL 句柄。
 * @param address 7 位从设备地址，范围 0x00..0x7F；内部左移一位后交给 HAL。
 * @param data 接收缓冲区，由调用方拥有；DMA 在最终事件前持续写入该内存。
 * @param size 接收字节数，范围 1..UINT16_MAX；公共层拒绝零长度。
 * @retval PLATFORM_STATUS_OK 请求已被 HAL 接受，DMA 传输已启动。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT i2c 无法映射到已启用 HAL 句柄；其他参数由公共层预检。
 * @retval PLATFORM_STATUS_BUSY 该实例已有活动 IT/DMA 传输或总线忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求；本次请求不会产生完成事件。
 * @pre 公共 I2C 层已校验参数，且调用发生在任务上下文；DMA 通道和 NVIC 已由 CubeMX 配置。
 * @post HAL 保留 data 指针且不复制结果；最终事件前不得读取完整缓冲区。
 * @warning data 必须在最终事件前保持有效且不得并发访问；错误中止时内容可能只更新一部分。
 * @note DMA 传输没有内部超时；上层若需超时必须自行跟踪并显式中止。
 */
platform_status_t platform_i2c_backend_master_receive_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Master_Receive_DMA(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       data,
                       size
                     ));
}

/**
 * @brief 启动 DMA 方式的器件存储地址写入；返回成功只表示 HAL 已接受请求。
 * @param i2c 已启用的平台实例；本后端将其映射到静态 HAL 句柄。
 * @param address 7 位从设备地址，范围 0x00..0x7F；内部左移一位后交给 HAL。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址，宽度由 mem_address_size 决定。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2；公共层负责校验。
 * @param data 发送缓冲区，由调用方拥有；DMA 在最终事件前持续读取该内存。
 * @param size 写入字节数，范围 1..UINT16_MAX；公共层拒绝零长度。
 * @retval PLATFORM_STATUS_OK 请求已被 HAL 接受，DMA 传输已启动。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT i2c 无法映射到已启用 HAL 句柄；其他参数由公共层预检。
 * @retval PLATFORM_STATUS_BUSY 该实例已有活动 IT/DMA 传输或总线忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求；本次请求不会产生完成事件。
 * @pre 公共 I2C 层已校验参数，且调用发生在任务上下文；DMA 通道和 NVIC 已由 CubeMX 配置。
 * @post HAL 保留 data 指针且不复制数据；完成、错误和中止事件由平台事件分发唯一终结本次请求。
 * @warning data 必须在最终事件前保持有效且内容不变；回调在 ISR 执行，不得阻塞或释放缓冲区。
 * @note DMA 传输没有内部超时；内部地址和数据按 HAL 的存储传输序列发送。
 */
platform_status_t platform_i2c_backend_mem_write_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Mem_Write_DMA(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       mem_address,
                       mem_address_size,
                       (uint8_t *)data,
                       size
                     ));
}

/**
 * @brief 启动 DMA 方式的器件存储地址读取；返回成功只表示 HAL 已接受请求。
 * @param i2c 已启用的平台实例；本后端将其映射到静态 HAL 句柄。
 * @param address 7 位从设备地址，范围 0x00..0x7F；内部左移一位后交给 HAL。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址，宽度由 mem_address_size 决定。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2；公共层负责校验。
 * @param data 接收缓冲区，由调用方拥有；DMA 在最终事件前持续写入该内存。
 * @param size 读取字节数，范围 1..UINT16_MAX；公共层拒绝零长度。
 * @retval PLATFORM_STATUS_OK 请求已被 HAL 接受，DMA 传输已启动。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT i2c 无法映射到已启用 HAL 句柄；其他参数由公共层预检。
 * @retval PLATFORM_STATUS_BUSY 该实例已有活动 IT/DMA 传输或总线忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求；本次请求不会产生完成事件。
 * @pre 公共 I2C 层已校验参数，且调用发生在任务上下文；DMA 通道和 NVIC 已由 CubeMX 配置。
 * @post HAL 保留 data 指针且不复制结果；最终事件前不得读取完整缓冲区。
 * @warning data 必须在最终事件前保持有效且不得并发访问；错误中止时内容可能只更新一部分。
 * @note DMA 传输没有内部超时；内部地址和数据按 HAL 的存储传输序列处理。
 */
platform_status_t platform_i2c_backend_mem_read_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Mem_Read_DMA(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       mem_address,
                       mem_address_size,
                       data,
                       size
                     ));
}

/**
 * @brief 请求以中断方式中止当前 I2C 主机传输。
 * @param i2c 已启用的平台实例；本后端将其映射到静态 HAL 句柄。
 * @param address 当前传输使用的 7 位从设备地址，范围 0x00..0x7F；内部左移一位后交给 HAL。
 * @retval PLATFORM_STATUS_OK 中止请求已接受；最终结果通过 ABORT_COMPLETE 事件通知。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT i2c 无法映射到已启用 HAL 句柄；其他参数由公共层预检。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例或总线忙。
 * @retval PLATFORM_STATUS_ERROR HAL 拒绝中止或无法启动中止序列。
 * @pre 公共 I2C 层已校验参数，且调用发生在任务上下文；目标实例存在进行中的异步传输。
 * @post 返回成功只表示中止已排队；不得在最终事件前复用缓冲区或启动新传输。
 * @warning 完成回调在中断上下文执行，不得阻塞、分配内存或执行长耗时解析。
 * @note 不复制缓冲区、不分配内存；调用不会同步等待硬件停止。
 */
platform_status_t platform_i2c_backend_abort_it(
  platform_i2c_instance_t i2c,
  uint16_t address
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Master_Abort_IT(
                       i2c_h,
                       platform_i2c_hal_address(address)
                     ));
}

/**
 * @brief 返回指定 I2C 实例的最新 HAL 错误位掩码。
 * @param i2c 平台 I2C 实例编号；有效实例映射到静态 HAL 句柄。
 * @return PLATFORM_I2C_ERROR_* 位掩码：有效且已映射实例由 HAL_I2C_GetError() 映射；
 *         无效或未映射实例返回 PLATFORM_I2C_ERROR_UNKNOWN。
 * @note 只读，不清除 HAL 错误状态；多个错误位可同时存在，未知 HAL 位映射为 UNKNOWN。
 * @note 未与传输事件同步；应在完成、错误或中止事件后读取，避免与中断并发解释同一 HAL 字段。
 */
uint32_t platform_i2c_backend_get_error(platform_i2c_instance_t i2c) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_I2C_ERROR_UNKNOWN
                   : platform_i2c_map_error(HAL_I2C_GetError(i2c_h));
}

/**
 * @brief 把 HAL I2C 回调句柄转换为实例并分发对应平台事件。
 * @param i2c_h HAL 回调传入的句柄；允许为 NULL，函数不取得其所有权。
 * @param type 要分发的平台 I2C 事件类型；应为有效枚举值，本函数不另行校验。
 * @post 句柄识别成功时调用 platform_i2c_dispatch_event()；NULL 或未启用句柄被静默忽略。
 * @warning 该私有函数位于 HAL 回调路径，可能在中断上下文执行；不得阻塞、分配内存或执行长耗时操作。
 */
static void platform_i2c_dispatch_hal_event(
  I2C_HandleTypeDef *i2c_h,
  platform_i2c_event_type_t type
) {
  platform_i2c_instance_t i2c;
  if (platform_hal_to_i2c(i2c_h, &i2c)) {
    platform_i2c_dispatch_event(i2c, type);
  }
}

/**
 * @brief 将 HAL I2C 主机发送完成回调转发为平台完成事件。
 * @param i2c_h HAL 传入的静态句柄；允许为 NULL，无法识别或未启用的句柄会被忽略。
 * @pre 由 HAL 在中断上下文调用，对应一次已启动的主机 IT/DMA 发送完成。
 * @post 识别成功时向该实例已注册回调转发 MASTER_TX_COMPLETE；不复制或释放发送缓冲区。
 * @note 仅做事件转发，不得在该路径阻塞、分配内存或执行长耗时解析。
 */
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(
    i2c_h,
    PLATFORM_I2C_EVENT_MASTER_TX_COMPLETE
  );
}

/**
 * @brief 将 HAL I2C 主机接收完成回调转发为平台完成事件。
 * @param i2c_h HAL 传入的静态句柄；允许为 NULL，无法识别或未启用的句柄会被忽略。
 * @pre 由 HAL 在中断上下文调用，对应一次已启动的主机 IT/DMA 接收完成。
 * @post 识别成功时向该实例已注册回调转发 MASTER_RX_COMPLETE；不复制或释放接收缓冲区。
 * @note 仅做事件转发，不得在该路径阻塞、分配内存或执行长耗时解析。
 */
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(
    i2c_h,
    PLATFORM_I2C_EVENT_MASTER_RX_COMPLETE
  );
}

/**
 * @brief 将 HAL I2C 器件存储地址写入完成回调转发为平台完成事件。
 * @param i2c_h HAL 传入的静态句柄；允许为 NULL，无法识别或未启用的句柄会被忽略。
 * @pre 由 HAL 在中断上下文调用，对应一次已启动的存储器 IT/DMA 写入完成。
 * @post 识别成功时向该实例已注册回调转发 MEM_TX_COMPLETE；不复制或释放发送缓冲区。
 * @note 仅做事件转发，不得在该路径阻塞、分配内存或执行长耗时解析。
 */
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(i2c_h, PLATFORM_I2C_EVENT_MEM_TX_COMPLETE);
}

/**
 * @brief 将 HAL I2C 器件存储地址读取完成回调转发为平台完成事件。
 * @param i2c_h HAL 传入的静态句柄；允许为 NULL，无法识别或未启用的句柄会被忽略。
 * @pre 由 HAL 在中断上下文调用，对应一次已启动的存储器 IT/DMA 读取完成。
 * @post 识别成功时向该实例已注册回调转发 MEM_RX_COMPLETE；不复制或释放接收缓冲区。
 * @note 仅做事件转发，不得在该路径阻塞、分配内存或执行长耗时解析。
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(i2c_h, PLATFORM_I2C_EVENT_MEM_RX_COMPLETE);
}

/**
 * @brief 将 HAL I2C 错误回调转发为平台错误事件。
 * @param i2c_h HAL 传入的静态句柄；允许为 NULL，无法识别或未启用的句柄会被忽略。
 * @pre 由 HAL 在中断上下文调用；对应传输已进入错误终态，HAL 错误寄存器可由 get_error 读取。
 * @post 识别成功时向该实例已注册回调转发 ERROR，event.error 携带最新后端错误位。
 * @warning 错误快照只读且不清除；回调不得在该路径阻塞、分配内存或执行长耗时解析。
 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(i2c_h, PLATFORM_I2C_EVENT_ERROR);
}

/**
 * @brief 将 HAL I2C 中止完成回调转发为平台终止事件。
 * @param i2c_h HAL 传入的静态句柄；允许为 NULL，无法识别或未启用的句柄会被忽略。
 * @pre 由 HAL 在中断上下文调用，对应一次已接受的异步中止操作完成。
 * @post 识别成功时向该实例已注册回调转发 ABORT_COMPLETE；调用方此后可显式恢复传输。
 * @note 仅做事件转发，不得在该路径阻塞、分配内存或执行长耗时解析。
 */
void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(i2c_h, PLATFORM_I2C_EVENT_ABORT_COMPLETE);
}

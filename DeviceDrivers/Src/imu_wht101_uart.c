//
// 由 Buns_ 创建于 2026/9/1。
//

/**
 * @file imu_wht101_uart.c
 * @brief WHT101 UART IMU 驱动实现。
 * @ingroup drivers_concrete
 */

#include "imu_wht101_uart.h"
#include <assert.h>
#include <stdint.h>

/**
 * @brief 注册 UART 回调并启动 WHT101 单字节中断接收。
 * @param imu_h base 视图；必须指向 imu_wht101_uart_t 的首成员且已 Init。
 * @pre imu_h 非空且尚未启动；UART 后端无其他回调所有者；在任务上下文调用。
 * @post 回调写入 UART 后端，is_started 置 true，并尝试挂接一字节接收。
 * @note 由 imu_base 的 Start 路径调用；本函数不推进配置状态机。
 */
static void imu_wht101_start(imu_base_t *imu_h);
/**
 * @brief 排空接收环形队列并推进解析与配置状态机。
 * @param imu_h base 视图；必须指向已启动的具体对象。
 * @param now_ms 当前单调毫秒时间戳；供解析帧和配置超时使用。
 * @pre 仅由单一传感器任务按周期调用；不得与 FeedByte 或 SetZero/SetOutputRate 并发。
 * @post 队列字节全部消费，可能发布新通道、推进配置事务或记录错误计数。
 * @note 非阻塞；队列为空时仍会推进配置状态机的超时判断。
 */
static void imu_wht101_process(imu_base_t *imu_h, uint32_t now_ms);
/**
 * @brief UART 接收完成中断回调：单字节入队并重新挂接。
 * @param uart_h 触发事件的 UART 后端；必须等于对象保存的 uart_h。
 * @param context 注册时传入的 imu_wht101_uart_t 指针。
 * @pre 回调和对象生命周期覆盖注册期；只在中断上下文执行。
 * @post rx_byte_count 递增；队列未满时写入并推进 rx_write，满时递增 rx_overflow_count 并请求重同步。
 * @note 不在 ISR 中解析帧；随后尝试重新挂接接收。
 * @warning 重挂失败会触发 assert；队列溢出丢弃当前字节。
 */
static void imu_wht101_rx_complete_cb(uart_base_t *uart_h, void *context);
/**
 * @brief UART 错误中断回调：计数、请求重同步并重挂接收。
 * @param uart_h 触发错误的 UART 后端；必须等于对象保存的 uart_h。
 * @param context 注册时传入的 imu_wht101_uart_t 指针。
 * @pre 回调和对象生命周期覆盖注册期；只在中断上下文执行。
 * @post rx_error_count 递增，rx_resync_requested 置 true，并尝试重新挂接接收。
 * @note 解析器在任务侧消费 rx_resync_requested 后回到帧头同步状态。
 * @warning 重挂失败会触发 assert；错误事件本身不携带更多诊断信息。
 */
static void imu_wht101_uart_err_cb(uart_base_t *uart_h, void *context);

/**
 * @brief 通过 base 操作表复制 WHT101 设备侧状态。
 * @param imu_h base 视图；必须指向 imu_wht101_uart_t 的首成员。
 * @param status 输出缓冲区；调用方拥有，函数完整覆盖已声明字段。
 * @pre 两个指针非空；经 ops 表调用时传入的必须是具体对象首地址。
 * @post status 的设备字段为启动标志、配置忙碌状态、失败标志和错误计数总和。
 * @note valid_mask、sample_count 等管线字段由 imu_base 在 Dev_IMU_Base_GetStatus 中补填，本函数不写。
 * @note 错误计数以 64 位求和后饱和到 UINT32_MAX；不在 ISR 中调用。
 */
static void imu_wht101_get_status(
  imu_base_t *imu_h,
  imu_status_t *status
) {
  assert(imu_h && status);
  imu_wht101_uart_t *imu_wht101_uart_h = (imu_wht101_uart_t *)imu_h;

  uint64_t error_count =
    (uint64_t)imu_wht101_uart_h->rx_error_count +
    (uint64_t)imu_wht101_uart_h->rx_overflow_count +
    (uint64_t)imu_wht101_uart_h->rx_checksum_error_count;

  if (error_count > UINT32_MAX) {
    error_count = UINT32_MAX;
  }

  *status = (imu_status_t){
    .is_started      = imu_wht101_uart_h->is_started,
    .is_busy         =
      imu_wht101_uart_h->config_state != IMU_WHT101_CONFIG_IDLE ||
      imu_wht101_uart_h->zero_pending ||
      imu_wht101_uart_h->rate_pending,
    .last_request_failed = imu_wht101_uart_h->config_failed,
    .error_count     = (uint32_t)error_count,
  };
}

/**
 * @brief 设备侧置零操作表入口，转发到公开的 SetZero。
 * @param imu_h base 视图；必须指向已启动的 imu_wht101_uart_t。
 * @pre imu_h 非空且 is_started 为 true；由单一任务经 Dev_IMU_Base_SetZero 调用。
 * @post 与 Dev_IMU_WHT101_UART_SetZero 相同：置位 zero_pending 并清除 config_failed。
 * @note 只发起请求，不等待 UART 写入完成；不得在 ISR 中调用。
 */
static void imu_wht101_set_zero(imu_base_t *imu_h) {
  imu_wht101_uart_t *imu_wht101_uart_h = (imu_wht101_uart_t*)imu_h;
  Dev_IMU_WHT101_UART_SetZero(imu_wht101_uart_h);
}

static const imu_ops_t imu_ops_wht101 = {
  .start      = imu_wht101_start,
  .process    = imu_wht101_process,
  .get_status = imu_wht101_get_status,
  .set_zero   = imu_wht101_set_zero,
};

/******************************************************************************/

/**
 * @brief 按设备协议发送一条“解锁/寄存器/值”配置命令。
 * @param imu_wht101_uart_h 已绑定 uart_h 的对象；只使用 uart_h 字段。
 * @param reg 目标寄存器编号；本函数不做白名单校验。
 * @param val 16 位小端寄存器值；取值范围由具体寄存器决定。
 * @return Dev_UART_Base_Transmit 的阻塞发送结果，DEV_UART_OK 表示已交给后端发送。
 * @pre 对象和 UART 后端可用；在单一任务上下文调用，避免与其他发送者争用同一 UART。
 * @post 发送 5 字节命令（0xFF、0xAA、reg、value 低字节、value 高字节）；命令缓冲为栈上局部量。
 * @note 使用 10 ms 超时；返回非 OK 时本函数不改写计数，由调用方决定如何记录。
 */
static dev_uart_status_t imu_wht101_write_reg(
  imu_wht101_uart_t *imu_wht101_uart_h,
  imu_wht101_reg_t reg,
  uint16_t val
) {
  uint8_t cmd[IMU_WHT101_WRITE_LEN] = {
    IMU_WHT101_WRITE_HDR1,
    IMU_WHT101_WRITE_HDR2,
    (uint8_t)reg,
    (uint8_t)(val & 0xFF),
    (uint8_t)((val >> 8) & 0xFF)
  };
  return Dev_UART_Base_Transmit(
    imu_wht101_uart_h->uart_h,
    cmd,
    IMU_WHT101_WRITE_LEN,
    10u
  );
}

/******************************************************************************/

/**
 * @brief 从当前角度帧解析 yaw。
 * @param imu_wht101_uart_h 已完成校验的角度帧对象。
 * @return yaw 角度，单位 deg，范围 [-180, 180)；帧无效时结果无意义。
 * @pre 调用前 frame[1] 必须为 IMU_WHT101_TYPE_ANGLE 且帧已通过校验和。
 * @post 只读 frame[6..7]，不修改对象状态。
 * @note 原始值按 int16 小端解释，再按 32768 满量程映射到 ±180 deg。
 */
static float imu_wht101_parse_angle(const imu_wht101_uart_t *imu_wht101_uart_h) {
  // 角度帧: 55 53 00 00 00 00 YawL YawH VL VH SUM
  int16_t yaw_raw = (int16_t)((imu_wht101_uart_h->frame[7] << 8) | imu_wht101_uart_h->frame[6]);
  return yaw_raw / IMU_WHT101_SCALE * IMU_WHT101_YAW_RANGE;
}

/**
 * @brief 从当前陀螺帧解析 Z 轴角速度。
 * @param imu_wht101_uart_h 已完成校验的陀螺帧对象。
 * @return Z 轴角速度，单位 deg/s，量程按 ±2000 deg/s 映射；帧无效时结果无意义。
 * @pre 调用前 frame[1] 必须为 IMU_WHT101_TYPE_GYRO 且帧已通过校验和。
 * @post 只读 frame[6..7]，不修改对象状态。
 * @note 原始值按 int16 小端解释；本帧不提供 X/Y 轴数据。
 */
static float imu_wht101_parse_gyro(const imu_wht101_uart_t *imu_wht101_uart_h) {
  // 角速度帧: 55 52 00 00 RWzL RWzH WzL WzH 00 00 SUM
  int16_t wz_raw = (int16_t)((imu_wht101_uart_h->frame[7] << 8) | imu_wht101_uart_h->frame[6]);
  return wz_raw / IMU_WHT101_SCALE * IMU_WHT101_GYRO_RANGE;
}

/**
 * @brief 把单个已解析通道提交给统一 IMU 管线。
 * @param imu_wht101_uart_h 已启动的具体对象；其 imu_base 已完成 Init 和 Start。
 * @param channel 通道下标，仅使用 YAW 或 GYRO_Z；必须在 IMU_CHANNEL_INDEX_COUNT 内。
 * @param value 工程单位值：YAW 为 deg，GYRO_Z 为 deg/s。
 * @param now_ms 当前单调毫秒时间戳，写入该通道的更新时间。
 * @pre 对象已启动；单写者调用；不得在 ISR 中调用。
 * @post 构造只含该通道的 filter_sample_t 并提交；滤波结果由 imu_base 合并发布。
 * @note 提交返回值被有意忽略；失败只可能来自 imu_base 的通道校验，不在此重试。
 */
static void imu_wht101_submit_channel(
  imu_wht101_uart_t *imu_wht101_uart_h,
  imu_channel_index_t channel,
  float value,
  uint32_t now_ms
) {
  filter_sample_t sample = {
    .valid_mask = (uint32_t)1u << channel,
    .time_ms = now_ms,
  };
  sample.value[channel] = value;
  (void)Dev_IMU_Base_SubmitSample(&imu_wht101_uart_h->imu_base, &sample);
}

/******************************************************************************/

/**
 * @brief 初始化 WHT101 对象、解析状态和配置状态。
 * @param imu_h 具体对象；调用方静态分配，函数只初始化已声明字段。
 * @param name 调试名称；只保存指针，其生命周期必须覆盖对象。
 * @param uart_h 已初始化的 UART 后端；只保存非 owning 指针，后端必须支持中断接收。
 * @param filter_h 已初始化的静态滤波器根节点；WHT101 通常注入 passthrough，必须非空。
 * @pre 四个参数非空；uart_h 尚未被本对象占用；对象未被并发访问。
 * @post imu_base 绑定 WHT101 操作表并保存 filter_h；解析/配置/计数状态清零，is_started 为 false。
 * @note 不注册 UART 回调、不启动接收、不写设备寄存器；frame 与 rx_buffer 内容不保证被清零。
 * @note 必须在任务上下文、Start 之前调用；重复初始化会丢弃未完成的配置请求状态。
 * @warning 不分配动态内存；对象、name、uart_h 和 filter_h 的生命周期必须覆盖整个使用期。
 */
void Dev_IMU_WHT101_UART_Init(
  imu_wht101_uart_t *imu_h,
  const char *name,
  uart_base_t *uart_h,
  filter_base_t *filter_h
) {
  assert(imu_h && uart_h && filter_h);

  Dev_IMU_Base_Init(&imu_h->imu_base, name, &imu_ops_wht101, filter_h);
  imu_h->uart_h                   = uart_h;
  imu_h->state                   = IMU_WHT101_STATE_SYNCING_1;
  imu_h->frame_idx               = 0;
  imu_h->is_started              = false;
  imu_h->config_state            = IMU_WHT101_CONFIG_IDLE;
  imu_h->config_deadline_ms      = 0u;
  imu_h->zero_pending            = false;
  imu_h->rate_pending            = false;
  imu_h->config_failed           = false;
  imu_h->rate_pending_value      = 0u;
  imu_h->rx_byte                 = 0;
  imu_h->rx_write                = 0;
  imu_h->rx_read                 = 0;
  imu_h->rx_byte_count           = 0;
  imu_h->rx_overflow_count       = 0;
  imu_h->rx_error_count          = 0;
  imu_h->rx_checksum_error_count = 0;
  imu_h->rx_resync_requested     = false;
}

/**
 * @brief 重新挂接单字节中断接收。
 * @param imu_wht101_uart_h 已绑定 uart_h 的对象；rx_byte 是接收目标。
 * @pre 对象与 uart_h 非空；在任务或 UART 中断上下文调用，rx_byte 地址在完成前保持有效。
 * @post 调用 Dev_UART_ReceiveIT 请求 1 字节接收。
 * @note 本实现要求重挂成功：非 DEV_UART_OK 立即 assert 终止，不设置待重挂标志。
 * @warning 该严格断言意味着 UART 后端短暂 BUSY 也会触发故障；宿主机测试必须提供成功的替身。
 */
static void imu_wht101_rearm_receive(imu_wht101_uart_t *imu_wht101_uart_h) {
  assert(imu_wht101_uart_h);
  dev_uart_status_t status = Dev_UART_ReceiveIT(
    imu_wht101_uart_h->uart_h,
    &imu_wht101_uart_h->rx_byte,
    1u
  );
  assert(status == DEV_UART_OK);
  (void)status;
}

/**
 * @brief UART 接收完成中断回调：单字节入队并重新挂接。
 * @param uart_h 触发事件的 UART 后端；必须等于对象保存的 uart_h。
 * @param context 注册时传入的 imu_wht101_uart_t 指针。
 * @pre 回调和对象生命周期覆盖注册期；只在中断上下文执行。
 * @post rx_byte_count 递增；队列未满时写入并推进 rx_write，满时递增 rx_overflow_count 并请求重同步。
 * @note 不在 ISR 中解析帧；随后尝试重新挂接接收。
 * @warning 重挂失败会触发 assert；队列溢出丢弃当前字节。
 */
static void imu_wht101_rx_complete_cb(uart_base_t *uart_h, void *context) {
  imu_wht101_uart_t *imu_wht101_uart_h = (imu_wht101_uart_t *)context;
  assert(imu_wht101_uart_h && uart_h == imu_wht101_uart_h->uart_h);
  (void)uart_h;

  imu_wht101_uart_h->rx_byte_count++;

  uint16_t next_write =
    (imu_wht101_uart_h->rx_write + 1u) & IMU_WHT101_RX_INDEX_MASK;

  if (next_write == imu_wht101_uart_h->rx_read) {
    imu_wht101_uart_h->rx_overflow_count++;
    imu_wht101_uart_h->rx_resync_requested = true;
  } else {
    imu_wht101_uart_h->rx_buffer[imu_wht101_uart_h->rx_write] = imu_wht101_uart_h->rx_byte;
    imu_wht101_uart_h->rx_write = next_write;
  }

  imu_wht101_rearm_receive(imu_wht101_uart_h);
}

/**
 * @brief UART 错误中断回调：计数、请求重同步并重挂接收。
 * @param uart_h 触发错误的 UART 后端；必须等于对象保存的 uart_h。
 * @param context 注册时传入的 imu_wht101_uart_t 指针。
 * @pre 回调和对象生命周期覆盖注册期；只在中断上下文执行。
 * @post rx_error_count 递增，rx_resync_requested 置 true，并尝试重新挂接接收。
 * @note 解析器在任务侧消费 rx_resync_requested 后回到帧头同步状态。
 * @warning 重挂失败会触发 assert；错误事件本身不携带更多诊断信息。
 */
static void imu_wht101_uart_err_cb(uart_base_t *uart_h, void *context) {
  imu_wht101_uart_t *imu_wht101_uart_h = (imu_wht101_uart_t *)context;
  assert(imu_wht101_uart_h && uart_h == imu_wht101_uart_h->uart_h);
  (void)uart_h;

  imu_wht101_uart_h->rx_error_count++;
  imu_wht101_uart_h->rx_resync_requested = true;

  imu_wht101_rearm_receive(imu_wht101_uart_h);
}

/**
 * @brief 消费 ISR 重同步请求并复位帧解析状态。
 * @param imu_wht101_uart_h 任务侧正在解析的对象。
 * @pre imu_wht101_uart_h 非空；只允许单一解析任务调用。
 * @post 若 rx_resync_requested 为 true，则 state 回到 SYNCING_1、frame_idx 清零并清除该标志；
 *       否则不做任何修改。
 * @note 本函数不丢弃 rx 环形队列中已入队字节，后续字节会被当作新帧起点重新同步。
 */
static void imu_wht101_apply_resync(imu_wht101_uart_t *imu_wht101_uart_h) {
  assert(imu_wht101_uart_h);

  if (!imu_wht101_uart_h->rx_resync_requested) {
    return;
  }

  imu_wht101_uart_h->state = IMU_WHT101_STATE_SYNCING_1;
  imu_wht101_uart_h->frame_idx = 0;
  imu_wht101_uart_h->rx_resync_requested = false;
}

/**
 * @brief 以有符号差值判断毫秒截止时间是否已到达。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @param deadline_ms 目标毫秒时间戳，单位 ms；由 now_ms + 延迟计算并使用无符号回绕。
 * @return now_ms 等于或晚于 deadline_ms 时返回 true，否则返回 false。
 * @note 使用 int32 差值，适用于两次时间戳相差不超过约 24.8 天的单调时钟。
 * @warning 超过半个计数周期时判断无意义；调用方必须保证 now_ms 单调。
 */
static bool imu_wht101_time_reached(uint32_t now_ms, uint32_t deadline_ms) {
  return (int32_t)(now_ms - deadline_ms) >= 0;
}

/**
 * @brief 发送一条配置命令，并在失败时统一记录错误。
 * @param imu_wht101_uart_h 当前配置事务所属的 IMU 对象；必须已由 Init 初始化，且由单一状态机独占。
 * @param reg 目标寄存器编号，范围 0x00..0xFF；本函数不做白名单校验。
 * @param value 16 位小端寄存器值。
 * @return 发送成功返回 true；Dev_UART_Base_Transmit 非 OK 时返回 false。
 * @pre 在任务上下文、单一配置状态机内调用；UART 后端当前没有其他发送者。
 * @post 成功时不改计数；失败时递增 rx_error_count 并置 config_failed。
 * @note 本函数只负责一次阻塞写，不推进 config_state；状态转移由调用方完成。
 */
static bool imu_wht101_config_write(
  imu_wht101_uart_t *imu_wht101_uart_h,
  imu_wht101_reg_t reg,
  uint16_t value
) {
  dev_uart_status_t status = imu_wht101_write_reg(imu_wht101_uart_h, reg, value);
  if (status != DEV_UART_OK) {
    imu_wht101_uart_h->rx_error_count++;
    imu_wht101_uart_h->config_failed = true;
    return false;
  }
  return true;
}

/**
 * @brief 在配置空闲时启动置零或输出速率事务。
 * @param imu_wht101_uart_h 配置事务所属的 IMU 对象；必须由任务上下文单写者持有并已由 Init 初始化。
 * @param now_ms 当前单调毫秒时间戳，用于计算首次解锁后的等待截止时间。
 * @pre 仅由 imu_wht101_config_process 在任务上下文调用；对象未被并发修改。
 * @post 有待处理请求时发送 KEY 解锁并进入对应 WAIT 状态；无请求或事务忙时不改变状态。
 * @note 置零优先于速率；发送失败时 config_write 记录错误并保持 IDLE。
 */
static void imu_wht101_config_start_next(
  imu_wht101_uart_t *imu_wht101_uart_h,
  uint32_t now_ms
) {
  assert(imu_wht101_uart_h);

  if (imu_wht101_uart_h->config_state != IMU_WHT101_CONFIG_IDLE) {
    return;
  }

  if (imu_wht101_uart_h->zero_pending) {
    imu_wht101_uart_h->zero_pending = false;

    if (!imu_wht101_config_write(
          imu_wht101_uart_h,
          IMU_WHT101_REG_KEY,
          IMU_WHT101_KEY_UNLOCK)) {
      return;
    }

    imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_ZERO_WAIT_CALIYAW;
    imu_wht101_uart_h->config_deadline_ms = now_ms + IMU_WHT101_DELAY_UNLOCK_MS;
    return;
  }

  if (imu_wht101_uart_h->rate_pending) {
    imu_wht101_uart_h->rate_pending = false;

    if (!imu_wht101_config_write(
          imu_wht101_uart_h,
          IMU_WHT101_REG_KEY,
          IMU_WHT101_KEY_UNLOCK)) {
      return;
    }

    imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_RATE_WAIT_WRITE;
    imu_wht101_uart_h->config_deadline_ms = now_ms + IMU_WHT101_DELAY_UNLOCK_MS;
  }
}

/**
 * @brief 按截止时间推进置零/速率配置事务。
 * @param imu_wht101_uart_h 配置事务所属的 IMU 对象；必须由任务上下文单写者持有并已由 Init 初始化。
 * @param now_ms 当前单调毫秒时间戳，与本状态机的软延时比较。
 * @pre 仅由 process 在任务上下文按周期调用；单写者。
 * @post 空闲时尝试启动下一请求；到期时依次写 CALIYAW/RRATE 与 SAVE，成功后回 IDLE 并清除 config_failed。
 * @note 延时只保证命令间隔，不读取设备应答；失败时回到 IDLE 并保留 config_failed。
 * @warning 同时排队置零和速率时，两个事务串行执行，总耗时约 800 ms。
 */
static void imu_wht101_config_process(
  imu_wht101_uart_t *imu_wht101_uart_h,
  uint32_t now_ms
) {
  assert(imu_wht101_uart_h);

  if (
    imu_wht101_uart_h->config_state == IMU_WHT101_CONFIG_IDLE ||
    !imu_wht101_time_reached(now_ms, imu_wht101_uart_h->config_deadline_ms)
  ) {
    imu_wht101_config_start_next(imu_wht101_uart_h, now_ms);
    return;
  }

  switch (imu_wht101_uart_h->config_state) {
    case IMU_WHT101_CONFIG_ZERO_WAIT_CALIYAW:
      if (!imu_wht101_config_write(
            imu_wht101_uart_h,
            IMU_WHT101_REG_CALIYAW,
            IMU_WHT101_CALIYAW_ZERO)) {
        imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_IDLE;
        return;
      }
      imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_ZERO_WAIT_SAVE;
      imu_wht101_uart_h->config_deadline_ms = now_ms + IMU_WHT101_DELAY_SAVE_MS;
      break;

    case IMU_WHT101_CONFIG_ZERO_WAIT_SAVE:
      if (!imu_wht101_config_write(
            imu_wht101_uart_h,
            IMU_WHT101_REG_SAVE,
            IMU_WHT101_SAVE_VALUE)) {
        imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_IDLE;
        return;
      }
      imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_IDLE;
      imu_wht101_uart_h->config_failed = false;
      break;

    case IMU_WHT101_CONFIG_RATE_WAIT_WRITE:
      if (!imu_wht101_config_write(
            imu_wht101_uart_h,
            IMU_WHT101_REG_RRATE,
            imu_wht101_uart_h->rate_pending_value)) {
        imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_IDLE;
        return;
      }
      imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_RATE_WAIT_SAVE;
      imu_wht101_uart_h->config_deadline_ms = now_ms + IMU_WHT101_DELAY_WRITE_MS;
      break;

    case IMU_WHT101_CONFIG_RATE_WAIT_SAVE:
      if (!imu_wht101_config_write(
            imu_wht101_uart_h,
            IMU_WHT101_REG_SAVE,
            IMU_WHT101_SAVE_VALUE)) {
        imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_IDLE;
        return;
      }
      imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_IDLE;
      imu_wht101_uart_h->config_failed = false;
      break;

    case IMU_WHT101_CONFIG_IDLE:
    default:
      assert(0);
      break;
  }
}

/**
 * @brief 注册 UART 回调并启动 WHT101 单字节中断接收。
 * @param imu_h base 视图；必须指向 imu_wht101_uart_t 的首成员且已 Init。
 * @pre imu_h 非空且尚未启动；UART 后端无其他回调所有者；在任务上下文调用。
 * @post 回调写入 UART 后端，is_started 置 true，并尝试挂接一字节接收。
 * @note 由 imu_base 的 Start 路径调用；本函数不推进配置状态机。
 */
static void imu_wht101_start(imu_base_t *imu_h) {
  assert(imu_h);
  imu_wht101_uart_t *imu_wht101_uart_h = (imu_wht101_uart_t *)imu_h;

  assert(!imu_wht101_uart_h->is_started);

  dev_uart_callbacks_t callbacks = {
    .rx_complete_cb = imu_wht101_rx_complete_cb,
    .err_cb     = imu_wht101_uart_err_cb,
    .context    = imu_wht101_uart_h,
  };

  Dev_UART_RegisterCallback(imu_wht101_uart_h->uart_h, &callbacks);
  imu_wht101_rearm_receive(imu_wht101_uart_h);

  imu_wht101_uart_h->is_started = true;
}

/**
 * @brief 排空接收环形队列并推进解析与配置状态机。
 * @param imu_h base 视图；必须指向已启动的具体对象。
 * @param now_ms 当前单调毫秒时间戳；供解析帧和配置超时使用。
 * @pre 仅由单一传感器任务按周期调用；不得与 FeedByte 或 SetZero/SetOutputRate 并发。
 * @post 队列字节全部消费，可能发布新通道、推进配置事务或记录错误计数。
 * @note 非阻塞；队列为空时仍会推进配置状态机的超时判断。
 */
static void imu_wht101_process(imu_base_t *imu_h, uint32_t now_ms) {
  assert(imu_h);
  imu_wht101_uart_t *imu_wht101_uart_h = (imu_wht101_uart_t *)imu_h;

  while (imu_wht101_uart_h->rx_read != imu_wht101_uart_h->rx_write) {
    imu_wht101_apply_resync(imu_wht101_uart_h);

    uint8_t byte = imu_wht101_uart_h->rx_buffer[imu_wht101_uart_h->rx_read];
    imu_wht101_uart_h->rx_read =
      (imu_wht101_uart_h->rx_read + 1u) & IMU_WHT101_RX_INDEX_MASK;
    Dev_IMU_WHT101_UART_FeedByte(imu_wht101_uart_h, byte, now_ms);
  }

  imu_wht101_apply_resync(imu_wht101_uart_h);
  imu_wht101_config_process(imu_wht101_uart_h, now_ms);
}

/**
 * @brief 向 WHT101 帧解析状态机输入一个字节。
 * @param imu_h 已 Init 且已 Start 的对象；由 process 或主机测试调用。
 * @param byte 输入字节，取值 0..255；0x55 为帧头，0x52/0x53 为已知类型。
 * @param now_ms 当前单调毫秒时间戳，作为成功提交帧的 time_ms。
 * @pre imu_h 非空且已 Start；对象只允许一个任务写；不得在 ISR 中调用。
 * @post 11 字节帧通过累加校验后提交相应通道；校验失败递增 rx_checksum_error_count；
 *       未知类型或同步错误丢弃当前帧并回到等待帧头。
 * @note 失败不返回错误码；提交失败由 imu_base 内部断言/过滤处理，本函数不重试。
 * @warning 必须在 Start 之后调用，否则 Dev_IMU_Base_SubmitSample 断言失败。
 */
void Dev_IMU_WHT101_UART_FeedByte(
  imu_wht101_uart_t *imu_h,
  uint8_t byte,
  uint32_t now_ms
) {
  assert(imu_h);

  switch (imu_h->state) {
    case IMU_WHT101_STATE_SYNCING_1:
      if (byte == IMU_WHT101_FRAME_HEADER) {
        imu_h->frame[0] = byte;
        imu_h->frame_idx = 1;
        imu_h->state = IMU_WHT101_STATE_SYNCING_2;
      }
      break;

    case IMU_WHT101_STATE_SYNCING_2:
      if (byte == IMU_WHT101_TYPE_GYRO || byte == IMU_WHT101_TYPE_ANGLE) {
        imu_h->frame[1] = byte;
        imu_h->frame_idx = 2;
        imu_h->state = IMU_WHT101_STATE_RECEIVING_DATA;
      } else if (byte == IMU_WHT101_FRAME_HEADER) {
        imu_h->frame[0] = byte;
        imu_h->frame_idx = 1;
      } else {
        imu_h->state = IMU_WHT101_STATE_SYNCING_1;
      }
      break;

    case IMU_WHT101_STATE_RECEIVING_DATA:
      imu_h->frame[imu_h->frame_idx++] = byte;
      if (imu_h->frame_idx == IMU_WHT101_FRAME_LEN - 1) {
        imu_h->state = IMU_WHT101_STATE_CALCULATING_SUM;
      }
      break;

    case IMU_WHT101_STATE_CALCULATING_SUM: {
      imu_h->frame[IMU_WHT101_FRAME_LEN - 1] = byte;
      uint8_t sum = 0;
      for (int i = 0; i < IMU_WHT101_FRAME_LEN - 1; i++) sum += imu_h->frame[i];
      if (sum == imu_h->frame[IMU_WHT101_FRAME_LEN - 1]) {
        if (imu_h->frame[1] == IMU_WHT101_TYPE_GYRO) {
          imu_wht101_submit_channel(
            imu_h,
            IMU_CHANNEL_INDEX_GYRO_Z,
            imu_wht101_parse_gyro(imu_h),
            now_ms
          );
        } else if (imu_h->frame[1] == IMU_WHT101_TYPE_ANGLE) {
          imu_wht101_submit_channel(
            imu_h,
            IMU_CHANNEL_INDEX_YAW,
            imu_wht101_parse_angle(imu_h),
            now_ms
          );
        }
      } else {
        imu_h->rx_checksum_error_count++;
      }
      imu_h->state = IMU_WHT101_STATE_SYNCING_1;
      break;
    }
  }
}

/**
 * @brief 请求设备侧设置 yaw 零点。
 * @param imu_h 已 Start 的对象；请求只置位 pending 标志。
 * @pre imu_h 非空且 is_started 为 true；只能由单一任务调用，不得与 process 并发。
 * @post zero_pending 置 true，config_failed 清零；实际“解锁、CALIYAW、保存”由后续 process 推进。
 * @note 非阻塞，不等待设备应答，也不改变 imu_base 的 yaw 偏移；成功/失败通过状态快照的 is_busy 与 last_request_failed 观察。
 * @warning 若已有配置事务进行中，本请求会等到该事务空闲后执行；不会取消正在进行的写入。
 */
void Dev_IMU_WHT101_UART_SetZero(imu_wht101_uart_t *imu_h) {
  assert(imu_h && imu_h->is_started);
  imu_h->config_failed = false;
  imu_h->zero_pending = true;
}

/**
 * @brief 请求设置设备输出速率。
 * @param imu_h 已 Start 的对象；请求只缓存速率并置位 pending 标志。
 * @param rate RRATE 寄存器原始配置码，典型值为 IMU_RATE_10HZ / 100HZ / 200HZ；
 *             本函数不校验取值，非法码由设备决定是否接受。
 * @pre imu_h 非空且 is_started 为 true；只能由单一任务调用，不得与 process 并发。
 * @post rate_pending_value 保存 rate，rate_pending 置 true，config_failed 清零；
 *       实际“解锁、RRATE、保存”由后续 process 推进。
 * @note 非阻塞；若置零请求同时存在，置零事务先完成；成功/失败通过状态快照观察。
 * @warning 请求不排队多次：新的调用会覆盖尚未处理的 rate_pending_value。
 */
void Dev_IMU_WHT101_UART_SetOutputRate(imu_wht101_uart_t *imu_h, uint8_t rate) {
  assert(imu_h && imu_h->is_started);
  imu_h->config_failed = false;
  imu_h->rate_pending_value = rate;
  imu_h->rate_pending = true;
}

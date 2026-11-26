/**
 * @file imu_wht101.c
 * @brief WHT101 串口陀螺仪驱动实现。
 * @details 属于 Framework/Drivers 设备驱动层，实现维特智能 WHT101 11 字节协议解析、
 *          8 位累加和校验、0xFF 0xAA 寄存器写序列状态机及上电静止自动归零流程。
 * @ingroup Framework_Drivers
 */

#include "imu_wht101.h"
#include "sys_math.h"
#include "sys_time_window.h"
#include <stddef.h>
#include <string.h>

/** @brief 16 位有符号协议定点满量程分母（32768.0f）。 */
#define WHT101_SCALE 32768.0f
/** @brief 偏航角协议满量程（单位：deg，对应 ±180°）。 */
#define WHT101_YAW_RANGE_DEG 180.0f
/** @brief 角速度协议满量程（单位：deg/s，对应 ±2000°/s）。 */
#define WHT101_GYRO_RANGE_DPS 2000.0f

/** @brief 接收帧固定前导同步字节。 */
#define WHT101_FRAME_HEADER 0x55U
/** @brief 角速度帧类型字节（含 Z 轴角速度）。 */
#define WHT101_TYPE_GYRO 0x52U
/** @brief 姿态角度帧类型字节（含 Z 轴偏航角）。 */
#define WHT101_TYPE_ANGLE 0x53U

/** @brief 写寄存器指令前导字节 1。 */
#define WHT101_WRITE_HDR1 0xFFU
/** @brief 写寄存器指令前导字节 2。 */
#define WHT101_WRITE_HDR2 0xAAU
/** @brief 写寄存器指令固定帧长（单位：字节）。 */
#define WHT101_WRITE_LEN 5U

/** @brief 保存配置寄存器地址（SAVE）。 */
#define WHT101_REG_SAVE 0x00U
/** @brief 输出速率寄存器地址（RRATE）。 */
#define WHT101_REG_RRATE 0x03U
/** @brief 写保护解锁密钥寄存器地址（KEY）。 */
#define WHT101_REG_KEY 0x69U
/** @brief 航向角置零寄存器地址（CALIYAW）。 */
#define WHT101_REG_CALIYAW 0x76U

/** @brief 解锁寄存器写保护的固定密钥（0xB588，小端发送）。 */
#define WHT101_KEY_UNLOCK 0xB588U
/** @brief 保存当前配置至模块 Flash 的命令字。 */
#define WHT101_SAVE_VALUE 0x0000U
/** @brief 航向角置零命令字。 */
#define WHT101_CALIYAW_ZERO 0x0000U

/** @brief 发送解锁密钥后等待模块响应的间隔（单位：ms）。 */
#define WHT101_DELAY_UNLOCK_MS 200U
/** @brief 发送置零命令后等待内部算法收敛再保存的间隔（单位：ms）。 */
#define WHT101_DELAY_SAVE_MS 500U
/** @brief 发送普通寄存器写入后等待保存的间隔（单位：ms）。 */
#define WHT101_DELAY_WRITE_MS 100U
/** @brief 单次 5 字节写命令阻塞发送超时（单位：ms）。 */
#define WHT101_TX_TIMEOUT_MS 10U

/** @brief 默认配置阶段超时阈值（单位：ms）。 */
#define WHT101_DEFAULT_CFG_TIMEOUT_MS 3000U
/** @brief 默认等待静止总超时阈值（单位：ms）。 */
#define WHT101_DEFAULT_STILL_TIMEOUT_MS 5000U
/** @brief 默认连续静止确认窗口时长（单位：ms）。 */
#define WHT101_DEFAULT_STILL_CONFIRM_MS 400U
/** @brief 默认静止判定 Z 轴角速度阈值（单位：deg/s）。 */
#define WHT101_DEFAULT_STILL_GYRO_DPS 3.0f
/** @brief 默认归零后等待首帧收敛超时（单位：ms）。 */
#define WHT101_DEFAULT_ZERO_TIMEOUT_MS 2000U
/** @brief 默认归零后允许的最大残差偏航角（单位：deg）。 */
#define WHT101_DEFAULT_ZERO_RESIDUAL_DEG 5.0f

/**
 * @brief 向 WHT101 发送 5 字节小端序写寄存器指令：[0xFF, 0xAA, reg, val_L, val_H]。
 * @param[in,out] imu   WHT101 对象指针。
 * @param[in]     reg   目标寄存器地址。
 * @param[in]     value 16 位写入值（小端序拆分为低/高字节）。
 * @return 串口阻塞发送是否成功。
 */
static bool wht101_write_reg(imu_wht101_t *imu, uint8_t reg, uint16_t value) {
  // 维特智能写指令格式固定 5 字节：0xFF 0xAA <REG> <DATA_L> <DATA_H>
  uint8_t cmd[WHT101_WRITE_LEN] = {
      WHT101_WRITE_HDR1,
      WHT101_WRITE_HDR2,
      reg,
      (uint8_t)(value & 0xFFU),
      (uint8_t)((value >> 8U) & 0xFFU),
  };
  bsp_status_t st = BSP_UART_SendBlocking(&imu->uart, cmd, WHT101_WRITE_LEN, WHT101_TX_TIMEOUT_MS);
  if (st != BSP_OK) {
    imu->tx_error_count++;
    imu->config_failed = true;
    return false;
  }
  return true;
}

/**
 * @brief 将单通道解析结果封装为 filter_sample_t 并提交给 imu_base_t 管线。
 * @param[in,out] imu     WHT101 对象指针。
 * @param[in]     channel 目标通道索引（GYRO_Z 或 YAW）。
 * @param[in]     value   通道物理量（deg/s 或 deg）。
 * @param[in]     now_ms  当前系统单调时间戳（单位：ms）。
 */
static void wht101_submit_channel(imu_wht101_t *imu, imu_channel_index_t channel, float value,
                                  uint32_t now_ms) {
  filter_sample_t sample = {
      .valid_mask = (uint32_t)1U << (uint8_t)channel,
      .time_ms = now_ms,
  };
  sample.value[channel] = value;
  (void)Dev_IMU_Base_SubmitSample(&imu->base, &sample);
}

/**
 * @brief 若当前配置状态机空闲，则按优先级启动挂起的归零或速率配置请求。
 * @param[in,out] imu    WHT101 对象指针。
 * @param[in]     now_ms 当前系统单调时间戳（单位：ms）。
 */
static void wht101_cfg_start_next(imu_wht101_t *imu, uint32_t now_ms) {
  if (imu->cfg_state != IMU_WHT101_CFG_IDLE) {
    return;
  }

  // 所有写寄存器操作均须先发送 KEY=0xB588 解锁，并等待 200 ms 使传感器内部进入可写态
  if (imu->zero_pending) {
    imu->zero_pending = false;
    if (!wht101_write_reg(imu, WHT101_REG_KEY, WHT101_KEY_UNLOCK)) {
      return;
    }
    imu->cfg_state = IMU_WHT101_CFG_ZERO_WAIT_CALIYAW;
    imu->cfg_deadline_ms = now_ms + WHT101_DELAY_UNLOCK_MS;
    return;
  }

  if (imu->rate_pending) {
    imu->rate_pending = false;
    if (!wht101_write_reg(imu, WHT101_REG_KEY, WHT101_KEY_UNLOCK)) {
      return;
    }
    imu->cfg_state = IMU_WHT101_CFG_RATE_WAIT_WRITE;
    imu->cfg_deadline_ms = now_ms + WHT101_DELAY_UNLOCK_MS;
  }
}

/**
 * @brief 非阻塞推进寄存器写指令状态机（解锁 -> 写寄存器 -> 保存）。
 * @param[in,out] imu    WHT101 对象指针。
 * @param[in]     now_ms 当前系统单调时间戳（单位：ms）。
 */
static void wht101_cfg_process(imu_wht101_t *imu, uint32_t now_ms) {
  if (imu->cfg_state == IMU_WHT101_CFG_IDLE ||
      !Sys_Time_HasReachedMs(now_ms, imu->cfg_deadline_ms)) {
    wht101_cfg_start_next(imu, now_ms);
    return;
  }

  switch (imu->cfg_state) {
  case IMU_WHT101_CFG_ZERO_WAIT_CALIYAW:
    if (!wht101_write_reg(imu, WHT101_REG_CALIYAW, WHT101_CALIYAW_ZERO)) {
      imu->cfg_state = IMU_WHT101_CFG_IDLE;
      return;
    }
    imu->cfg_state = IMU_WHT101_CFG_ZERO_WAIT_SAVE;
    imu->cfg_deadline_ms = now_ms + WHT101_DELAY_SAVE_MS;
    break;

  case IMU_WHT101_CFG_ZERO_WAIT_SAVE:
    if (!wht101_write_reg(imu, WHT101_REG_SAVE, WHT101_SAVE_VALUE)) {
      imu->cfg_state = IMU_WHT101_CFG_IDLE;
      return;
    }
    imu->cfg_state = IMU_WHT101_CFG_IDLE;
    imu->config_failed = false;
    break;

  case IMU_WHT101_CFG_RATE_WAIT_WRITE:
    if (!wht101_write_reg(imu, WHT101_REG_RRATE, imu->rate_pending_value)) {
      imu->cfg_state = IMU_WHT101_CFG_IDLE;
      return;
    }
    imu->cfg_state = IMU_WHT101_CFG_RATE_WAIT_SAVE;
    imu->cfg_deadline_ms = now_ms + WHT101_DELAY_WRITE_MS;
    break;

  case IMU_WHT101_CFG_RATE_WAIT_SAVE:
    if (!wht101_write_reg(imu, WHT101_REG_SAVE, WHT101_SAVE_VALUE)) {
      imu->cfg_state = IMU_WHT101_CFG_IDLE;
      return;
    }
    imu->cfg_state = IMU_WHT101_CFG_IDLE;
    imu->config_failed = false;
    break;

  case IMU_WHT101_CFG_IDLE:
  default:
    imu->cfg_state = IMU_WHT101_CFG_IDLE;
    break;
  }
}

/**
 * @brief 切换上电自动归零状态机阶段并复位静止跟踪计时。
 * @param[in,out] imu    WHT101 对象指针。
 * @param[in]     next   目标启动状态。
 * @param[in]     now_ms 当前系统单调时间戳（单位：ms）。
 */
static void wht101_enter_boot_state(imu_wht101_t *imu, imu_wht101_boot_state_t next,
                                    uint32_t now_ms) {
  imu->boot_state = next;
  imu->boot_state_started_ms = now_ms;
  imu->still_since_ms = 0U;
  imu->still_tracking = false;
}

/**
 * @brief 推进上电自动归零状态机：速率配置 -> 静止等待 -> 硬件归零 -> 残差验证。
 * @param[in,out] imu    WHT101 对象指针。
 * @param[in]     now_ms 当前系统单调时间戳（单位：ms）。
 */
static void wht101_boot_process(imu_wht101_t *imu, uint32_t now_ms) {
  if (!imu->auto_zero_on_boot || imu->boot_state == IMU_WHT101_BOOT_READY ||
      imu->boot_state == IMU_WHT101_BOOT_FAULT || imu->boot_state == IMU_WHT101_BOOT_IDLE) {
    return;
  }

  imu_status_t status = {0};
  Dev_IMU_Base_GetStatus(&imu->base, &status);

  switch (imu->boot_state) {
  case IMU_WHT101_BOOT_RATE_CONFIG:
    if (status.last_request_failed) {
      imu->boot_state = IMU_WHT101_BOOT_FAULT;
      return;
    }
    if (!status.is_busy) {
      // 记录速率配置完成时刻，后续仅采信此时间戳之后到达的新鲜数据帧
      imu->config_done_ms = now_ms;
      wht101_enter_boot_state(imu, IMU_WHT101_BOOT_WAIT_STILL, now_ms);
    } else if (Sys_Time_HasElapsedMs(now_ms, imu->boot_state_started_ms, imu->cfg_timeout_ms)) {
      imu->boot_state = IMU_WHT101_BOOT_FAULT;
    }
    break;

  case IMU_WHT101_BOOT_WAIT_STILL: {
    imu_sample_t sample = {0};
    if (Dev_IMU_Base_GetSample(&imu->base, &sample)) {
      // 要求 YAW 与 GYRO_Z 均已在配置完成后更新，且 |wz| 持续低于静止阈值
      bool fresh = ((sample.valid_mask & IMU_CHANNEL_YAW) != 0U) &&
                   ((sample.valid_mask & IMU_CHANNEL_GYRO_Z) != 0U) &&
                   Sys_Time_IsAfterMs(sample.channel_update_ms[IMU_CHANNEL_INDEX_YAW],
                                      imu->config_done_ms) &&
                   Sys_Time_IsAfterMs(sample.channel_update_ms[IMU_CHANNEL_INDEX_GYRO_Z],
                                      imu->config_done_ms);
      bool still = fresh && (SYS_ABS(sample.gyro_dps[2]) <= imu->still_gyro_dps);

      if (still) {
        if (!imu->still_tracking) {
          imu->still_since_ms = now_ms;
          imu->still_tracking = true;
        } else if (Sys_Time_HasElapsedMs(now_ms, imu->still_since_ms, imu->still_confirm_ms)) {
          Dev_IMU_WHT101_SetZero(imu);
          wht101_enter_boot_state(imu, IMU_WHT101_BOOT_ZERO_CONFIG, now_ms);
          return;
        }
      } else {
        imu->still_tracking = false;
      }
    }

    if (Sys_Time_HasElapsedMs(now_ms, imu->boot_state_started_ms, imu->still_timeout_ms)) {
      imu->boot_state = IMU_WHT101_BOOT_FAULT;
    }
    break;
  }

  case IMU_WHT101_BOOT_ZERO_CONFIG:
    if (status.last_request_failed) {
      imu->boot_state = IMU_WHT101_BOOT_FAULT;
      return;
    }
    if (!status.is_busy) {
      imu->zero_done_ms = now_ms;
      wht101_enter_boot_state(imu, IMU_WHT101_BOOT_WAIT_ZERO_FRAME, now_ms);
    } else if (Sys_Time_HasElapsedMs(now_ms, imu->boot_state_started_ms, imu->cfg_timeout_ms)) {
      imu->boot_state = IMU_WHT101_BOOT_FAULT;
    }
    break;

  case IMU_WHT101_BOOT_WAIT_ZERO_FRAME: {
    imu_sample_t sample = {0};
    // 确认归零指令之后收到新角度帧且 |yaw| 收敛到残差范围内，再叠加上软件归零消除微小余差
    if (Dev_IMU_Base_GetSample(&imu->base, &sample) &&
        ((sample.valid_mask & IMU_CHANNEL_YAW) != 0U) &&
        Sys_Time_IsAfterMs(sample.channel_update_ms[IMU_CHANNEL_INDEX_YAW], imu->zero_done_ms) &&
        (SYS_ABS(sample.yaw_deg) <= imu->zero_residual_deg)) {
      Dev_IMU_Base_ZeroYaw(&imu->base);
      wht101_enter_boot_state(imu, IMU_WHT101_BOOT_READY, now_ms);
    } else if (Sys_Time_HasElapsedMs(now_ms, imu->boot_state_started_ms, imu->zero_timeout_ms)) {
      imu->boot_state = IMU_WHT101_BOOT_FAULT;
    }
    break;
  }

  case IMU_WHT101_BOOT_IDLE:
  case IMU_WHT101_BOOT_READY:
  case IMU_WHT101_BOOT_FAULT:
  default:
    break;
  }
}

/**
 * @brief WHT101 虚表回调：启动 UART 接收并初始化上电归零状态机。
 * @param[in,out] imu_h IMU 基类指针（实际指向 imu_wht101_t）。
 */
static void wht101_start_op(imu_base_t *imu_h) {
  imu_wht101_t *imu = (imu_wht101_t *)imu_h;
  if (imu == NULL || imu->is_started) {
    return;
  }
  (void)BSP_UART_StartRx(&imu->uart);
  imu->is_started = true;
  if (imu->auto_zero_on_boot) {
    imu->boot_started_pending = true;
    imu->boot_state = IMU_WHT101_BOOT_RATE_CONFIG;
    imu->rate_pending_value = (uint8_t)imu->boot_rate;
    imu->rate_pending = true;
    imu->config_failed = false;
  } else {
    imu->boot_started_pending = false;
    imu->boot_state = IMU_WHT101_BOOT_READY;
  }
}

/**
 * @brief WHT101 虚表回调：补挂接收、排空 ISR 环形缓冲并推进配置/启动状态机。
 * @param[in,out] imu_h  IMU 基类指针（实际指向 imu_wht101_t）。
 * @param[in]     now_ms 当前系统单调时间戳（单位：ms）。
 */
static void wht101_process_op(imu_base_t *imu_h, uint32_t now_ms) {
  imu_wht101_t *imu = (imu_wht101_t *)imu_h;
  if (imu == NULL || !imu->is_started) {
    return;
  }

  if (imu->boot_started_pending) {
    imu->boot_started_pending = false;
    imu->boot_state_started_ms = now_ms;
  }

  // 若底层 UART 因错误中止接收，自动重新挂接；发生溢出/帧错时复位帧同步状态机
  BSP_UART_EnsureRxArmed(&imu->uart);

  uint8_t byte = 0U;
  while (BSP_UART_PopByte(&imu->uart, &byte)) {
    if (BSP_UART_TakeResync(&imu->uart)) {
      imu->rx_state = IMU_WHT101_RX_SYNC_1;
      imu->frame_idx = 0U;
    }
    Dev_IMU_WHT101_FeedByte(imu, byte, now_ms);
  }

  if (BSP_UART_TakeResync(&imu->uart)) {
    imu->rx_state = IMU_WHT101_RX_SYNC_1;
    imu->frame_idx = 0U;
  }

  wht101_cfg_process(imu, now_ms);
  wht101_boot_process(imu, now_ms);
}

/**
 * @brief WHT101 虚表回调：汇总配置忙状态与串口收发/校验错误计数。
 * @param[in,out] imu_h  IMU 基类指针（实际指向 imu_wht101_t）。
 * @param[out]    status 状态输出结构体指针。
 */
static void wht101_get_status_op(imu_base_t *imu_h, imu_status_t *status) {
  imu_wht101_t *imu = (imu_wht101_t *)imu_h;
  if (imu == NULL || status == NULL) {
    return;
  }

  uint64_t err_sum = (uint64_t)imu->tx_error_count + (uint64_t)imu->uart.rx_error_count +
                     (uint64_t)imu->uart.rx_dropped_bytes + (uint64_t)imu->rx_checksum_error_count;
  if (err_sum > UINT32_MAX) {
    err_sum = UINT32_MAX;
  }

  *status = (imu_status_t){
      .is_started = imu->is_started,
      .is_busy = (imu->cfg_state != IMU_WHT101_CFG_IDLE) || imu->zero_pending || imu->rate_pending,
      .last_request_failed = imu->config_failed,
      .error_count = (uint32_t)err_sum,
  };
}

/**
 * @brief WHT101 虚表回调：转发硬件偏航归零请求。
 * @param[in,out] imu_h IMU 基类指针（实际指向 imu_wht101_t）。
 */
static void wht101_set_zero_op(imu_base_t *imu_h) { Dev_IMU_WHT101_SetZero((imu_wht101_t *)imu_h); }

/** @brief WHT101 IMU 多态虚表实例。 */
static const imu_ops_t s_imu_ops_wht101 = {
    .start = wht101_start_op,
    .process = wht101_process_op,
    .get_status = wht101_get_status_op,
    .set_zero = wht101_set_zero_op,
};

bsp_status_t Dev_IMU_WHT101_Init(imu_wht101_t *imu, const imu_wht101_cfg_t *cfg) {
  if (imu == NULL || cfg == NULL || cfg->name == NULL || cfg->huart == NULL ||
      cfg->filter == NULL) {
    return BSP_ERR_INVALID_ARG;
  }

  bsp_uart_rx_mode_t rx_mode =
      (cfg->rx_mode == BSP_UART_RX_NONE) ? BSP_UART_RX_IT_BYTE : cfg->rx_mode;
  if (rx_mode != BSP_UART_RX_IT_BYTE && rx_mode != BSP_UART_RX_DMA_IDLE) {
    return BSP_ERR_INVALID_ARG;
  }

  float still_gyro_dps =
      (cfg->still_gyro_dps > 0.0f) ? cfg->still_gyro_dps : WHT101_DEFAULT_STILL_GYRO_DPS;
  float zero_residual_deg =
      (cfg->zero_residual_deg > 0.0f) ? cfg->zero_residual_deg : WHT101_DEFAULT_ZERO_RESIDUAL_DEG;
  if (!SYS_IS_POS_FINITE(still_gyro_dps) || !SYS_IS_POS_FINITE(zero_residual_deg)) {
    return BSP_ERR_INVALID_ARG;
  }

  (void)memset(imu, 0, sizeof(*imu));

  const bsp_uart_cfg_t uart_cfg = {
      .huart = cfg->huart,
      .rx_mode = rx_mode,
      .rx_ring_buf = imu->rx_ring_buf,
      .rx_ring_size = (uint16_t)sizeof(imu->rx_ring_buf),
      .rx_dma_buf = (rx_mode == BSP_UART_RX_DMA_IDLE) ? imu->rx_dma_buf : NULL,
      .rx_dma_size = (rx_mode == BSP_UART_RX_DMA_IDLE) ? (uint16_t)sizeof(imu->rx_dma_buf) : 0U,
      .on_tx_done = NULL,
      .tx_ctx = NULL,
  };

  bsp_status_t st = BSP_UART_Init(&imu->uart, &uart_cfg);
  if (st != BSP_OK) {
    return st;
  }

  Dev_IMU_Base_Init(&imu->base, cfg->name, &s_imu_ops_wht101, cfg->filter);
  imu->auto_zero_on_boot = cfg->auto_zero_on_boot;
  imu->boot_rate = ((uint32_t)cfg->boot_rate == 0U) ? IMU_WHT101_RATE_10HZ : cfg->boot_rate;
  imu->boot_state = IMU_WHT101_BOOT_IDLE;
  imu->cfg_timeout_ms =
      (cfg->cfg_timeout_ms > 0U) ? cfg->cfg_timeout_ms : WHT101_DEFAULT_CFG_TIMEOUT_MS;
  imu->still_timeout_ms =
      (cfg->still_timeout_ms > 0U) ? cfg->still_timeout_ms : WHT101_DEFAULT_STILL_TIMEOUT_MS;
  imu->still_confirm_ms =
      (cfg->still_confirm_ms > 0U) ? cfg->still_confirm_ms : WHT101_DEFAULT_STILL_CONFIRM_MS;
  imu->still_gyro_dps = still_gyro_dps;
  imu->zero_timeout_ms =
      (cfg->zero_timeout_ms > 0U) ? cfg->zero_timeout_ms : WHT101_DEFAULT_ZERO_TIMEOUT_MS;
  imu->zero_residual_deg = zero_residual_deg;
  return BSP_OK;
}

void Dev_IMU_WHT101_FeedByte(imu_wht101_t *imu, uint8_t byte, uint32_t now_ms) {
  if (imu == NULL) {
    return;
  }

  // WHT101 11 字节协议帧结构：
  // [0]=0x55 帧头, [1]=0x52(角速度)/0x53(角度), [2..9]=4 个 int16 小端数据(X,Y,Z,Reserved/Temp),
  // [10]=前 10 字节之和的低 8 位校验和
  switch (imu->rx_state) {
  case IMU_WHT101_RX_SYNC_1:
    if (byte == WHT101_FRAME_HEADER) {
      imu->frame[0] = byte;
      imu->frame_idx = 1U;
      imu->rx_state = IMU_WHT101_RX_SYNC_2;
    }
    break;

  case IMU_WHT101_RX_SYNC_2:
    if (byte == WHT101_TYPE_GYRO || byte == WHT101_TYPE_ANGLE) {
      imu->frame[1] = byte;
      imu->frame_idx = 2U;
      imu->rx_state = IMU_WHT101_RX_DATA;
    } else if (byte == WHT101_FRAME_HEADER) {
      // 连续出现 0x55 时将当前字节作为新的帧头起点，避免漏同步
      imu->frame[0] = byte;
      imu->frame_idx = 1U;
    } else {
      imu->rx_state = IMU_WHT101_RX_SYNC_1;
    }
    break;

  case IMU_WHT101_RX_DATA:
    imu->frame[imu->frame_idx++] = byte;
    if (imu->frame_idx == (IMU_WHT101_FRAME_LEN - 1U)) {
      imu->rx_state = IMU_WHT101_RX_CHECKSUM;
    }
    break;

  case IMU_WHT101_RX_CHECKSUM: {
    imu->frame[IMU_WHT101_FRAME_LEN - 1U] = byte;
    // 校验和为 frame[0..9] 的无符号 8 位累加截断值
    uint8_t sum = 0U;
    for (uint8_t i = 0U; i < (IMU_WHT101_FRAME_LEN - 1U); ++i) {
      sum = (uint8_t)(sum + imu->frame[i]);
    }
    if (sum == byte) {
      // Z 轴数据位于载荷第 3 个 int16（frame[6]=低字节, frame[7]=高字节）
      int16_t raw = (int16_t)(((uint16_t)imu->frame[7] << 8U) | (uint16_t)imu->frame[6]);
      if (imu->frame[1] == WHT101_TYPE_GYRO) {
        float wz_dps = ((float)raw / WHT101_SCALE) * WHT101_GYRO_RANGE_DPS;
        wht101_submit_channel(imu, IMU_CHANNEL_INDEX_GYRO_Z, wz_dps, now_ms);
      } else if (imu->frame[1] == WHT101_TYPE_ANGLE) {
        float yaw_deg = ((float)raw / WHT101_SCALE) * WHT101_YAW_RANGE_DEG;
        wht101_submit_channel(imu, IMU_CHANNEL_INDEX_YAW, yaw_deg, now_ms);
      }
    } else {
      imu->rx_checksum_error_count++;
    }
    imu->rx_state = IMU_WHT101_RX_SYNC_1;
    break;
  }

  default:
    imu->rx_state = IMU_WHT101_RX_SYNC_1;
    break;
  }
}

void Dev_IMU_WHT101_SetZero(imu_wht101_t *imu) {
  if (imu == NULL || !imu->is_started) {
    return;
  }
  imu->config_failed = false;
  imu->zero_pending = true;
}

void Dev_IMU_WHT101_SetOutputRate(imu_wht101_t *imu, uint8_t rate) {
  if (imu == NULL || !imu->is_started) {
    return;
  }
  imu->config_failed = false;
  imu->rate_pending_value = rate;
  imu->rate_pending = true;
}

imu_wht101_boot_state_t Dev_IMU_WHT101_GetBootState(const imu_wht101_t *imu) {
  return (imu != NULL) ? imu->boot_state : IMU_WHT101_BOOT_FAULT;
}

bool Dev_IMU_WHT101_IsReady(const imu_wht101_t *imu) {
  return (imu != NULL) && imu->is_started && (imu->boot_state == IMU_WHT101_BOOT_READY);
}

bool Dev_IMU_WHT101_HasFault(const imu_wht101_t *imu) {
  return (imu != NULL) && (imu->boot_state == IMU_WHT101_BOOT_FAULT);
}

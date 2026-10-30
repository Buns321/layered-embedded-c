//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file app_fast_channel.h
 * @brief F103 速通赛道任务一应用入口、状态和一致快照。
 * @addtogroup app
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_APP_FAST_CHANNEL_H
#define LAYERED_EMBEDDED_C_APP_FAST_CHANNEL_H

#include "app_calibration.h"
#include "app_uart_link.h"
#include "filter_ekf.h"
#include "imu_base.h"
#include "mpu6050_i2c.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 应用主状态。
 */
typedef enum app_fast_channel_state_e {
  APP_FAST_CHANNEL_STATE_UNINITIALIZED = 0U,
  APP_FAST_CHANNEL_STATE_INITIAL_POSE_CHECK,
  APP_FAST_CHANNEL_STATE_GYRO_CALIBRATION,
  APP_FAST_CHANNEL_STATE_WAIT_FACE,
  APP_FAST_CHANNEL_STATE_STABILIZE_FACE,
  APP_FAST_CHANNEL_STATE_SAMPLE_FACE,
  APP_FAST_CHANNEL_STATE_MAIN,
  APP_FAST_CHANNEL_STATE_ERROR,
} app_fast_channel_state_t;

/**
 * @brief 任务耗时和周期统计。
 */
typedef struct app_task_metrics_s {
  /** @brief 单次 ImuEkfTask 执行时间，单位 us。 */
  uint32_t execution_us;
  /** @brief 相对 10 ms 目标周期的绝对抖动，单位 ms。 */
  uint32_t period_jitter_ms;
  /** @brief 当前任务剩余栈空间，单位 byte。 */
  uint32_t stack_free_bytes;
} app_task_metrics_t;

/**
 * @brief 遥测任务使用的一致数据快照。
 */
typedef struct app_fast_channel_snapshot_s {
  /** @brief 未经应用层校正的最近原始样本。 */
  mpu6050_raw_sample_t raw_sample;
  /** @brief 经 Filter Chain 处理并包含 EKF 姿态的样本。 */
  imu_sample_t calibrated_sample;
  /** @brief EKF 最近一次状态快照，内部角度单位为 rad。 */
  filter_ekf_state_t ekf_state;
  /** @brief 当前标定状态机状态。 */
  app_fast_channel_state_t calibration_state;
  /** @brief 已接受六面的位掩码。 */
  uint32_t face_mask;
  /** @brief 正负样本齐全的三轴位掩码。 */
  uint32_t completed_axis_mask;
  /** @brief 最近一条参数命令结果。 */
  uint32_t command_status;
  /** @brief IMU 发布快照的递增序号。 */
  uint32_t sequence;
  /** @brief 加速度直接解算横滚角，单位 rad。 */
  float direct_roll_rad;
  /** @brief 加速度直接解算俯仰角，单位 rad。 */
  float direct_pitch_rad;
  /** @brief 陀螺积分横滚角，单位 rad。 */
  float integrated_roll_rad;
  /** @brief 陀螺积分俯仰角，单位 rad。 */
  float integrated_pitch_rad;
  /** @brief 陀螺积分偏航角，单位 rad。 */
  float integrated_yaw_rad;
  /** @brief 最近一次 EKF 更新使用的实际周期，单位 s。 */
  float dt_s;
  /** @brief 原始样本是否有效。 */
  bool raw_valid;
  /** @brief 滤波后样本是否有效。 */
  bool calibrated_valid;
  /** @brief 是否成功取得过 Flash 校准记录。 */
  bool flash_record_valid;
  /** @brief 本次运行是否出现过 Flash 写入失败。 */
  bool flash_write_failed;
} app_fast_channel_snapshot_t;

/**
 * @brief 初始化板级设备、滤波链、Flash 校准和 UART 链路。
 * @retval PLATFORM_STATUS_OK 设备、滤波链、校准状态和命令链路已初始化；重复调用直接返回 OK。
 * @retval PLATFORM_STATUS_BUSY 物理 UART 回调槽已被其他所有者占用。
 * @retval PLATFORM_STATUS_ERROR IMU 校准滤波器、EKF、MPU6050 或 UART 端口初始化失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 板级静态配置或 UART 实例不满足底层前置条件。
 * @pre CubeMX 已初始化 I2C1、USART1、DMA1_Channel4、GPIO 和 CRC。
 * @pre 首次初始化必须在 RTOS 调度器启动前完成，并独占 I2C1、USART1 和 Flash；已初始化后的重复调用直接返回 OK。
 * @post 成功时进入 INITIAL_POSE_CHECK，电机外设尚未参与；RX 启动失败会记录警告并延后重试。
 * @post Flash 记录缺失或损坏不阻止启动，但 flash_record_valid 为 false；后续提交会尝试重建记录。
 * @note 不分配动态内存；初始化包含同步 I2C/Flash 操作，失败时模块保持未初始化。
 */
platform_status_t App_FastChannel_Init(void);

/**
 * @brief 执行一次 100 Hz IMU、标定和 EKF 处理。
 * @param now_ms 当前单调毫秒时间戳；首帧可传 0，state_started_ms 为 0 时会用该值初始化状态起点。
 * @pre 只允许 ImuEkfTask 在 10 ms 周期上下文中调用，且 App_FastChannel_Init 已成功返回。
 * @post 已驱动一次 MPU6050 采样/滤波，推进姿态、陀螺和六面标定状态机，并发布一致快照。
 * @post 同步推进蜂鸣器和 LED 时序；未初始化时函数直接返回，状态保持不变。
 * @note 任务上下文执行，可能等待最多 APP_IMU_I2C_TIMEOUT_MS 的 I2C 事务；六面提交时还可能在任务内执行 Flash 擦写并显著延长本次调用；不分配内存。
 */
void App_FastChannel_ProcessImu(uint32_t now_ms);

/**
 * @brief 处理命令并发送 50 Hz 32 通道 JustFloat。
 * @param now_ms 当前单调毫秒时间戳，同时写入遥测样本时间戳。
 * @pre 只允许 VofaTask 在 20 ms 周期上下文中调用，且 App_FastChannel_Init 已成功返回。
 * @post 已消费 RX 命令、复制最新一致快照、组装 32 通道 JustFloat 并发起一次非阻塞发送。
 * @post 约每 1000 ms 输出一条 RTT 状态日志；发送忙时整帧丢弃，不影响 IMU 状态机。
 * @note 任务上下文；命令回调同步执行，不分配内存，不等待 DMA 完成。
 */
void App_FastChannel_ProcessTelemetry(uint32_t now_ms);

/**
 * @brief 复制最近一次一致快照。
 * @param snapshot 输出快照；函数完整覆盖其全部字段，所有权仍归调用方。
 * @pre snapshot 非 NULL，可写且不与内部双缓冲重叠。
 * @post 返回的 raw/calibrated/EKF、标定状态、序列号和 Flash 标志来自同一个已发布缓冲区。
 * @note 使用短临界区复制整个快照；不分配内存，可从任务上下文调用。
 * @warning 复制期间会短暂关闭中断，禁止在 ISR 或高实时性路径调用。
 */
void App_FastChannel_GetSnapshot(
  app_fast_channel_snapshot_t *snapshot
);

/**
 * @brief 更新 ImuEkfTask 性能统计。
 * @param metrics 最新执行时间、周期抖动和剩余栈字节；只读，调用方保留所有权。
 * @pre 只允许 ImuEkfTask 调用，metrics 非 NULL 且包含本单位换算后的有效统计。
 * @post 内部统计已在临界区内整体替换，供 VofaTask 日志读取；不改变姿态或标定状态。
 * @note 使用短 PRIMASK 临界区保证写侧一致；读取侧未加锁，三个字段可能来自相邻更新。
 * @note 不分配内存，调试构建对空指针 assert。
 */
void App_FastChannel_ReportImuMetrics(
  const app_task_metrics_t *metrics
);

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_FAST_CHANNEL_H

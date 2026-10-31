/**
 * @file app_fast_channel.h
 * @brief F103 速通任务一板级装配和任务入口。
 * @addtogroup app
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_APP_FAST_CHANNEL_H
#define LAYERED_EMBEDDED_C_APP_FAST_CHANNEL_H

#include "app_imu_session.h"
#include "platform_def.h"
#include <stdint.h>

/**
 * @brief 初始化板级设备、滤波链、UART Link 和 IMU 会话。
 * @retval PLATFORM_STATUS_OK 全部设备与会话已初始化；重复调用直接返回 OK。
 * @retval PLATFORM_STATUS_BUSY 物理 UART 回调槽已被其他所有者占用。
 * @retval PLATFORM_STATUS_ERROR 滤波器、EKF、UART、MPU6050 或 Flash 初始化失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 底层静态配置不合法。
 * @pre CubeMX 已初始化 I2C1、USART1、DMA1_Channel4、GPIO 和 CRC。
 * @pre 首次初始化必须在 RTOS 调度器启动前单线程调用。
 * @post 成功时 MPU6050、IMU 会话和 USART1 文本行链路已启动或进入自动重挂状态。
 * @note 包含同步 I2C/Flash 操作；不分配内存。
 */
platform_status_t App_FastChannel_Init(void);

/**
 * @brief 推进一次 100 Hz IMU 标定和姿态会话。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 只允许 ImuEkfTask 在 10 ms 周期上下文中调用。
 * @post IMU 采样、标定、参考姿态和一致快照均已推进。
 * @note 可能阻塞等待 I2C 或 Flash；不分配内存。
 */
void App_FastChannel_ProcessImu(uint32_t now_ms);

/**
 * @brief 消费 UART 命令并发送一帧 50 Hz JustFloat。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 只允许 VofaTask 在 20 ms 周期上下文中调用。
 * @post 已处理 RX 文本行、更新 EKF 参数并尝试非阻塞发送 32 通道遥测。
 * @note 不等待 DMA、不分配内存。
 */
void App_FastChannel_ProcessTelemetry(uint32_t now_ms);

/**
 * @brief 复制最近一次 IMU 会话快照。
 * @param snapshot 调用方输出存储，不得为 NULL。
 * @pre snapshot 可写且不与内部存储重叠。
 * @post 获得一致的 IMU、EKF、标定和性能统计快照。
 * @note 使用短临界区复制；不分配内存。
 */
void App_FastChannel_GetSnapshot(
  app_fast_channel_snapshot_t *snapshot
);

/**
 * @brief 更新 ImuEkfTask 性能统计。
 * @param metrics 非空统计对象，只在调用期间读取。
 * @pre 只允许 ImuEkfTask 调用。
 * @post 后续公开快照包含新统计。
 * @note 不分配内存。
 */
void App_FastChannel_ReportImuMetrics(
  const app_task_metrics_t *metrics
);

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_FAST_CHANNEL_H

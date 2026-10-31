/**
 * @file app_fast_channel.c
 * @brief F103 速通任务一板级装配、命令和任务入口实现。
 * @ingroup app
 */

#include "app_fast_channel.h"
#include "app_fast_channel_config.h"
#include "app_fast_channel_telemetry.h"
#include "app_command_parser.h"
#include "filter_chain.h"
#include "justfloat_uart.h"
#include "mpu6050_i2c.h"
#include "system_log.h"
#include "system_text.h"
#include "uart_link.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

static mpu6050_i2c_t s_imu;
static filter_imu_calibration_t s_calibration_filter;
static filter_ekf_t s_ekf;
static filter_chain_t s_filter_chain;
static uart_link_t s_uart_link;
static justfloat_uart_t s_justfloat;
static app_fast_channel_telemetry_t s_telemetry;
static uint8_t s_uart_rx_buffer[APP_UART_RX_RING_CAPACITY];
static char s_uart_line_buffer[APP_UART_COMMAND_CAPACITY];
static app_fast_channel_command_status_t s_command_status;
static bool s_initialized;

/**
 * @brief 解析并应用 EKF 参数命令。
 * @param command 非空命令对象，只在调用期间有效。
 * @param context 未使用上下文，可为 NULL。
 * @retval true RESET EKF 已执行，或 SET 参数通过 EKF 范围校验。
 * @retval false 命令类型未知、参数名不支持或 EKF 拒绝参数。
 * @pre 只允许 VofaTask 命令处理路径调用。
 * @post 接受命令时提交下一帧生效的 EKF 配置或默认值。
 * @note 不访问硬件、不分配内存。
 */
static bool app_fastchannel_applycommand(
  const app_uart_command_t *command,
  void *context
) {
  (void)context;
  assert(command);

  if (command->kind == APP_UART_COMMAND_RESET_EKF) {
    Filter_Ekf_ResetParameters(&s_ekf);
    return true;
  }
  if (command->kind != APP_UART_COMMAND_SET) {
    return false;
  }

  filter_ekf_parameter_t parameter;
  if (Sys_Text_EqualsIgnoreCase(command->name, "Q_ANGLE")) {
    parameter = FILTER_EKF_PARAM_ANGLE_PROCESS_NOISE;
  } else if (Sys_Text_EqualsIgnoreCase(command->name, "Q_BIAS")) {
    parameter = FILTER_EKF_PARAM_BIAS_PROCESS_NOISE;
  } else if (Sys_Text_EqualsIgnoreCase(command->name, "R_ACCEL")) {
    parameter = FILTER_EKF_PARAM_ACCEL_MEASUREMENT_NOISE;
  } else if (Sys_Text_EqualsIgnoreCase(command->name, "P0_ANGLE")) {
    parameter = FILTER_EKF_PARAM_INITIAL_ANGLE_VARIANCE;
  } else if (Sys_Text_EqualsIgnoreCase(command->name, "P0_BIAS")) {
    parameter = FILTER_EKF_PARAM_INITIAL_BIAS_VARIANCE;
  } else if (Sys_Text_EqualsIgnoreCase(command->name, "ACCEL_MIN")) {
    parameter = FILTER_EKF_PARAM_ACCEL_NORM_MIN;
  } else if (Sys_Text_EqualsIgnoreCase(command->name, "ACCEL_MAX")) {
    parameter = FILTER_EKF_PARAM_ACCEL_NORM_MAX;
  } else if (Sys_Text_EqualsIgnoreCase(command->name, "GYRO_LIMIT")) {
    parameter = FILTER_EKF_PARAM_GYRO_LIMIT;
  } else if (Sys_Text_EqualsIgnoreCase(command->name, "BIAS_LIMIT")) {
    parameter = FILTER_EKF_PARAM_BIAS_LIMIT;
  } else if (Sys_Text_EqualsIgnoreCase(command->name, "DT_MIN")) {
    parameter = FILTER_EKF_PARAM_MIN_DT;
  } else if (Sys_Text_EqualsIgnoreCase(command->name, "DT_MAX")) {
    parameter = FILTER_EKF_PARAM_MAX_DT;
  } else {
    return false;
  }

  return Filter_Ekf_SetParameter(&s_ekf, parameter, command->value);
}

/**
 * @brief 解析一条完整 UART 文本行并更新 App 命令状态。
 * @param line 可写完整文本行，不得为 NULL。
 * @param now_ms 当前单调毫秒时间戳，单位 ms；当前命令处理不使用。
 * @param context 未使用上下文，可为 NULL。
 * @retval true 命令语法正确且业务处理接受。
 * @retval false 语法、参数名或参数范围被拒绝。
 * @pre 只允许 VofaTask 通过 uart_link 行回调调用。
 * @post s_command_status 反映 ACCEPTED、REJECTED 或 FORMAT_ERROR。
 * @note 会原地修改 line；不阻塞、不分配内存。
 */
static bool app_fastchannel_handleline(
  char *line,
  uint32_t now_ms,
  void *context
) {
  assert(line);
  (void)now_ms;

  app_uart_command_t command;
  if (!App_Command_Parse(line, &command)) {
    s_command_status = APP_FAST_CHANNEL_COMMAND_FORMAT_ERROR;
    return false;
  }

  bool accepted = app_fastchannel_applycommand(&command, context);
  s_command_status = accepted
    ? APP_FAST_CHANNEL_COMMAND_ACCEPTED
    : APP_FAST_CHANNEL_COMMAND_REJECTED;
  return accepted;
}

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
platform_status_t App_FastChannel_Init(void) {
  if (s_initialized) {
    return PLATFORM_STATUS_OK;
  }

  const filter_imu_calibration_config_t default_calibration = {
    .accel_scale = {1.0f, 1.0f, 1.0f},
  };
  if (!Filter_ImuCalibration_Init(
        &s_calibration_filter,
        "imu_calibration",
        &default_calibration
      )) {
    return PLATFORM_STATUS_ERROR;
  }

  const filter_ekf_config_t ekf_config = {
    .angle_process_noise_rad2 = APP_EKF_ANGLE_PROCESS_NOISE_RAD2,
    .bias_process_noise_radps2 = APP_EKF_BIAS_PROCESS_NOISE_RADPS2,
    .accel_measurement_noise_mps2_2 =
      APP_EKF_ACCEL_MEASUREMENT_NOISE_MPS2_2,
    .initial_angle_variance_rad2 =
      APP_EKF_INITIAL_ANGLE_VARIANCE_RAD2,
    .initial_bias_variance_radps2 =
      APP_EKF_INITIAL_BIAS_VARIANCE_RADPS2,
    .accel_norm_min_mps2 = APP_EKF_ACCEL_NORM_MIN_MPS2,
    .accel_norm_max_mps2 = APP_EKF_ACCEL_NORM_MAX_MPS2,
    .gyro_limit_radps = APP_EKF_GYRO_LIMIT_RADPS,
    .bias_limit_radps = APP_EKF_BIAS_LIMIT_RADPS,
    .min_dt_s = APP_EKF_MIN_DT_S,
    .max_dt_s = APP_EKF_MAX_DT_S,
  };
  if (!Filter_Ekf_Init(&s_ekf, "ekf", &ekf_config)) {
    return PLATFORM_STATUS_ERROR;
  }

  filter_base_t *filter_stages[] = {
    &s_calibration_filter.filter_base,
    &s_ekf.filter_base,
  };
  Filter_Chain_Init(
    &s_filter_chain,
    "imu_chain",
    filter_stages,
    2u
  );

  uart_link_config_t link_config = {
    .name = "uart1_link",
    .instance = APP_UART_INSTANCE,
    .rx_buffer = s_uart_rx_buffer,
    .rx_capacity_bytes = sizeof(s_uart_rx_buffer),
    .line_buffer = s_uart_line_buffer,
    .line_capacity_bytes = sizeof(s_uart_line_buffer),
    .line_handler = app_fastchannel_handleline,
    .line_context = NULL,
  };
  platform_status_t status = Dev_UARTLink_Init(&s_uart_link, &link_config);
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }

  uart_base_t *uart_view = Dev_UARTLink_GetUartView(&s_uart_link);
  Dev_JustFloat_UART_Init(
    &s_justfloat,
    "justfloat",
    uart_view,
    (uint8_t)APP_TELEMETRY_CHANNEL_COUNT
  );

  mpu6050_config_t imu_config = {
    .i2c = APP_IMU_I2C_INSTANCE,
    .address = (uint8_t)APP_IMU_I2C_ADDRESS,
    .dlpf = APP_IMU_DLPF,
    .gyro_range = APP_IMU_GYRO_RANGE,
    .accel_range = APP_IMU_ACCEL_RANGE,
    .sample_rate_hz = APP_IMU_SAMPLE_RATE_HZ,
    .calibration_samples = 0u,
    .i2c_timeout_ms = APP_IMU_I2C_TIMEOUT_MS,
    .axis_source = {0u, 1u, 2u},
    .axis_sign = {1, 1, 1},
  };
  status = Dev_MPU6050_I2C_Init(
    &s_imu,
    "mpu6050",
    &imu_config,
    &s_filter_chain.filter_base
  );
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }
  Dev_IMU_Base_Start(&s_imu.imu_base);

  status = App_ImuSession_Init(
    &s_imu.imu_base,
    &s_ekf,
    &s_calibration_filter
  );
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }

  status = Dev_UARTLink_Start(&s_uart_link);
  if (status != PLATFORM_STATUS_OK) {
    SYS_LOG_WARN("UART", "receive start deferred");
  }

  App_FastChannelTelemetry_Init(&s_telemetry);
  s_command_status = APP_FAST_CHANNEL_COMMAND_IDLE;
  s_initialized = true;
  SYS_LOG_INFO("APP", "fast channel initialized");
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 推进一次 100 Hz IMU 标定和姿态会话。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 只允许 ImuEkfTask 在 10 ms 周期上下文中调用。
 * @post IMU 采样、标定、参考姿态和一致快照均已推进。
 * @note 可能阻塞等待 I2C 或 Flash；不分配内存。
 */
void App_FastChannel_ProcessImu(uint32_t now_ms) {
  if (!s_initialized) {
    return;
  }
  App_ImuSession_Process(now_ms);
}

/**
 * @brief 消费 UART 命令并发送一帧 50 Hz JustFloat。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 只允许 VofaTask 在 20 ms 周期上下文中调用。
 * @post 已处理 RX 文本行、更新 EKF 参数并尝试非阻塞发送 32 通道遥测。
 * @note 不等待 DMA、不分配内存。
 */
void App_FastChannel_ProcessTelemetry(uint32_t now_ms) {
  if (!s_initialized) {
    return;
  }

  Dev_UARTLink_Process(&s_uart_link, now_ms);

  app_fast_channel_snapshot_t snapshot;
  App_ImuSession_GetSnapshot(&snapshot);

  uart_link_status_t link_status;
  Dev_UARTLink_GetStatus(&s_uart_link, &link_status);

  App_FastChannelTelemetry_Process(
    &s_telemetry,
    &s_justfloat.base,
    &snapshot,
    &link_status,
    s_command_status,
    now_ms
  );
}

/**
 * @brief 复制最近一次 IMU 会话快照。
 * @param snapshot 调用方输出存储，不得为 NULL。
 * @pre snapshot 可写且不与内部存储重叠。
 * @post 获得一致的 IMU、EKF、标定和性能统计快照。
 * @note 使用短临界区复制；不分配内存。
 */
void App_FastChannel_GetSnapshot(
  app_fast_channel_snapshot_t *snapshot
) {
  assert(snapshot);
  App_ImuSession_GetSnapshot(snapshot);
}

/**
 * @brief 更新 ImuEkfTask 性能统计。
 * @param metrics 非空统计对象，只在调用期间读取。
 * @pre 只允许 ImuEkfTask 调用。
 * @post 后续公开快照包含新统计。
 * @note 不分配内存。
 */
void App_FastChannel_ReportImuMetrics(
  const app_task_metrics_t *metrics
) {
  assert(metrics);
  App_ImuSession_ReportMetrics(metrics);
}

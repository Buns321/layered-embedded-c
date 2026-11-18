# F103 IMU 应用示例

> 状态：示例
> 架构规则：[框架契约](../framework-contract.md)
> 板级工程：
> [F103 速通任务一](../../Boards/f103c8t6-fast-channel-task-1/README.md)

## Core/Board

`Core/Board` 负责：

- 配置 I2C1、MPU6050 地址、量程、采样率和超时。
- 定义 MPU6050、IMU、六轴校正滤波器、EKF、Filter Chain 和 UART Link。
- 配置 USART1、DMA、JustFloat 和板级缓冲。
- 创建并初始化 `system_record_store_t`。
- 配置状态 LED 和提示硬件的逻辑服务。
- 填写 `app_fast_channel_dependencies_t`。

依赖示例：

```c
typedef struct app_fast_channel_dependencies_s {
  imu_base_t *imu_h;
  filter_imu_calibration_t *calibration_filter_h;
  filter_ekf_t *ekf_h;
  uart_base_t *command_uart_h;
  telemetry_base_t *telemetry_h;
  led_base_t *status_led_h;
  system_record_store_t *record_store_h;
} app_fast_channel_dependencies_t;
```

入口：

```c
platform_status_t Core_Board_Init(
  app_fast_channel_dependencies_t *dependencies
);
```

## App

App 负责：

- 初始姿态检查、陀螺零偏和六面标定状态机。
- EKF 参数命令和在线参数提交。
- 校准记录版本、字段布局、CRC、默认值和恢复策略。
- 32 通道遥测、状态快照和 LED 提示策略。
- 标定失败和 Flash 写入失败的业务处理。

入口：

```c
bool App_FastChannel_Init(
  const app_fast_channel_dependencies_t *dependencies
);
```

## RTOS

| 任务 | 周期 | App 入口 |
| --- | ---: | --- |
| ImuEkfTask | 10 ms | `App_FastChannel_ProcessImu(now_ms)` |
| VofaTask | 20 ms | `App_FastChannel_ProcessTelemetry(now_ms)` |

RTOS 负责静态任务内存、优先级、周期和任务性能采样。

## Flash 记录存储

- Core/Board 拥有 `system_record_store_t` 和 Flash 物理配置。
- Core/Board 使用 App 公共 Magic 配置初始化记录存储。
- App 接收初始化后的 `system_record_store_t *`。
- App 负责记录版本、字段布局、CRC、默认值和恢复策略。
- App 调用 Load、Commit、Apply 等记录业务操作。

## 初始化

```c
app_fast_channel_dependencies_t dependencies;

if (Core_Board_Init(&dependencies) != PLATFORM_STATUS_OK) {
  Error_Handler();
}

if (!App_FastChannel_Init(&dependencies)) {
  Error_Handler();
}

osKernelInitialize();

if (!RTOS_Init()) {
  Error_Handler();
}

osKernelStart();
```

当前迁移任务见 [迁移路线](../todo.md)。

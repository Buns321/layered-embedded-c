# IMU 与 MPU6050

## 1. 职责边界

具体 IMU 驱动只负责：

- 从 UART、I2C 等硬件链路采集原始数据。
- 完成单位换算、轴映射和设备侧标定。
- 用本次实际更新的通道构造 `filter_sample_t`。
- 通过 `Dev_IMU_Base_SubmitSample()` 提交原始帧。

`imu_base_t` 统一负责：

- 调用初始化时注册的 `filter_base_t`。
- 合并 WHT101 的异步角度/角速度帧。
- 合并滤波器新增或清除的通道。
- 保存 raw/filtered 快照、逐通道时间和序号。
- 在任务与任务之间提供一致快照复制。

上层只读取滤波后的 `imu_sample_t`，不直接解释具体 IMU 寄存器。

## 2. 通道 schema

| 通道 | 含义 | 单位 |
| --- | --- | --- |
| 0 | yaw | deg |
| 1 | roll | deg |
| 2 | pitch | deg |
| 3 | gyro X | deg/s |
| 4 | gyro Y | deg/s |
| 5 | gyro Z | deg/s |
| 6 | accel X | m/s² |
| 7 | accel Y | m/s² |
| 8 | accel Z | m/s² |

具体驱动只提交自己拥有的通道。滤波输出可以清除已有通道或增加姿态通道，
但 `time_ms` 必须沿用输入帧。

## 3. WHT101

WHT101 注册 `filter_passthrough_t`：

- 角度帧提交 yaw。
- 陀螺仪帧提交 gyro-Z。
- 两个通道保留各自的更新时间。
- 不支持的 accel/roll/pitch 通道不出现在 `valid_mask` 中。

## 4. MPU6050

MPU6050 通过 Platform I2C 使用 7 位地址和逻辑实例。具体 I2C 外设、SCL/SDA、
总线频率与 NVIC 由 CubeMX 配置，驱动不包含引脚信息。

初始化配置包括：

- DLPF、陀螺量程、加速度量程和采样率。
- AD0 决定的 `0x68` 或 `0x69` 地址。
- I2C 超时。
- 静止零偏标定样本数。
- 芯片轴到车体轴的 `axis_source` 和 `axis_sign`。

采集过程：

1. `WHO_AM_I` 校验。
2. 软复位并等待。
3. 配置寄存器并回读关键字段。
4. 静止采集陀螺零偏，同时检查加速度模长和角速度方差。
5. 每次从 `0x3B` 连续读取 14 字节。
6. 输出 accel/gyro/temperature 原始工程单位快照。
7. 将 accel/gyro 六通道提交给已注册滤波器。

首版不加载 DMP，也不使用 FIFO。INT 引脚可以不连接，采样由
`Dev_IMU_Base_Process()` 按配置周期轮询。

## 5. 构建接线

正式目标的 MPU6050 源文件随 `PLATFORM_BUILD_I2C` 启用：

- CubeMX 生成实际 `i2c.c` / `i2c.h`、引脚、时钟和 handle。
- CMake 打开 `PLATFORM_BUILD_I2C` 并定义至少一个
  `PLATFORM_ENABLE_I2Cx=1`。
- App 装配层静态定义 MPU6050、滤波器和采样任务归属。

F103 与 F407 使用同一驱动源码，不包含 MCU 家族条件编译。

## 6. yaw 约束

MPU6050 没有磁力计。只有注册滤波器提供 yaw 时，`IMU_CHANNEL_YAW` 才会有效；
纯陀螺积分得到的是相对 yaw，长期会漂移。需要绝对航向时应增加磁力计或外部航向源。

`Dev_IMU_Base_ZeroYaw()` 只改变对外 yaw 参考，不改变滤波器和原始样本。

## 7. 并发约定

- `process()` 和 `SubmitSample()` 只允许一个传感器任务写。
- `GetSample()`、`GetRawSample()` 和便捷 getter 可以在其他任务读取。
- 快照复制和合并使用 System 临界区保护。
- 复杂滤波在临界区外执行。

## 8. 速通任务一接线

`Boards/f103c8t6-fast-channel-task-1/` 使用 I2C1 轮询 MPU6050，并把
`mpu6050_config_t.calibration_samples` 设为 0。因此：

- `imu_base.raw_sample` 保留轴映射后的原始工程单位六轴数据。
- `Dev_MPU6050_I2C_GetRawSample()` 可用于原始曲线和六面标定采样。
- 统一 IMU Filter Chain 固定为 `filter_imu_calibration_t` 后接
  `filter_ekf_t`。
- `imu_base.sample` 和 MPU 驱动提交的滤波帧包含标定后的六轴及 EKF 姿态。

六面标定状态机属于板级 App，不写 MPU6050 寄存器做加速度校准。每个成功面追加
一条 Flash 记录，记录包含陀螺零偏、已完成轴加速度 offset/scale、面掩码和
CRC32。超时或初始 `-Z` 跳过时使用最近一次有效 Flash 记录。

EKF 框架边界仍使用现有 IMU schema：

- 输入陀螺：`deg/s`
- 输出 roll/pitch/yaw：`deg`

`filter_ekf_t` 内部预测/校正、协方差、在线参数和 `Filter_Ekf_GetState()` 使用
`rad`、`rad/s`。当前 EKF 核心为占位实现，只发布零姿态且 `placeholder = true`。

速通任务一在 I2C1 初始化前用 PB6/PB7 执行总线恢复脉冲。上电初始判断和陀螺
零偏采集允许高达 10 deg/s 的未标定原始角速度；零偏应用后，六面流程改用
3 deg/s 的校准后静止门限。

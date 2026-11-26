# 单位与量纲

## 1. 标识符单位后缀

表示物理量或工程量的标识符必须带规范单位后缀。规则适用于参数、结构体字段、局部变量、全局变量和常量；函数名、类型名、状态名、索引、对象或事件计数器和纯协议控制字段不因本规则改名。编码器 `counts` 属于工程量，仍使用 `_counts` 后缀。后缀必须反映实际存储的量纲和倍率，不能只作为注释存在。

| 后缀 | 含义 | 示例 |
| --- | --- | --- |
| `_s` | 秒 | `control_period_s` |
| `_ms` | 毫秒 | `timeout_ms` |
| `_us` | 微秒 | `start_us` |
| `_hz` | 赫兹 | `sample_rate_hz` |
| `_m` | 米 | `wheel_distance_m` |
| `_mm` | 毫米 | `clearance_mm` |
| `_mps` | 米每秒 | `target_speed_mps` |
| `_mps2` | 米每二次方秒 | `accel_mps2` |
| `_mps2_2` | 加速度的平方（方差/协方差） | `accel_measurement_noise_mps2_2` |
| `_rad` | 弧度 | `yaw_rad` |
| `_rad2` | 弧度的平方（方差/协方差） | `angle_variance_rad2` |
| `_radps` | 弧度每秒 | `omega_radps` |
| `_radps2` | 弧度每秒的平方（方差/协方差） | `bias_variance_radps2` |
| `_deg` | 度 | `yaw_deg` |
| `_dps` | 度每秒 | `gyro_dps` |
| `_g` | 标准重力加速度 | `accel_g` |
| `_v` | 伏 | `bus_voltage_v` |
| `_mv` | 毫伏 | `sense_mv` |
| `_a` | 安 | `phase_current_a` |
| `_ma` | 毫安 | `sleep_current_ma` |
| `_kg` | 千克 | `mass_kg` |
| `_duty` | 占空比，量程必须另行声明 | `target_duty` |
| `_counts` | 编码器计数 | `delta_counts` |
| `_bytes` | 字节数 | `received_bytes` |
| `_ratio` | 无量纲比例 | `gear_ratio` |
| `_norm` | 归一化值，范围必须另行声明 | `stick_norm` |

补充约束：

- 标识符保持项目原有大小写规则；小写标识符使用小写后缀，宏使用大写后缀，例如 `timeout_ms` 与 `CAR_DEFAULT_RC_TIMEOUT_MS`。
- `_g` 只表示标准重力加速度；质量统一使用 `_kg`。
- `_mps2` 固定表示 m/s²，不得复用为加速度平方；方差和协方差使用登记过的平方后缀。
- 传感器灵敏度等复合单位使用 `<被测量>_per_<基准单位>` 形式登记，例如 `accel_lsb_per_g`、`gyro_lsb_per_dps`。
- `_h` 只表示对象句柄，不表示亨利。
- 单位后缀不能替代接口契约；单位、范围、符号与调用上下文仍需在公开头文件简明说明。

## 2. 各层单位

| 层 / 对象 | 输入 | 输出 | 说明 |
| --- | --- | --- | --- |
| `BSP_PWM` | `duty`、`max_duty` | CCR | `ccr = ARR * clamped_duty / effective_max`（64 位防溢出） |
| `BSP_Encoder` | — | `counts` | 16/32 位增量 `delta_counts` 与累计 `position_counts` |
| `BSP_I2C` | 7 位地址、字节数、毫秒超时 | 状态码 | 内部自动左移 1 位 |
| `BSP_Flash` | 字节地址/长度 | 状态码 | F4 扇区擦除（16/64/128 KiB），写入须 4 字节对齐 |
| `Motor`（TB6612） | `target_speed_mps` | `duty` | `current_speed_mps = delta_counts / (counts_per_meter * control_period_s)` |
| `Controller` | `target`、`measure`、`dt_s` | `out` | 由 `ctrl_*_cfg_t` 声明目标与输出限幅 |
| `Kinematics`（Omni） | `vx_mps`、`vy_mps`、`wz_radps`、`frame`、`yaw_rad` | 四轮 `out_wheel_mps[4]` | 显式区分 `CHASSIS_FRAME_BODY` 与 `CHASSIS_FRAME_WORLD` |
| `Chassis` twist | `vx_body_mps`、`vy_body_mps`、`wz_radps` | 四轮目标 `m/s` | 底盘控制指令，由 `chassis_omni` 映射 |
| `IMU` | 原始加速度 `m/s²`、角速度 `deg/s`、姿态 `deg` | 滤波后同一单位 | 9 通道样本 + 六轴标定滤波器 `filter_imu_cal` |
| `JoyCon` | ASCII 原始值 `[-100, 100]` | 归一化 `[-1.0, 1.0]` | `left_x_norm`、`left_y_norm`、`right_x_norm`、`right_y_norm` |
| `Filter` | 通道样本、`time_ms` | 滤波后样本 | 支持低通、透传、链式与六轴标定 |
| `BSP_DWT` / `RTOS` | `us` / `ms` | 周期微秒 / 毫秒时间戳 | DWT 忙等短延时；`RTOS_TASK_PERIODIC` 基于 `osDelayUntil` |

## 3. 关键换算

轮速换算：

```text
current_speed_mps = delta_counts / (counts_per_meter * control_period_s)
```

PWM 占空比换算（`effective_max = (max_duty > 0) ? max_duty : (arr + 1)`）：

```text
ccr = (uint32_t)(((uint64_t)arr * clamped_duty) / effective_max)
```

## 4. 常数归属

| 常数 | 归属 | 说明 |
| --- | --- | --- |
| `counts_per_meter` | `User/board_cfg.c` → `car_omni_wheel_cfg_t` | 编码器每米计数，含减速比与轮径 |
| `control_period_s` | `User/board_cfg.c` → `car_omni_cfg_t` | 必须与 `User/rtos_tasks.c` 中控制任务周期一致 |
| `pwm.max_duty` | `User/board_cfg.c` | 执行器量程（F407 `TIM1` 为 `1000`） |
| `theta_rad` / `sign` | `User/board_cfg.c` → `kinematics_omni_cfg_t` | 轮子滚动方向角（`rad`）与安装方向符号（`±1`） |
| `wheel_distance_m` | `User/board_cfg.c` → `car_omni_cfg_t` | 轮心到车体中心距离（`m`） |
| `MPU6050` 轴映射 | 驱动配置 `imu_mpu6050_cfg_t` | 芯片轴到车体轴的映射与零偏标定 |
| 引脚 / `STBY` 分组 | `User/board_cfg.c` | `bsp_gpio_t` 映射 `PC13`/`PE5` 等 |

## 5. 符号与运动学约定

- `yaw_rad` / `wz_radps`：弧度与弧度每秒，逆时针为正；`theta_rad` 以车头 `x` 轴为 `0`，逆时针为正。
- **车体系（`CHASSIS_FRAME_BODY`）**：不依赖 IMU `yaw_rad`，直接令 `vx_body = vx_mps`、`vy_body = vy_mps`。
- **世界系 / 无头模式（`CHASSIS_FRAME_WORLD`）**：先按 `-yaw_rad` 将世界系平动旋至车体系：
  `vx_body = vx * cos(yaw) + vy * sin(yaw)`，`vy_body = -vx * sin(yaw) + vy * cos(yaw)`。
- **单轮出力与 `wheel_sign[i]`**：作用于整轮总输出 `v_i = (vx_body * cos(theta_i) + vy_body * sin(theta_i) + wz * R) * wheel_sign[i]`，正值表示轮子沿设定正方向滚动。
- `duty` 符号由 `motor_tb6612` 决定方向写 `IN1`/`IN2`，切换方向前先置零占空比；短路制动使用 `IN1=IN2=HIGH`。

## 6. 标定流程

1. 架空车轮，固定低占空比或开启闭环读取 `delta_counts`，确认正指令对应正速度方向；反向则设置 `encoder.invert = true` 或调整接线/`wheel_sign`。
2. 用已知位移反推并填写 `counts_per_meter`。
3. 实测轮心距 `wheel_distance_m` 与四轮角度 `theta_rad`，验证平移与自转解算无耦合。
4. 确认 `s_car_cfg.control_period_s`（`0.01f`）与 `s_control_task` 周期（`10U` ms）严格一致。

## 7. 常见错误

- 把 `max_duty` 当速度（`m/s`）传给闭环接口。
- 控制周期常数 `control_period_s` 与 RTOS 周期不一致，导致速度与 PID 积分/微分时间常数失真。
- 同一控制周期内重复调用 `BSP_Encoder_GetDelta()` 导致第二次读取得到 `0`。
- 在世界系模式下忘记确认 WHT101 归零完成或混淆车体前方与世界零度方向。

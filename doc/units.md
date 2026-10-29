# 单位与量纲

## 1. 标识符单位后缀

表示物理量或工程量的标识符必须带规范单位后缀。规则适用于参数、结构体字段、
局部变量、全局变量和常量；函数名、类型名、状态名、索引、对象或事件计数器
和纯协议控制字段不因本规则改名。编码器 `counts` 属于工程量，仍使用 `_counts`
后缀。后缀必须反映实际存储的量纲和倍率，不能只作为注释存在。

下表是项目已登记的规范后缀。表中单位用于声明量纲，实际标识符必须按本表采用
统一缩写；单位后必须以独立下划线 token 接入语义主体，新增单位、倍率或复合
单位必须先登记到本表。

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

- 标识符保持项目原有大小写规则；小写标识符使用小写后缀，宏使用大写后缀，
  例如 `timeout_ms` 与 `APP_CAR_TIMEOUT_MS`。
- `_g` 只表示标准重力加速度；质量统一使用 `_kg`。
- `_mps2` 固定表示 m/s²，不得复用为加速度平方；方差和协方差使用登记过的
  平方后缀。
- 传感器灵敏度等复合单位使用 `<被测量>_per_<基准单位>` 形式登记，例如
  `accel_lsb_per_g`、`gyro_lsb_per_dps`。
- `_h` 只表示对象句柄，不表示亨利。
- 单位后缀不能替代 Doxygen 契约；单位、范围、符号、所有权和调用上下文仍需
  在公开接口处完整说明。

本规则适用于新增和修改的标识符。存量不合规标识符按模块单独迁移，不得把
单位后缀迁移混入无关功能改动。

## 2. 各层单位

下表中的 `duty`、`target`、`measure`、`v`、`dir`、`omega` 等是量纲说明简写，
不代表 C 标识符名；实际标识符必须按第 1 节带规范单位后缀。第 2–7 节保留的
旧标识符用于描述存量接口，迁移时按第 1 节改名。

| 层 / 对象 | 输入 | 输出 | 说明 |
| --- | --- | --- | --- |
| Platform PWM | `duty`, `max_duty` | CCR | `ccr = ARR * duty / max_duty` |
| Platform Encoder | — | counts | 原始计数器 / 累计位置 |
| Platform I2C | 7 位地址、字节数、毫秒超时 | 字节数 | IT/DMA 超时由设备层管理 |
| Platform SPI | 字节数；16 位模式要求偶数长度 | 字节数 | HAL 帧数在实现内部换算 |
| Platform ADC | 转换次数 | 原始 ADC 码值 | DMA `length` 是转换次数 |
| Platform DAC | 原始样本 | 原始模拟码值 | DMA `length` 是样本数 |
| Platform CAN | 经典帧、毫秒超时 | 经典帧 | DLC 为 `0..8` |
| Platform RTC | 二进制日期/时间 | 二进制日期/时间 | 年份 2000..2099 |
| Platform Flash | 字节地址/长度 | 字节数 | 写入要求 4 字节对齐 |
| Encoder | — | delta counts | 自上次读取以来的增量 |
| Motor | target m/s | duty | `current = delta / (counts_per_meter * period)` |
| Controller | target/measure m/s, dt s | duty | 输出范围由调用方/电机上限约束 |
| Policy (omni) | v m/s, dir rad, yaw rad, omega rad/s | 四轮 m/s | 世界系平动 + 独立自转 |
| Chassis (omni) | v m/s, dir rad, omega rad/s | 四轮目标 m/s | 只下发目标 |
| IMU | 原始加速度 m/s²、陀螺仪 deg/s、姿态 deg | 滤波后使用相同单位 | 通道 0–8 固定 |
| IMU 六轴校正 | offset m/s²、scale 无量纲、gyro bias deg/s | 标定后同一框架单位 | 原始通道由 `imu_base.raw_sample` 保留 |
| EKF | 标定后六轴；核心内部 rad、rad/s | 状态 rad、rad/s；框架姿态 deg | 在线参数使用 SI |
| JoyCon | ASCII 原始量 `[-100,100]` | 归一化 `[-1,1]` | 设备层不解释底盘单位 |
| Filter | 用户定义通道 | 用户定义通道 | `time_ms` 毫秒 |
| System delay | us / ms | — | DWT / `osDelay` |

## 3. 关键换算

轮速：

```
current_speed[m/s] = delta[counts] / (counts_per_meter[counts/m] * control_period[s])
```

反推目标增量（需要时）：

```
delta[counts] = target_speed[m/s] * counts_per_meter[counts/m] * control_period[s]
```

PWM：

```
ccr = ARR * duty / max_duty
```

`Platform_PWM_SetDuty` 内部使用 64 位中间量，避免乘法溢出。

SPI：

```
hal_size[frames] = 8-bit ? size[bytes] : size[bytes] / 2
```

ADC 与 DAC：

```
ADC DMA length = 转换次数
DAC DMA length = 样本数
```

CAN：

```
标准标识符范围 = 0..0x7FF
扩展标识符范围 = 0..0x1FFFFFFF
DLC 范围 = 0..8 字节
```

RTC：

```
年份范围 = 2000..2099
星期范围 = 1..7（周一到周日）
```

Flash：

```
F407 擦除单元 = 扇区（16 KiB、64 KiB 或 128 KiB）
F103 擦除单元 = 页（1 KiB）
Platform_FLASH_Write 对齐要求 = 4 字节
```

## 4. 常数归属

| 常数 | 归属 | 说明 |
| --- | --- | --- |
| `counts_per_meter` | 电机 / 传动 | 编码器每米计数，包含减速比与轮径 |
| `control_period` | 电机 / 控制任务 | 必须与任务真实周期一致，单位 s |
| `max_duty` | 电机 | 执行器量程，通常等于 PWM ARR |
| `wheel_theta[4]` | policy | 轮子滚动方向与车头夹角，单位 rad |
| `wheel_sign[4]` | policy | 安装方向符号，通常 ±1 |
| `wheel_distance` | policy | 轮子到车体中心距离，单位 m |
| MPU6050 `axis_source/sign` | 板级 App / MPU6050 配置 | 芯片轴到车体轴的映射 |
| RTC 唤醒计数 | Platform RTC | 所选 RTC 唤醒时钟的 tick 数 |
| Flash 擦除粒度 | Platform Flash | F4 sector / F1 page，单位 byte |
| 引脚 / STBY 分组 | 板级 App 设备装配 | 由各板 App 持有并注入具体设备 |

## 5. 符号约定

- `dir` / `yaw`：弧度，逆时针为正。
- `omega`：rad/s，逆时针为正。
- 轮速：正值表示该轮“向前”滚动；`wheel_sign` 负责抵消安装方向。
- duty：符号表示方向；`motor_tb6612` 根据符号写 IN1/IN2。
- `yaw` 由 IMU 提供，世界系平动方向通过 `dir - (wheel_theta + yaw)` 落到轮子上。
- CAN 标识符只表示协议 ID，不包含 IDE/RTR 位；IDE/RTR 使用独立字段。
- RTC 时间字段统一为二进制，不使用 BCD。

## 6. 标定流程

1. 固定一个占空比，让轮子空载稳态转动。
2. 读取一段时间内的编码器增量，计算 `counts/period`。
3. 用实测轮速反推 `counts_per_meter`。
4. 检查正方向：正 duty 应产生正的 `current_speed`；否则调整 `wheel_sign` 或接线。
5. 标定 `wheel_theta`、`wheel_distance`，验证 omni 解算方向。
6. 确认 `control_period` 与 `osDelayUntil` 周期一致。

## 7. 常见错误

- 把 `max_duty` 当速度用。
- 控制周期常数与任务周期不一致，导致速度换算错误。
- 同一周期重复调用 `get_delta`，第二次得到 0。
- 把 `dir`（世界系行进方向）和 `yaw`（机身当前朝向）混为一谈。
- 忘记设置 `counts_per_meter`，导致目标速度与实际速度对不上。

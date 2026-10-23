# 单位与量纲

## 1. 各层单位

| 层 / 对象 | 输入 | 输出 | 说明 |
| --- | --- | --- | --- |
| Platform PWM | `duty`, `max_duty` | CCR | `ccr = ARR * duty / max_duty` |
| Platform Encoder | — | counts | 原始计数器 / 累计位置 |
| Platform I2C | 7-bit address, bytes, ms timeout | bytes | IT/DMA 超时由设备层管理 |
| Platform SPI | bytes; 16-bit mode 要求偶数长度 | bytes | HAL 帧数在实现内部换算 |
| Platform ADC | 转换次数 | raw ADC code | DMA `length` 是转换次数 |
| Platform DAC | raw sample | raw analog code | DMA `length` 是样本数 |
| Platform CAN | classic frame, ms timeout | classic frame | DLC 为 `0..8` |
| Platform RTC | binary date/time | binary date/time | 年份 2000..2099 |
| Platform Flash | byte address/length | bytes | 写入要求 4 字节对齐 |
| Encoder | — | delta counts | 自上次读取以来的增量 |
| Motor | target m/s | duty | `current = delta / (counts_per_meter * period)` |
| Controller | target/measure m/s, dt s | duty | 输出范围由调用方/电机上限约束 |
| Policy (omni) | v m/s, dir rad, yaw rad, omega rad/s | 四轮 m/s | 世界系平动 + 独立自转 |
| Chassis (omni) | v m/s, dir rad, omega rad/s | 四轮目标 m/s | 只下发目标 |
| Filter | 用户定义通道 | 用户定义通道 | `time_ms` 毫秒 |
| System delay | us / ms | — | DWT / `osDelay` |

## 2. 关键换算

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
ADC DMA length = number_of_conversions
DAC DMA length = number_of_samples
```

CAN：

```
standard id range = 0..0x7FF
extended id range = 0..0x1FFFFFFF
dlc range = 0..8 bytes
```

RTC：

```
year range = 2000..2099
weekday range = 1..7 (Monday..Sunday)
```

Flash：

```
F407 erase unit = sector (16 KiB, 64 KiB or 128 KiB)
F103 erase unit = page (1 KiB)
Platform_FLASH_Write alignment = 4 bytes
```

## 3. 常数归属

| 常数 | 归属 | 说明 |
| --- | --- | --- |
| `counts_per_meter` | 电机 / 传动 | 编码器每米计数，包含减速比与轮径 |
| `control_period` | 电机 / 控制任务 | 必须与任务真实周期一致，单位 s |
| `max_duty` | 电机 | 执行器量程，通常等于 PWM ARR |
| `wheel_theta[4]` | policy | 轮子滚动方向与车头夹角，单位 rad |
| `wheel_sign[4]` | policy | 安装方向符号，通常 ±1 |
| `wheel_distance` | policy | 轮子到车体中心距离，单位 m |
| RTC 唤醒计数 | Platform RTC | 所选 RTC 唤醒时钟的 tick 数 |
| Flash 擦除粒度 | Platform Flash | F4 sector / F1 page，单位 byte |
| 引脚 / STBY 分组 | Board | 待集中到 Board 层 |

## 4. 符号约定

- `dir` / `yaw`：弧度，逆时针为正。
- `omega`：rad/s，逆时针为正。
- 轮速：正值表示该轮“向前”滚动；`wheel_sign` 负责抵消安装方向。
- duty：符号表示方向；`motor_tb6612` 根据符号写 IN1/IN2。
- `yaw` 由 IMU 提供，世界系平动方向通过 `dir - (wheel_theta + yaw)` 落到轮子上。
- CAN 标识符只表示协议 ID，不包含 IDE/RTR 位；IDE/RTR 使用独立字段。
- RTC 时间字段统一为二进制，不使用 BCD。

## 5. 标定流程

1. 固定一个占空比，让轮子空载稳态转动。
2. 读取一段时间内的编码器增量，计算 `counts/period`。
3. 用实测轮速反推 `counts_per_meter`。
4. 检查正方向：正 duty 应产生正的 `current_speed`；否则调整 `wheel_sign` 或接线。
5. 标定 `wheel_theta`、`wheel_distance`，验证 omni 解算方向。
6. 确认 `control_period` 与 `osDelayUntil` 周期一致。

## 6. 常见错误

- 把 `max_duty` 当速度用。
- 控制周期常数与任务周期不一致，导致速度换算错误。
- 同一周期重复调用 `get_delta`，第二次得到 0。
- 把 `dir`（世界系行进方向）和 `yaw`（机身当前朝向）混为一谈。
- 忘记设置 `counts_per_meter`，导致目标速度与实际速度对不上。

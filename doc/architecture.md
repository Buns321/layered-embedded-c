# 架构说明

## 1. 目标

- 上层代码只依赖接口，不依赖具体芯片或具体驱动。
- 控制律、运动策略、滤波器可以替换，尽量保持纯函数或明确的状态机。
- 所有对象静态分配，不使用动态内存。
- 单位、所有权、调用上下文在接口处写清楚。

## 2. 分层

```
App / RTOS
    │
    ├── Chassis ── Policy
    │      ├── Motor ── Controller
    │      │      └── Encoder
    │      └── IMU
    │
    ├── Filter
    └── System

Platform
    │
HAL / CMSIS / FreeRTOS
```

依赖方向是上层 → 下层，下层不反向依赖上层。`DeviceDrivers` 的具体驱动可以依赖
base 与 Platform；base 只依赖 System 和标准库。

## 3. C 对象模型

- `xxx_base_t` 是接口，不是可以独立实例化的对象。
- 具体类型把 base 放在结构体第一个成员。
- 具体 init 负责绑定静态 ops 表，并初始化自己的字段。
- 回调里通过 `(具体类型 *)motor_h` 等角色句柄还原具体对象。
- 上层通过 `<role>_h` 持有 `xxx_base_t *` 视图，例如
  `motor_base_t *motor_h = &motor.motor_base`。
- 不能复制 base 对象；复制会切片，回调强转后会读到越界数据。

## 4. 各层职责

### Platform

- 用功能独立的枚举身份（PWM、Encoder、UART 等）替代 HAL handle。
- `platform_config.h` 在编译期声明可用实例，未启用实例不进入公共枚举。
- CubeMX/Core 独占引脚、时钟、DMA、NVIC 和 `MX_*_Init()`。
- Platform 只做运行期启动、传输、校准、状态读取和回调分发。
- 每个平台实现内部做私有 handle 映射，非法枚举直接断言。
- 对外只暴露不依赖 HAL 类型的枚举、`platform_status_t` 和回调契约。
- I2C、SPI、UART、ADC 和 DAC 的 IT/DMA 缓冲区在完成、错误或 abort 前
  由调用方保持有效。
- 跨系列公共校验、策略和状态机位于 `Platform/Src/common/`；HAL handle、
  时钟和位宽适配位于 `Platform/Src/stm32f4/` 或 `Platform/Src/stm32f1/`。
  `Boards/f407vet6-omni-car/` 目标选用 F4 backend，
  `Boards/f103c8t6-platform-test/` 目标选用 F1 backend。
- F407 已实现 GPIO、PWM、Encoder、USART1–6、EXTI、I2C1–3、SPI1–3、
  ADC1–3、DAC、CAN1–2、RTC、Flash、CRC、RNG。
- F103C8T6 已实现 GPIO、PWM、Encoder、USART1–3、EXTI、I2C1–2、SPI1–2、
  ADC1–2、CAN1、RTC、Flash、CRC；板级实际实例仍由 `.ioc` 和 CMake
  开关选择。

### DeviceDrivers

- `Base/` 放抽象接口：`led_base_t`、`motor_base_t`、`encoder_base_t`、
  `uart_base_t`、`imu_base_t`、`joycon_base_t`、`chassis_base_t`、
  `telemetry_base_t`。
- `Inc/`、`Src/` 放具体实现：`led_gpio_t`、`led_pwm_t`、`motor_tb6612_t`、
  `encoder_tim_t`、`uart_port_t`、`imu_wht101_uart_t`、
  `mpu6050_i2c_t`、`chassis_omni_t`、
  `firewater_uart_t`、`justfloat_uart_t`、`joycon_uart_t`。
- 电机是闭环设备：目标速度、编码器、控制律、执行器都在 `motor_base` 里汇合。
- UART 由 `uart_base_t` 抽象，`uart_port_t` 负责桥接 Platform 回调。
- IMU 的 `start` / `process` / `get_status` 由具体实现提供；具体驱动只提交
  带时间戳的原始通道帧，`imu_base_t` 统一调用已注册滤波器并发布结果。
- WHT101 使用单字节中断接收、环形队列和非阻塞配置状态机，并通常注册
  `filter_passthrough_t`；MPU6050 通过 Platform I2C 轮询采样、校准并提交六轴数据。
- `telemetry_base_t` 定义固定多通道样本和非阻塞整帧发送契约；
  `firewater_uart_t` 与 `justfloat_uart_t` 是具体协议设备，各自持有发送缓冲。
- `uart_link_t` 取得一个物理 UART 的唯一回调所有权，使用 System 字节环负责 RX、
  自动重挂、文本行组帧和虚拟 UART 转发；它不解析业务命令。

### Controller

- 轮级 SISO 接口：

```c
controller_status_t update(
  controller_base_t *controller_h,
  float target,
  float measure,
  float dt,
  float *out
);
```

- `target` / `measure` 是当前槽位的物理量（轮速环为 m/s）。
- `dt` 单位秒，必须大于 0。
- 上述名称是存量接口；新增或修改接口按单位后缀规则使用 `target_mps`、
  `measure_mps` 和 `dt_s`。
- `out` 是执行器量（轮速环为 duty）。
- 返回 `OK` / `SATURATED` / `ERROR`。
- 线性控制律是开环映射加限幅/步长/摩擦补偿。
- `controller_pid_t` 是位置式 PID，微分作用于测量值，包含微分低通、积分限幅
  和条件积分抗饱和。
- `controller_compose_t` 把多个子控制器并联求和。

### Policy

- 独立于 Controller 的 MIMO 运动解算层。
- 按底盘种类拆分接口，例如 `policy_omni_base_t` 的
  `solve(v, dir, yaw, omega, out[4])`。
- 底盘 init 接收对应的 policy base 类型，类型不匹配在编译期被挡住。
- 当前实现 `policy_omni_kinematics_t`，持有安装角、方向符号、轮距。

### Chassis

- `chassis_base_t` 只承载生命周期：`update` / `stop` / `estop`。
- 具体底盘内嵌 base，持有自己的执行器、IMU、policy 和命令。
- `chassis_omni_t` 持有四个 `motor_base_t *`、`imu_base_t *`、
  `policy_omni_base_t *`。
- `Chassis_Omni_SetMotion(v, dir, omega)` 只保存命令。
- `Chassis_Base_Update(now_ms)` 通过一次 IMU 快照检查 yaw 通道是否有效且未超时，
  将 yaw 转为弧度后调用 policy，再向四个电机写目标速度；
  它不运行电机闭环。IMU 无效或过期时锁存制动。
- `Chassis_Base_Stop` 清零目标并调用电机制动；
  制动状态会保持到显式重新使能。`Chassis_Base_EStop` 直接禁用四个电机。

### Filter

- `filter_sample_t` 固定 16 通道，包含 `value[]`、`valid_mask`、`time_ms`。
- `Filter_Base_Update` 先把输入全量复制到输出，再调用具体滤波器的 `update`。
- IMU 通道 0–8 固定为 yaw、roll、pitch、gyro XYZ、accel XYZ；具体驱动只置位
  本帧更新的通道，`imu_base_t` 合并异步帧并保存逐通道时间戳。
- MPU6050 首版提交 accel/gyro 六通道；yaw/roll/pitch 由后续注册的姿态滤波
  或 Kalman 输出，驱动本身不内置滤波算法。
- 低通、透传、chain、distributor 都依赖这个预复制语义。
- Distributor 的路由输出会覆盖预复制通道；跨路由写同一输出通道在 Init 阶段拒绝。
- `filter_imu_calibration_t` 在统一 IMU schema 上应用陀螺零偏和加速度
  offset/scale，原始帧由 `imu_base.raw_sample` 保留。
- `filter_ekf_t` 使用七状态四元数加陀螺零偏接口。框架边界输入为
  `deg/s`、输出姿态为 `deg`，EKF 核心、在线参数和状态读取使用 SI。
- 速通任务一的 EKF 核心当前是占位实现，只发布单位四元数、零姿态和
  `PLACEHOLDER` 状态；占位姿态的 `valid` 必须为 false，不得用于闭环。
- 整层不使用动态内存。

### System

- `system_math.h` 汇总 `SYS_ABS` / `SYS_CLAMP` / `SYS_SIGN` / `SYS_ISFINITE` 等
  C11 `_Generic` 宏。
- `system_delay.h` 用 DWT 做微秒延时；`USE_FREERTOS` 下毫秒延时走 `osDelay`。
- 通过 `STM32F4xx` / `STM32F1xx` 条件编译选择 CMSIS 设备头；DWT 细节保留在 System 内。
- `system_critical.h` 提供任务/ISR 临界区，RTOS 与裸机两套实现。
- `system_fault.h` 捕获 CFSR/HFSR/MMFAR/BFAR 与 MSP/PSP，并提供弱安全钩子。
- `system_log_base.h` 提供日志输出后端接口，`system_log.h` 提供非阻塞日志门面，
  `system_log_rtt.h` 是 SEGGER RTT 具体后端。
- `system_time.h` 用 DWT CYCCNT 提供周期/微秒差值和任务执行时间换算。
- `system_time_window.h` 提供无符号毫秒截止时间和持续时间判断，统一处理单次
  32 位时基回绕。
- `system_byte_ring.h` 提供调用方静态分配缓冲的单生产者/单消费者字节环。
- `system_angle.h` 提供有限 deg/rad 角度的规范周期归一化。
- `system_text.h` 提供不依赖 locale 的 ASCII 忽略大小写比较、固定容量空白切分
  和严格有限 float 解析；它不解释具体命令或业务参数名。
- `system_record_store_t` 使用双 Flash 页追加带 CRC32 的固定槽位记录；
  参数布局与应用 Magic/版本仍由上层定义。
- `Sys_Critical_Enter()` 在调度器启动前使用 PRIMASK 保存/恢复，启动后使用
  FreeRTOS 任务临界区，避免板级初始化阶段残留 BASEPRI 屏蔽 SysTick。
- SEGGER RTT 原始源码位于 `Middlewares/Third_Party/SEGGER_RTT/`，只有
  `system_log_rtt.c` 可以直接包含其头文件。

## 5. 数据流

### 轮速环

```
Dev_Motor_Base_SetTargetSpeed(target_mps)
    └── motor_base.target_speed = target_mps

Dev_Motor_Base_Update()
    ├── delta = Dev_Encoder_Base_GetDelta()          // counts / period
    ├── current = delta / (counts_per_meter * period) // m/s
    ├── Ctrl_Base_Update(controller, target, current, period, &power)
    ├── power = clamp(power, ±max_duty)
    └── motor_ops.set_power(power)                    // TB6612
```

### 底盘环

```
Chassis_Omni_SetMotion(v, dir, omega)

ControlTask:
    Chassis_Base_Update(&chassis.base, now_ms)
        ├── sample = Dev_IMU_Base_GetSample(imu)
        ├── 拒绝无效或过期的 yaw，然后停机并制动
        ├── yaw_rad = sample.yaw_deg * PI / 180
        ├── Policy_Omni_Base_Solve(policy, v, dir, yaw_rad, omega, wheel_mps[4])
        └── 对每个 i：Dev_Motor_Base_SetTargetSpeed(wheel[i], wheel_mps[i])

    对每个 i：Dev_Motor_Base_Update(wheel[i])
```

底盘只负责解算和下发目标；电机闭环仍然在 `motor_base` 内。

### 遥测环

```
ControlTask / MonitorTask
    └── 组装 telemetry_sample_t

MonitorTask
    └── Dev_Telemetry_Base_Send(...)
          ├── JustFloat: 小端 float 数组 + 固定帧尾
          ├── FireWater: name:value 文本
          └── uart_base_t -> USART2 DMA
```

遥测发送不阻塞控制任务。UART DMA 忙时整帧丢弃，并通过
`telemetry_status_t` 暴露发送、丢弃和错误计数。

### F103 速通任务一

```
MPU6050
    └── imu_base.raw_sample
          └── Filter Chain
                ├── imu_calibration
                └── ekf
                      └── imu_base.sample + EKF 状态接口

ImuEkfTask
    └── app_imu_session
          ├── app_calibration_flow
          │    └── app_calibration_record
          ├── app_attitude_reference
          ├── app_status_indicator
          └── 双缓冲遥测快照

VofaTask
    ├── uart_link 文本命令行
    ├── 32 通道 JustFloat 遥测映射
    └── USART1 RX 参数命令
```

标定前六轴校正参数为恒等变换。每个面成功后由 `Sys_RecordStore_Append()`
追加完整工作记录，回读校验成功后才更新活动参数并双短鸣。超时或进入主程序时
丢弃未成功提交的面进度，恢复最近一次 Flash 有效记录。

USART1 的物理回调由共享 `uart_link_t` 独占，JustFloat 使用该链路转发的虚拟
`uart_base_t`，RX 字节只进入其环形缓冲，
命令解析和 EKF 参数提交在 VofaTask 执行；TX 不插入文本应答。

F103 板级 I2C1 初始化前使用 PB6/PB7 输出 9 个恢复时钟和 STOP，防止 MCU
在事务中被复位后从设备持续拉低 SDA。

## 6. F407 初始化顺序

1. `App_Car_Init()` 初始化 USART1 WHT101、USART2 JustFloat 和 USART3 JoyCon。
2. `Dev_Encoder_TIM_Init` × 4。
3. `Ctrl_PID_Init` × 4。
4. `Dev_Motor_TB6612_Init` × 4，A/B 和 C/D 分别共享 STBY。
5. `Policy_Omni_Kinematics_Init` 和 `Chassis_Omni_Init`。
6. WHT101 请求 10 Hz 输出率，UARTTask 等待静止后写入设备侧 yaw 零点。
7. 归零完成并收到有效 JoyCon 后，才使能四轮闭环。

## 7. F407 任务模型

`Boards/f407vet6-omni-car/Core/Src/freertos.c` 已创建四个 CMSIS-RTOS2 任务：

| 任务 | 栈 | 优先级 | 当前状态 |
| --- | --- | --- | --- |
| defaultTask | 512 B | Normal | 空循环 |
| MonitorTask | 2048 B | Low | 50 Hz 遥测、心跳和故障停机 |
| ControlTask | 4096 B | Realtime | 100 Hz 底盘与四轮 PID |
| UARTTask | 4096 B | Normal | WHT101/JoyCon 协议与归零状态机 |

任务职责：

- **ControlTask**：固定 100 Hz 调用底盘和四轮闭环，维护控制心跳。
- **UARTTask**：处理 WHT101 异步帧、JoyCon ASCII 帧和上电归零状态机。
- **MonitorTask**：以 50 Hz 发送 JustFloat，检查控制心跳和控制律故障，
  负责安全停机。

## 7.1 F103 速通任务一任务模型

| 任务 | 栈 | 优先级 | 职责 |
| --- | --- | --- | --- |
| ImuEkfTask | 3 KB 静态栈 | AboveNormal | 100 Hz 采样、标定、EKF、蜂鸣器和 LED |
| VofaTask | 3 KB CubeMX 栈 | Normal | 50 Hz JustFloat、USART1 命令和状态日志 |

EKF 核心占位期间仍发布零姿态，但状态标记为 `PLACEHOLDER` 且 `valid=false`。
进入主程序前会重置 EKF，避免标定期间的部分校正污染主程序起始状态。

## 8. 所有权与内存

- 所有设备、控制律、policy、chassis 对象都是静态定义。
- base 视图是非 owning 指针，生命周期跟随具体静态对象。
- chassis 只保存 motor / imu / policy 的指针，不复制对象。
- 不使用 `malloc` / `free`。
- F103 速通任务一的 ImuEkfTask 使用静态 TCB 和栈；CubeMX 生成的 defaultTask
  槽位承载 VofaTask，应用对象、记录、环形缓冲和遥测帧均为静态内存。

## 9. 约束与不变量

- 编码器 `get_delta` 每个控制周期只能读一次。
- `control_period` 必须和控制任务的真实周期一致。
- 控制律时间步（存量名 `dt`，新接口 `dt_s`）必须有限且大于 0。
- `max_duty` 是执行器量程；控制律输出最终会被夹紧。
- `SetTargetSpeed` / `SetMotion` 对非有限输入做清零保护。
- ISR 只做入队和重新挂接收；解析和控制逻辑放在任务里。
- HAL 类型不能越过 Platform 层。
- base 必须是具体类型的第一个成员。
- 不使用动态内存。

## 10. 扩展方式

- **新电机**：实现 `motor_ops_t`，内嵌 `motor_base_t`，绑定静态 ops 表。
- **新编码器**：实现 `encoder_ops_t`（`set_zero` / `get_position` / `get_delta` / `resync`）。
- **新控制律**：实现 `controller_ops_t`（`update` / `reset`），内嵌 `controller_base_t`。
- **新 policy**：实现对应底盘的 policy ops，内嵌对应 policy base。
- **新底盘**：内嵌 `chassis_base_t`，持有自己的执行器、IMU、policy，实现 `update` / `stop` / `estop`。
- **新滤波器**：实现 `filter_ops_t`（`update` / `reset`）。
- **新 MCU**：增加 `Platform/Src/<mcu>/`，补家族宏、HAL/CMSIS、启动文件、链接脚本和构建目标。

## 11. 当前状态

已实现：Platform（F4 完整接入，F103 支持外设完整实现）、LED、电机、编码器、UART、
统一 IMU 滤波管线、WHT101、JoyCon UART、MPU6050 I2C 驱动、
Controller 契约 + 线性 + PID + compose、
Policy omni、Chassis omni、Filter 基础设施、FireWater/JustFloat 遥测设备、
IMU 六轴校正、EKF 接口壳层、F103 速通任务一标定与 JustFloat、
System（数学/延时/临界区/Fault/RTT 日志/DWT 时间/双页 Flash 记录）。
F103 速通任务一已实板验证 MPU6050、I2C 总线恢复、USART1、Flash 和 RTT。

待完成或暂缓：F103 板级外设接线与验证、参数标定、
EKF 核心姿态算法、速通任务一完整六面人工翻转与蜂鸣器听测、Platform 新增外设板上验证、
MCU 运行时与安全（剩余：复位原因、PVD/BOR、MPU、SWO/ITM）、
F407 蓝牙/JoyCon/JustFloat 实板验证、RTT 主机侧实测。

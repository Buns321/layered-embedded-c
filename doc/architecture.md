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
- 回调里通过 `(具体类型 *)this` 还原具体对象。
- 上层持有 `xxx_base_t *` 视图，例如 `&motor.motor_base`。
- 不能复制 base 对象；复制会切片，回调强转后会读到越界数据。

## 4. 各层职责

### Platform

- 用功能独立的枚举身份（PWM、Encoder、UART 等）替代 HAL handle。
- `platform_config.h` 在编译期声明可用实例，未启用实例不进入公共枚举。
- CubeMX/Core 独占引脚、时钟、DMA、NVIC 和 `MX_*_Init()`。
- Platform 只做运行期启动、传输、校准、状态读取和回调分发。
- 每个平台实现内部做私有 handle 映射，非法枚举直接断言。
- 对外只暴露 HAL-free 类型、`Platform_StatusTypeDef` 和回调契约。
- UART IT/DMA 缓冲区在完成或错误回调前由调用方保持有效。
- F4 实现位于 `Platform/Src/stm32f4/`，F1 兼容实现位于 `Platform/Src/stm32f1/`；
  当前构建只选用 F4。

### DeviceDrivers

- `Base/` 放抽象接口：`led_base_t`、`motor_base_t`、`encoder_base_t`、
  `usart_base_t`、`imu_base_t`、`chassis_base_t`、`telemetry_base_t`。
- `Inc/`、`Src/` 放具体实现：`led_gpio_t`、`led_pwm_t`、`motor_tb6612_t`、
  `encoder_tim_t`、`usart_port_t`、`imu_wht101_usart_t`、`chassis_omni_t`、
  `firewater_usart_t`、`justfloat_usart_t`。
- 电机是闭环设备：目标速度、编码器、控制律、执行器都在 `motor_base` 里汇合。
- USART 由 `usart_base_t` 抽象，`usart_port_t` 负责桥接 Platform 回调。
- IMU 的 `start` / `process` / `get_status` 由具体实现提供；WHT101 使用
  单字节中断接收、环形队列和非阻塞配置状态机。
- `telemetry_base_t` 定义固定多通道样本和非阻塞整帧发送契约；
  `firewater_usart_t` 与 `justfloat_usart_t` 是具体协议设备，各自持有发送缓冲。

### Controller

- 轮级 SISO 接口：

```c
controller_status_t update(
  controller_base_t *this,
  float target,
  float measure,
  float dt,
  float *out
);
```

- `target` / `measure` 是当前槽位的物理量（轮速环为 m/s）。
- `dt` 单位秒，必须大于 0。
- `out` 是执行器量（轮速环为 duty）。
- 返回 `OK` / `SATURATED` / `ERROR`。
- 线性控制律是开环映射加限幅/步长/摩擦补偿；PID 尚未实现。
- `controller_compose_t` 把多个子控制器并联求和。

### Policy

- 独立于 Controller 的 MIMO 运动解算层。
- 按底盘种类拆分接口，例如 `omni_policy_base_t` 的
  `solve(v, dir, yaw, omega, out[4])`。
- 底盘 init 接收对应的 policy base 类型，类型不匹配在编译期被挡住。
- 当前实现 `policy_omni_kinematics_t`，持有安装角、方向符号、轮距。

### Chassis

- `chassis_base_t` 只承载生命周期：`update` / `stop` / `estop`。
- 具体底盘内嵌 base，持有自己的执行器、IMU、policy 和命令。
- `chassis_omni_t` 持有四个 `motor_base_t *`、`imu_base_t *`、
  `omni_policy_base_t *`。
- `Chassis_Omni_SetMotion(v, dir, omega)` 只保存命令。
- `Chassis_Base_Update(now_ms)` 检查 yaw 通道是否有效且未超时，将 IMU
  的角度制 yaw 转为弧度后调用 policy，再向四个电机写目标速度；
  它不运行电机闭环。IMU 无效或过期时锁存制动。
- `Chassis_Base_Stop` 清零目标并调用电机制动；
  制动状态会保持到显式重新使能。`Chassis_Base_EStop` 直接禁用四个电机。

### Filter

- `filter_sample_t` 固定 16 通道，包含 `value[]`、`valid_mask`、`time_ms`。
- `Filter_Base_Update` 先把输入全量复制到输出，再调用具体滤波器的 `update`。
- 低通、透传、chain、distributor 都依赖这个预复制语义。
- Distributor 的路由输出会覆盖预复制通道；跨路由写同一输出通道在 Init 阶段拒绝。
- 整层不使用动态内存。

### System

- `system_math.h` 汇总 `SYS_ABS` / `SYS_CLAMP` / `SYS_SIGN` / `SYS_ISFINITE` 等
  C11 `_Generic` 宏。
- `system_delay.h` 用 DWT 做微秒延时；`USE_FREERTOS` 下毫秒延时走 `osDelay`。
- 通过 `STM32F4xx` / `STM32F1xx` 条件编译选择 CMSIS 设备头；DWT 细节保留在 System 内。
- `system_critical.h` 提供任务/ISR 临界区，RTOS 与裸机两套实现。
- `system_fault.h` 捕获 CFSR/HFSR/MMFAR/BFAR 与 MSP/PSP，并提供弱安全钩子。
- `system_watchdog.h` 提供任务 check-in 与统一喂狗策略；IWDG 寄存器操作
  已下沉到 Platform。
- `system_log_base.h` 提供日志输出后端接口，`system_log.h` 提供非阻塞日志门面，
  `system_log_rtt.h` 是 SEGGER RTT 具体后端。
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
        ├── status = Dev_IMU_Base_GetStatus(imu)
        ├── reject invalid/stale yaw, then stop and brake
        ├── yaw_rad = Dev_IMU_Base_GetYaw(imu) * PI / 180
        ├── Omni_Policy_Base_Solve(policy, v, dir, yaw_rad, omega, wheel_mps[4])
        └── for i: Dev_Motor_Base_SetTargetSpeed(wheel[i], wheel_mps[i])

    for i: Dev_Motor_Base_Update(wheel[i])
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
          └── usart_base_t -> USART2 DMA
```

遥测发送不阻塞控制任务。USART DMA 忙时整帧丢弃，并通过
`telemetry_status_t` 暴露发送、丢弃和错误计数。

## 6. 初始化顺序（计划）

1. `Dev_Encoder_TIM_Init` × 4
2. 控制律初始化（当前用 `Ctrl_Linear_Init`）
3. `Dev_Motor_TB6612_Init` × 4（注入 controller + encoder + 传动常数）
4. `Dev_Motor_Base_Enable` × 4
5. `Dev_USART_Port_Init`（USART1）
6. `Dev_IMU_WHT101_USART_Init` + `Dev_IMU_Base_Start`
7. `Policy_Omni_Kinematics_Init`
8. `Chassis_Omni_Init(..., imu_timeout_ms)`
9. `Dev_USART_Port_Init`（USART2）
10. `Dev_JustFloat_USART_Init` 或 `Dev_FireWater_USART_Init`
11. `Sys_Log_RTT_Init` + `Sys_Log_Init`

当前 `main()` 已调用 `Sys_Delay_Init()`，并在启用 `SYS_LOG_USE_RTT` 时初始化
RTT 日志；设备对象和 `App_Init()` 仍待上层接线。

## 7. 任务模型

`Core/Src/freertos.c` 已创建四个 CMSIS-RTOS2 任务：

| 任务 | 栈 | 优先级 | 当前状态 |
| --- | --- | --- | --- |
| defaultTask | 512 B | Normal | 空循环 |
| MonitorTask | 2048 B | Low | 最小占位（每 100 ms 让一次） |
| ControlTask | 4096 B | Realtime | 弱占位，待实现 |
| USARTTask | 4096 B | Normal | 弱占位，待实现 |

计划中的职责：

- **ControlTask**：固定周期调用底盘 update 和四个电机 update，维护心跳。
- **USARTTask**：调用 `Dev_IMU_Base_Process(&imu, now_ms)`，后续接入命令链路。
- **MonitorTask**：检查心跳、控制律状态和故障，负责安全停机。

## 8. 所有权与内存

- 所有设备、控制律、policy、chassis 对象都是静态定义。
- base 视图是非 owning 指针，生命周期跟随具体静态对象。
- chassis 只保存 motor / imu / policy 的指针，不复制对象。
- 不使用 `malloc` / `free`。

## 9. 约束与不变量

- 编码器 `get_delta` 每个控制周期只能读一次。
- `control_period` 必须和控制任务的真实周期一致。
- 控制律 `dt` 必须有限且大于 0。
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

已实现：Platform（F4）、LED、电机、编码器、USART、IMU、Controller 契约 + 线性 + compose、
Policy omni、Chassis omni、Filter 基础设施、FireWater/JustFloat 遥测设备、
System（数学/延时/临界区/Fault/看门狗/RTT 日志）。

待完成或暂缓：PID、App / RTOS 接线、Board 层、F1 构建目标、参数标定、
Filter 单测 / 统一时间源 / Kalman、Platform 外设扩展、
MCU 运行时与安全（剩余：复位原因、PVD/BOR、MPU、SWO/ITM、参数存储）、
遥测对象与任务接线、USART2 高波特率配置、RTT 主机侧实测。

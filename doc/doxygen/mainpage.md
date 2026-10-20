# layered-embedded-c

面向 STM32F407 的分层嵌入式 C 工程。核心思路是用静态设备模型和 base + 具体实现
把 HAL、设备驱动、控制律、运动策略和上层任务分开。

## 架构

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

## 模块

- **Platform**：HAL 封装，枚举身份 + 静态映射。
- **DeviceDrivers**：LED、电机、编码器、USART、IMU、底盘。
- **Controller**：轮级 SISO 控制律（线性、compose；PID 待实现）。
- **Policy**：底盘级 MIMO 运动策略（当前为 omni 运动学）。
- **Filter**：固定容量多通道滤波。
- **System**：数学、检查、限幅、延时。
- **RTOS**：任务骨架（控制 / 串口任务体待实现）。
- **App**：业务逻辑（待实现）。

## 状态

已完成：Platform（F4）、设备驱动、Controller 契约 + 线性 + compose、
Policy omni、Chassis omni、Filter 基础设施、System。

待完成或暂缓：PID、App / RTOS 接线、Board 层、F1 构建目标、参数标定、
Filter 单测 / 统一时间源 / Kalman。

## 文档

- [架构说明](../architecture.md)
- [单位与量纲](../units.md)
- [编码约定](../conventions.md)
- [板级配置](../board.md)
- [TODO](../todo.md)

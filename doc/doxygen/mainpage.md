# layered-embedded-c

面向 STM32F407 和 STM32F103 的分层嵌入式 C 工程。核心思路是用静态设备模型和
base + 具体实现把 HAL、设备驱动、控制律、运动策略和上层任务分开。

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
- **DeviceDrivers**：LED、电机、编码器、UART、IMU（统一滤波管线、WHT101、
  MPU6050 I2C）、JoyCon、底盘、遥测协议设备。
- **Controller**：轮级 SISO 控制律（线性、PID、compose）。
- **Policy**：底盘级 MIMO 运动策略（当前为 omni 运动学）。
- **Filter**：固定容量多通道滤波、IMU 六轴校正与七状态 EKF 接口壳层。
- **System**：数学、检查、限幅、延时、临界区、Fault、RTT 日志、DWT 时间和
  双页 Flash 记录存储。
- **RTOS**：F407 运动控制任务；F103 速通任务一的 100 Hz ImuEkfTask 和
  50 Hz VofaTask。
- **App**：F407 四轮小车与 F103 MPU6050 自动标定、JustFloat 和参数入口。

## 状态

已完成：Platform（F4 完整接入，F103 基础外设接入）、设备驱动、
Controller 契约 + 线性 + PID + compose、JoyCon UART、
Policy omni、Chassis omni、Filter 基础设施、System。

待完成或暂缓：F407 实板蓝牙/JoyCon/JustFloat 联调、F103 板级外设接线与验证、
参数标定、Filter 单测 / 统一时间源 / Kalman。

## 文档

- [架构说明](../architecture.md)
- [单位与量纲](../units.md)
- [编码约定](../conventions.md)
- [F103 板级工程](../../Boards/f103c8t6-platform-test/README.md)
- [F103 硬件与接口规范](../../Boards/f103c8t6-platform-test/hardware-map.md)
- [F103 速通赛道任务一](../../Boards/f103c8t6-fast-channel-task-1/README.md)
- [F103 速通任务一硬件规范](../../Boards/f103c8t6-fast-channel-task-1/hardware-map.md)
- [F407 板级工程](../../Boards/f407vet6-omni-car/README.md)
- [F407 硬件与接口规范](../../Boards/f407vet6-omni-car/hardware-map.md)
- [TODO](../todo.md)

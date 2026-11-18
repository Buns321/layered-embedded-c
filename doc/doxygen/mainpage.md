# layered-embedded-c

面向嵌入式 C 固件的分层框架。应用开发集中在 App 和 RTOS，Core/Board 负责具体
设备、硬件映射和对象装配，共享框架提供驱动、算法和系统服务。

## 架构

```text
Core / main
    ├── CubeMX 生成代码
    └── Core/Board
           ├── 具体设备与算法对象
           ├── 板级资源映射
           ├── 对象装配
           └── App 依赖注入

App
    └── 业务状态、控制决策、安全策略、命令和遥测

RTOS
    └── CMSIS-RTOS2 任务、周期、优先级、栈和 OS 原语

共享框架
    └── Platform、DeviceDrivers、Controller、Policy、Filter、System
```

## 模块

- **Platform**：HAL 封装，枚举身份 + 静态映射。
- **DeviceDrivers**：设备 base/domain 接口和具体驱动。
- **Controller**：SISO 控制契约和具体控制律。
- **Policy**：MIMO 运动策略。
- **Filter**：固定容量滤波、组合滤波和状态估计接口。
- **System**：数学、检查、限幅、延时、临界区、Fault、RTT 日志、DWT 时间和
  双页 Flash 记录存储。
- **Core/Board**：板级手写装配区，负责具体设备、资源映射、对象装配和依赖注入。
- **RTOS**：CMSIS-RTOS2 任务、周期、优先级、栈和 OS 原语。
- **App**：业务状态机、控制决策、安全策略、命令和输出内容。

## 状态

已完成：Platform 公共契约和多 backend 实现、设备驱动、Controller、
Policy、Filter、System、CMSIS-RTOS2 任务基础和板级集成。

具体产品状态和实板验证进度以对应板级文档为准。

## 文档

- [框架契约](../framework-contract.md)
- [架构说明](../architecture.md)
- [单位与量纲](../units.md)
- [编码约定](../conventions.md)
- [应用示例](../examples/README.md)
- [F103 板级工程](../../Boards/f103c8t6-platform-test/README.md)
- [F103 硬件与接口规范](../../Boards/f103c8t6-platform-test/hardware-map.md)
- [F103 速通赛道任务一](../../Boards/f103c8t6-fast-channel-task-1/README.md)
- [F103 速通任务一硬件规范](../../Boards/f103c8t6-fast-channel-task-1/hardware-map.md)
- [F407 板级工程](../../Boards/f407vet6-omni-car/README.md)
- [F407 硬件与接口规范](../../Boards/f407vet6-omni-car/hardware-map.md)
- [TODO](../todo.md)

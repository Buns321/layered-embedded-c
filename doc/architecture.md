# 架构说明

## 1. 目标

- 应用代码集中在 `App/` 和 `RTOS/`。
- 具体设备、硬件映射和对象装配集中在 `Core/Board/`。
- App 只依赖领域接口、选定算法接口和 System 公共接口。
- RTOS 固定使用 CMSIS-RTOS2，任务代码调用 App 公共入口。
- 控制律、运动策略、滤波器和设备驱动可以独立替换。
- 所有长期对象静态分配。
- 单位、所有权、调用上下文和安全规则在接口处声明。

架构门禁和验收规则见 [框架契约](framework-contract.md)。

## 2. 分层

```text
Core / main
    ├── CubeMX 生成代码
    └── Core/Board
           ├── 具体设备与算法对象
           ├── 板级资源映射
           ├── 对象装配
           └── App 依赖注入

App
    └── 业务状态、控制决策、安全策略、命令和输出

RTOS
    └── CMSIS-RTOS2 任务、周期、优先级、栈和 OS 原语

共享框架
    ├── Platform
    ├── DeviceDrivers
    ├── Controller
    ├── Policy
    ├── Filter
    └── System
```

依赖方向：

```text
Core/Board -> App 公共依赖契约
Core/Board -> Platform / 具体设备 / Controller / Policy / Filter
RTOS -> App 公共入口
RTOS -> CMSIS-RTOS2
App -> DeviceDrivers base/domain / Controller / Policy / Filter / System
```

共享框架向上提供稳定接口。App 和 RTOS 不依赖 Core/Board 私有实现。

## 3. C 对象模型

- `xxx_base_t` 是抽象接口。
- 具体类型把 base 放在结构体第一个成员。
- 具体初始化绑定静态 ops 表并初始化自己的字段。
- 回调通过角色句柄还原具体对象。
- 上层通过 `<role>_h` 持有 base 视图。
- base 对象按引用传递，不复制。
- 长期对象由 Core/Board 或 App 静态持有。

## 4. 各层职责

### Core/Board

- CubeMX 负责时钟、引脚、DMA、NVIC、外设结构和 `MX_*_Init()`。
- `Core/Board/Inc/core_board.h` 提供 `Core_Board_Init()`。
- Core/Board 定义静态具体设备、控制器、策略、滤波器和组合对象。
- Core/Board 完成硬件角色映射、驱动初始化和算法依赖注入。
- Core/Board 填写 `<app_module>_dependencies_t`。
- Core/Board 保持对象生命周期覆盖整个固件运行期。

### App

- App 负责业务状态机、输入语义、安全策略、命令和输出内容。
- App 通过依赖头接收 Core/Board 创建的逻辑服务。
- App 公共初始化入口返回 `bool`。
- App 可以使用 `Sys_Critical_*` 保护短临界区。
- App 配置包含控制参数、算法参数和输出语义。
- App 公共入口由 RTOS 任务按固定周期调用。

### RTOS

- RTOS 固定使用 CMSIS-RTOS2。
- RTOS 负责任务、静态栈、优先级、周期、队列、信号量和事件组。
- RTOS 将 tick 转换为统一毫秒时间并传给 App。
- RTOS 任务循环保持调度和转发职责。
- 超时、状态迁移和业务决策位于 App。

### Platform

- Platform 使用功能独立的枚举身份替代 HAL handle。
- `platform_config.h` 在编译期声明可用实例。
- CubeMX/Core 负责引脚、时钟、DMA、NVIC 和 `MX_*_Init()`。
- Platform 负责运行期启动、传输、校准、状态读取和回调分发。
- 各 backend 负责 HAL handle、时钟、位宽和能力差异。
- 公共契约、参数校验、状态机和错误映射位于 `Platform/Src/common/`。
- Platform 公共头不暴露 HAL 类型。

### DeviceDrivers

- `Base/` 提供设备抽象接口。
- `Inc/`、`Src/` 提供具体设备实现。
- 具体驱动依赖 base 接口和 Platform。
- 设备负责协议、采样、状态和维护自身数据。
- 上层通过 base/domain 接口使用设备。

### Controller

- Controller 提供轮级 SISO 控制接口。
- Controller 接收目标值、测量值、控制周期和输出范围。
- 具体实现包括线性、PID 和组合控制律。
- Controller 输出由执行器量程和后续限幅约束。

### Policy

- Policy 提供底盘级 MIMO 运动解算。
- Policy 接收速度、方向、姿态和角速度命令。
- Policy 输出各执行器目标。
- 具体 Policy 与底盘类型匹配。

### Filter

- Filter 使用固定容量样本、有效位掩码和时间戳。
- Filter base 统一执行输入复制、输出清洗和状态复位。
- 具体滤波器实现自己的配置和更新算法。
- Chain 和 Distributor 组合多个滤波器。
- Filter 层使用静态存储。

### System

- System 提供数学、检查、限幅、符号和单位辅助。
- System 提供统一时间、临界区和时间窗口。
- System 提供故障诊断、日志门面和日志后端接口。
- System 提供静态字节环和记录存储。
- System 公共头保持硬件无关；MCU 实现位于 System backend。

## 5. 初始化与依赖注入

```c
app_<module>_dependencies_t dependencies;

if (Core_Board_Init(&dependencies) != PLATFORM_STATUS_OK) {
  Error_Handler();
}

if (!App_<Module>_Init(&dependencies)) {
  Error_Handler();
}

osKernelInitialize();

if (!RTOS_Init()) {
  Error_Handler();
}

osKernelStart();
```

规则：

- CubeMX 先完成 `MX_*_Init()`。
- Core/Board 在调度器启动前完成静态装配。
- App 依赖由 Core/Board 创建、填写并发布。
- App 只保存依赖指针，不拥有依赖对象。
- RTOS 在 App 初始化成功后创建任务。
- 初始化失败停止启动流程。

## 6. 通用数据流

### 周期业务处理

```text
RTOS Task
    ├── 获取 tick
    ├── 转换为 now_ms
    └── 调用 App_<Module>_Process<Phase>(now_ms)
            ├── 读取逻辑服务
            ├── 推进业务状态
            ├── 发布输出或快照
            └── 更新故障状态
```

### 控制输出

```text
App
    ├── 计算控制目标
    ├── 写入领域控制接口
    └── 推进 Controller/Motor 更新
            ├── 读取编码器
            ├── 计算控制输出
            └── 写入执行器
```

### 遥测

```text
App
    ├── 组装固定通道样本
    ├── 设置有效位和时间戳
    └── 调用 Telemetry base
            └── 具体协议驱动
                    └── UART 输出
```

### 持久记录

```text
Core/Board
    ├── 配置物理存储
    ├── 初始化 system_record_store_t
    └── 注入 App

App
    ├── 定义记录 schema
    ├── 加载和校验记录
    ├── 应用默认值
    └── 提交新记录
```

## 7. 所有权与内存

- 设备、控制器、策略、滤波器和组合对象由 Core/Board 静态持有。
- App 状态和快照由 App 静态持有。
- RTOS 任务控制块和栈使用静态内存。
- base 视图是非 owning 指针。
- App 依赖指针由 Core/Board 保证生命周期。
- 固件不使用动态内存。

## 8. 约束与不变量

- 具体设备初始化和板级映射位于 `Core/Board/`。
- App 依赖通过 `<app_module>_dependencies.h` 声明。
- `Core_Board_Init()` 在调度器启动前完成板级装配。
- RTOS 任务代码使用 CMSIS-RTOS2 和 App 公共入口。
- HAL 类型不越过 Platform 层。
- base 是具体类型的第一个成员。
- 编码器等有状态设备按接口规定消费数据。
- 控制周期参数与 RTOS 实际调度周期一致。
- 时间步参数有限且大于 0。
- 执行器输出经过量程约束。
- ISR 只做入队、通知和重新挂接。
- 跨任务共享数据有明确并发规则。

## 9. 扩展方式

- **新设备**：实现对应 base/domain 接口，在 Core/Board 中装配。
- **新 Controller**：实现 `controller_ops_t` 并内嵌 `controller_base_t`。
- **新 Policy**：实现底盘 policy base 和静态 ops。
- **新 Filter**：实现 `filter_ops_t`，通过 Filter base 构造和配置。
- **新 App 模块**：增加 `<app_module>_dependencies.h`，实现
  `App_<Module>_Init()` 和业务入口。
- **新板卡**：增加 CubeMX 工程、Core/Board 装配、板级 CMake 和资源映射。
- **新 MCU**：增加 Platform backend、System backend、CMSIS-RTOS2 port、
  启动文件、链接脚本和工具链配置。

# 框架契约

> 状态：`生效`
> 作用：定义应用开发、板级适配、跨平台移植和构建验收的共同边界。

## 1. 目标

本框架面向以下开发方式：

- 应用开发者维护 `App/` 和 `RTOS/`。
- `App/` 承载业务状态机、控制决策、安全策略、命令语义和输出内容。
- `RTOS/` 使用 CMSIS-RTOS2，负责任务入口、周期和 OS 原语；CubeMX 配置任务
  属性、优先级、静态 TCB 和静态栈。
- `Core/Board/` 承载具体设备、硬件映射和对象装配。
- 更换板卡时，App 业务逻辑和 RTOS 任务结构保持复用。
- 更换 MCU 时，通过替换 Core 板级装配、Platform backend、System backend、
  CMSIS-RTOS2 port、工具链和启动配置完成移植。
- 固件对象、任务控制和持久数据结构使用静态分配。

跨平台保证以 App 和 RTOS 源码为对象。每块板独立生成自己的固件。

## 2. 开发角色

| 角色 | 代码范围 | 主要职责 |
| --- | --- | --- |
| 应用开发者 | `App/`、`RTOS/` | 业务逻辑、应用状态、安全策略、任务入口和周期 |
| 板级适配者 | `Core/Board/`、CubeMX 配置 | 硬件映射、设备初始化、对象装配、任务配置和依赖注入 |
| 框架维护者 | `Platform/`、`DeviceDrivers/`、`Controller/`、`Policy/`、`Filter/`、`System/` | 通用驱动、算法、服务和平台契约 |
| CubeMX | `Core/` 生成文件 | 时钟、引脚、外设结构、DMA、NVIC、静态任务和生成代码 |

个人项目可以由同一人承担多个角色。代码职责按上述边界维护。

## 3. 架构

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
    └── Platform、DeviceDrivers、Controller、Policy、Filter、System
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

## 4. 代码面

### 4.1 应用开发者

应用开发者维护：

- `App/Inc/`、`App/Src/`
- `RTOS/Inc/`、`RTOS/Src/`

应用开发工作包括：

- 编写业务状态机。
- 处理输入、命令和服务状态。
- 编写安全判断和故障策略。
- 定义输出、遥测和业务快照。
- 配置任务周期、优先级、栈和 CMSIS-RTOS2 对象。

### 4.2 板级适配者

板级适配者维护：

- `Core/Board/Inc/`
- `Core/Board/Src/`
- `Core/*.ioc` 及 CubeMX 生成配置
- 板级 CMake 源文件列表

板级适配工作包括：

- 配置外设、引脚、时钟、DMA 和 NVIC。
- 定义具体设备和算法对象。
- 建立逻辑角色到硬件资源的映射。
- 完成驱动初始化和算法依赖注入。
- 创建并填写 App 依赖。
- 初始化板级存储等公共服务。

## 5. 初始化顺序

```c
/* main.c 的 USER CODE 区域。 */
app_<module>_dependencies_t dependencies;

if (Core_Board_Init(&dependencies) != PLATFORM_STATUS_OK) {
  Error_Handler();
}

if (!App_<Module>_Init(&dependencies)) {
  Error_Handler();
}

osKernelInitialize();

MX_FREERTOS_Init(); /* CubeMX 创建静态任务、TCB 和栈。 */

osKernelStart();
```

初始化规则：

- CubeMX 先完成 `MX_*_Init()`。
- `Core_Board_Init()` 在调度器启动前完成板级装配。
- 板级装配失败时停止启动。
- `App_<Module>_Init()` 只初始化应用状态，保存依赖指针。
- App 初始化入口返回 `bool`：`true` 表示成功，`false` 表示失败。
- App 依赖由 Core/Board 静态对象提供，生命周期覆盖整个固件运行期。
- `MX_FREERTOS_Init()` 在 App 初始化成功后创建 CubeMX 配置的静态任务。
- 所有可失败初始化返回明确成功状态或错误状态。

## 6. App 契约

### 6.1 职责

App 负责：

- 业务状态机和业务定时判断。
- 输入到产品动作的语义转换。
- 应用流程和安全策略。
- 输出 schema、状态快照和日志内容。
- 对应用依赖的合法性检查。

App 公共入口按业务模块命名，例如：

```c
bool App_<Module>_Init(
  const app_<module>_dependencies_t *dependencies
);

void App_<Module>_Process<Phase>(uint32_t now_ms);
```

### 6.2 依赖范围

App 可以使用：

- `DeviceDrivers/Base/` 的 base/domain 接口。
- `Controller/`、`Policy/`、`Filter/` 的公共抽象。
- `System/` 的公共数学、检查、时间、临界区和记录存储接口。
- 应用自己的依赖、配置、状态和快照类型。

App 公共代码使用逻辑服务和领域类型。硬件实例、引脚、通道和具体驱动配置由
Core/Board 转换为 App 依赖后注入。

### 6.3 依赖头命名

每个 App 公共模块拥有独立依赖头：

```text
App/Inc/<app_module>_dependencies.h
```

| 项目 | 规则 |
| --- | --- |
| 文件名 | `<app_module>_dependencies.h` |
| struct 标签 | `<app_module>_dependencies_s` |
| typedef | `<app_module>_dependencies_t` |
| include guard | `LAYERED_EMBEDDED_C_<APP_MODULE>_DEPENDENCIES_H` |

依赖集合属于接口数据，不是对象句柄。依赖参数统一命名为 `dependencies`。
依赖中的长期对象指针使用 `<role>_h`。

通用形态：

```c
typedef struct app_<module>_dependencies_s {
  <domain_service_t> *<role>_h;
} app_<module>_dependencies_t;
```

产品示例位于 `doc/examples/`，不属于本契约。

### 6.4 依赖规则

- Core/Board 创建静态依赖对象并在调度器启动前填写。
- App 初始化后以只读方式使用依赖。
- App 不拥有、释放、替换或重新装配依赖对象。
- 依赖字段使用 base/domain 接口、只读配置和产品允许的具体算法类型。
- 可选能力通过明确的存在标志或能力状态表达。
- App 依赖头可以由 Core/Board 和 App 实现包含。
- RTOS 使用 App 公共入口，不读取 App 依赖内容。

### 6.5 配置归属

| 配置类型 | 内容 | 归属 |
| --- | --- | --- |
| 板级资源 | 外设实例、GPIO、通道、定时器和总线 | `Core/Board` |
| 器件连接 | 地址、轴映射、方向、片选和总线关系 | `Core/Board` |
| 板级物理常数 | 量程、传动比、几何常数、存储地址和页大小 | `Core/Board` |
| 控制参数 | 控制增益、输入死区、目标范围和超时 | App |
| 算法参数 | 滤波、估计和标定参数 | App |
| 输出语义 | 遥测顺序、日志字段、命令含义 | App |
| 任务调度 | 栈、优先级、调度周期和 OS 对象 | RTOS |

配置规则：

- App 配置头使用 App 公共类型。
- Core/Board 配置默认保持私有。
- Core/Board 可以读取 App 明确公开的只读配置。
- 控制算法周期与 RTOS 实际调度周期保持单一事实来源。

### 6.6 时间与并发

- RTOS 向 App 传入统一单调毫秒时间 `now_ms`。
- App 的超时判断使用同一时基。
- App 公共入口声明允许调用的任务上下文。
- 跨任务共享状态定义明确的单写者/单读者关系。
- App 可以使用 `Sys_Critical_*` 保护短临界区。
- CMSIS/FreeRTOS 类型保留在 System 实现和 RTOS 层。

## 7. RTOS 契约

RTOS 固定使用 CMSIS-RTOS2。

RTOS 负责：

- 提供任务入口和固定周期循环。
- 将 CubeMX 创建的任务转发到 App 公共入口。
- 维护周期、截止时间和调度策略。
- 创建队列、信号量、事件组、互斥量和任务通知。
- 生成统一单调时间并传给 App。
- 在任务上下文调用 App 公共入口。

CubeMX 负责任务属性、静态 TCB、静态任务栈和 `osThreadNew()` 调用。

通用任务形态：

| 项目 | 规则 |
| --- | --- |
| 任务职责 | 周期调度、时间传递、调用 App |
| 任务内存 | 静态 TCB、静态栈 |
| 业务处理 | 由 App 公共入口完成 |
| 时间参数 | `now_ms` |
| OS 类型 | CMSIS-RTOS2 |

时间规则：

- `osKernelGetTickCount()` 的 tick 频率必须转换为毫秒。
- App 入口参数统一命名 `now_ms`。
- tick 回绕和周期漂移按无符号时间差处理。

## 8. Core/Board 契约

### 8.1 目录与所有权

```text
Boards/<mcu>-<board>/
    Core/
        Inc/                  # CubeMX 生成头文件
        Src/                  # CubeMX 生成源文件
        Board/
            Inc/
                core_board.h
            Src/
                core_board.c
    App/
    RTOS/
```

- CubeMX 生成文件由 CubeMX 管理。
- 手写板级装配位于 `Core/Board/`。
- `Core/Board/` 是 Core 内部的手写区域。
- `Core_Board_Init()` 是板级装配公共入口。

入口声明：

```c
platform_status_t Core_Board_Init(
  app_<module>_dependencies_t *dependencies
);
```

### 8.2 板级装配职责

Core/Board 负责：

- 定义静态具体设备、控制器、策略、滤波器和设备组合对象。
- 将逻辑角色映射到 Platform 实例、通道、GPIO、总线和存储资源。
- 完成具体设备初始化。
- 完成算法依赖注入。
- 初始化公共板级服务。
- 填写 App 依赖快照。

Core/Board 保持对象生命周期覆盖整个固件运行期。

### 8.3 CubeMX 生成文件

CubeMX 生成文件负责：

- 时钟和引脚配置。
- 外设结构初始化。
- DMA 和 NVIC 配置。
- `MX_*_Init()` 和 HAL handle。
- CMSIS-RTOS2 任务配置、静态 TCB、静态栈和任务创建。

`main.c` 的 `USER CODE` 区域负责顶层组合：

```c
/* USER CODE BEGIN Includes */
#include "app_<module>.h"
#include "app_<module>_dependencies.h"
#include "core_board.h"
/* USER CODE END Includes */
```

```c
/* USER CODE BEGIN 2 */
app_<module>_dependencies_t dependencies;

if (Core_Board_Init(&dependencies) != PLATFORM_STATUS_OK) {
  Error_Handler();
}

if (!App_<Module>_Init(&dependencies)) {
  Error_Handler();
}
/* USER CODE END 2 */
```

对象映射、具体设备和 App 依赖存储在 `Core/Board/` 中维护。

## 9. 持久记录存储

记录存储分为物理层和业务层：

- Core/Board 拥有 `system_record_store_t` 和物理存储配置。
- Core/Board 使用 App 公共 Magic 配置初始化记录存储。
- Core/Board 将初始化后的 `system_record_store_t *` 注入 App。
- App 拥有业务记录状态和非 owning 指针。
- App 负责记录版本、字段布局、CRC、默认值和恢复策略。
- App 调用记录 Load、Commit、Apply 等业务操作。
- App 通过 System 公共存储接口访问记录。

## 10. 跨平台契约

### 10.1 源码复用

- 同一 App 模块可以在满足依赖契约的不同板卡上编译。
- 同一 RTOS 任务结构可以在提供 CMSIS-RTOS2 port 的不同 MCU 上复用。
- 更换板卡时修改 Core/Board、CubeMX 配置和板级构建。
- 更换 MCU 时增加 Platform backend、System backend、CMSIS-RTOS2 port、
  启动文件、链接脚本和工具链配置。

### 10.2 能力差异

- App 在依赖头中声明必要服务。
- 编译期能力通过类型和配置声明。
- 硬件相关能力通过初始化状态或能力查询表达。
- 芯片差异位于 Platform、System backend 或 Core/Board。
- App 使用统一的物理量和领域接口。

### 10.3 新板卡接入

新增板卡步骤：

1. 创建板级目录和 CubeMX 工程。
2. 配置时钟、引脚、外设、DMA 和 NVIC。
3. 创建 App 依赖头。
4. 创建 `Core/Board` 装配文件。
5. 实现 `Core_Board_Init()`。
6. 配置板级 CMake。
7. 复用 App 和 RTOS 源码。
8. 完成构建和目标板验证。

## 11. CMake 契约

目标名称：

- `layered_app`
- `layered_rtos`
- `layered_core_board`
- `layered_app_portability_fixture`
- `<board_firmware>`

依赖方向：

```text
layered_rtos -> layered_app
layered_core_board -> layered_app
layered_app_portability_fixture -> layered_app

layered_core_board -> Platform / 具体设备 / Controller / Policy / Filter

firmware -> Core 生成代码
firmware -> layered_core_board
firmware -> layered_app
firmware -> layered_rtos
```

include 边界：

| 目标 | 可获得 | 不可获得 |
| --- | --- | --- |
| `layered_app` | App 公共头、base/domain、选定算法公共头、System 公共头 | Platform、硬件驱动实现、Core 私有头、CMSIS |
| `layered_rtos` | App 公共头、CMSIS-RTOS2 | Platform、具体驱动、Core 私有头 |
| `layered_core_board` | App 依赖契约、Platform、具体设备、算法 | Core 生成文件私有实现 |
| `<board_firmware>` | 全部固件组成 | 无 |

`layered_app_portability_fixture` 是独立最小 App，使用 fake dependencies，
分别以主机和全部受支持 backend 配置编译。

聚合目标服务最终固件，App 通过 `layered_app` 获得依赖边界。

## 12. 验收条件

架构迁移完成后满足：

- App 目标在没有 Platform、具体设备、Core 和 CMSIS 头路径时编译。
- App 使用 fake dependencies 运行主机测试。
- App 初始化入口通过 `bool` 返回成功或失败。
- RTOS 只包含 CMSIS-RTOS2 和 App 公共入口。
- 每块板通过 `Core_Board_Init()` 完成一次静态装配。
- Core/Board 初始化后的对象在固件运行期保持有效。
- App 依赖通过 `<app_module>_dependencies.h` 声明并由 Core/Board 填写。
- portability fixture 通过主机和全部受支持 backend 编译。
- App 可以使用 `Sys_Critical_*`，依赖扫描确认没有 CMSIS/FreeRTOS 类型。
- 记录存储由 Core/Board 初始化，由 App 注入使用。
- 更换板卡时 App 和 RTOS 源码保持复用。
- Platform、App 主机测试和各 backend 编译检查通过。

## 13. 代码量观察

- 分别统计 App、RTOS 和 Core/Board 的有效代码量。
- App 代码量包含业务逻辑、应用状态和产品配置。
- RTOS 代码量包含任务入口、属性、周期和 OS 对象。
- Core/Board 代码量包含硬件映射、设备初始化和对象装配。
- 复杂业务按功能复杂度保留必要代码。
- 代码量作为架构观察指标，职责边界作为验收依据。

## 14. 文档与示例

框架文档定义通用规则：

1. `framework-contract.md` 是架构主契约。
2. `platform-contract.md` 管理 Platform 行为契约。
3. `conventions.md` 管理编码规范。
4. `architecture.md` 描述通用框架架构。
5. `todo.md` 记录当前仓库迁移路线和未完成事项。

具体产品接线位于 `doc/examples/` 和对应板级文档，不构成本契约条款。

## 15. 合规检查清单

- `Core/Board/` 作为 Core 内部手写装配区。
- `Core_Board_Init()` 作为每块板的板级装配入口。
- App 依赖头采用 `<app_module>_dependencies.h`。
- App 依赖类型采用 `<app_module>_dependencies_t`。
- CMake 目标采用 `layered_app`、`layered_rtos`、
  `layered_core_board` 和 `layered_app_portability_fixture`。
- App 可以通过 System 公共接口使用临界区。
- portability fixture 独立于实际产品 App。
- 记录存储物理资源由 Core/Board 初始化，记录 schema 由 App 负责。
- `architecture.md`、`conventions.md`、`platform-contract.md` 和 `todo.md`
  与本契约保持一致。

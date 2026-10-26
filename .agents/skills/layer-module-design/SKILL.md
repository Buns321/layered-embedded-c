---
name: layer-module-design
description: Guide creation of new or substitutable non-user-logic modules in Platform, DeviceDrivers, Controller, Policy, Filter, or System, including pre-coding design-guide reading and post-coding item-by-item acceptance. Use when adding a module, concrete implementation, base/ops contract, or changing shared module behavior; do not use for board App/RTOS user logic, ordinary localized fixes, generated CubeMX/HAL code, or third-party code.
---

# 分层模块设计与验收

本 skill 只编排仓库既有规范，不另行推导架构规则。设计依据以 `AGENTS.md` 和
`doc/` 中的维护文档为准；若文档遗漏、冲突或无法确定层级语义，停止实现并向用户
报告冲突，不得凭经验选择其中一方。

## 适用范围

以下工作开始时必须执行“创建前”流程：

- 在 `Platform/`、`DeviceDrivers/`、`Controller/`、`Policy/`、`Filter/` 或
  `System/` 新增模块、可替换实现或新的 base/ops 家族。
- 修改上述模块的公共接口、分层职责、调用链、所有权、单位、错误语义或
  并发契约。
- 在 `DeviceDrivers/` 中新增 Chassis、IMU、遥测协议等设备角色。

以下工作不触发本 skill：

- `Boards/<mcu>-<name>/App/`、`Boards/<mcu>-<name>/RTOS/` 等用户逻辑层代码。
- 不改变模块形态或公共契约的局部缺陷修复、注释和普通文档编辑。
- `Drivers/`、`Middlewares/`、CubeMX/HAL 生成代码和第三方源码。

若任务同时包含用户逻辑和共享模块，只对共享模块执行本验收；仍需完成板级接线和
受影响目标的编译验证。

## 创建前

1. 先确定层级、模块词干、抽象接口与具体实现、使用者和被依赖者、调用上下文、
   单位、生命周期、失败语义以及需要的测试。此时不要开始写实现。
2. 完整阅读以下文档：
   - `AGENTS.md`
   - `doc/architecture.md`
   - `doc/conventions.md`
   - `doc/units.md`
3. 根据目标层继续阅读：

| 目标层 | 必读文档 |
| --- | --- |
| `Platform/` | `doc/platform.md`、`doc/platform-contract.md` |
| `DeviceDrivers/Base/` | `doc/architecture.md` 的对象模型、DeviceDrivers 职责和扩展方式；相关设备契约 |
| `DeviceDrivers/` 具体实现 | DeviceDrivers 与相关设备契约；涉及 Platform 时读 `doc/platform.md` |
| IMU 驱动或滤波接线 | `doc/imu.md` |
| 遥测或日志设备 | `doc/telemetry.md` |
| `Controller/` | `doc/architecture.md` 的 Controller 职责、数据流和约束 |
| `Policy/` | `doc/architecture.md` 的 Policy 职责、数据流和约束 |
| `Filter/` | `doc/architecture.md` 的 Filter 职责、数据流和约束 |
| `System/` | `doc/architecture.md` 的 System 职责、约束和日志说明；日志实现另读 `doc/telemetry.md` |

4. 新增公共头、Doxygen 分组或测试时，同时阅读 `doc/doxygen/groups.dox`、
   `doc/doxygen/Doxyfile` 和 `cmake/platform-test/README.md` 中适用部分。
5. 具体驱动依赖板级引脚、总线实例或硬件映射时，阅读目标板
   `README.md` 与 `hardware-map.md`。
6. 查看同层相邻模块的接口、示例调用和测试，复现既有抽象范式。阅读用于理解
   契约，不复制无关实现，也不发明新的层级模式。
7. 在编辑前用简短契约说明确认：命名、base/ops、调用逻辑、单位、所有权、
   并发、返回值、失败路径、静态资源和验证方式。任何一项无法从文档确定时，
   先解决文档问题。

## 实现

- 保持依赖方向为上层到下层；共享层不得反向依赖 App、RTOS 或具体板级代码。
- 优先复用现有 base/ops、状态类型和工具函数；抽象只在当前模块确有职责边界时
  增加。
- 只在本次模块所需的目录、CMake、Doxygen 和测试中做改动。
- 行为变化必须有聚焦测试，覆盖非法参数、边界和错误路径。

## 创建后验收

模块实现完成后、声称任务完成前，必须：

1. 完整阅读 [references/acceptance-checklist.md](references/acceptance-checklist.md)。
2. 阅读完整目标差异和每个触碰文件的上下文，并检查直接调用方与被调用方。
3. 按清单逐项给出“通过”“不通过”或“不适用”；每项附 `文件:行`、测试命令
   或文档依据。“不适用”必须说明原因，不得静默跳过。
4. 对不通过项先输出醒目的结论：

> **验收不通过：停止完成声明。**

   然后逐项给出失败位置、违反的文档条款、具体修正动作，以及修正前必须重新
   阅读的文档和章节。
5. 修正前重新打开并阅读失败项指向的文档，不得凭记忆直接改代码。修正后从
   第一项重新执行完整清单，并重跑全部受影响检查，不只重跑失败项。
6. 不得通过放宽警告、删除或跳过测试、修改期望值或绕过参数校验取得通过。
   连续两轮修正仍无净改善，或修复需要改变架构契约时，停止并询问用户。

只有全部适用项通过、受影响测试与构建完成、未验证项和剩余风险已明确报告时，
才可以结束。本验收不替代仓库要求 Git 操作前执行的 `$pre-git-review`。

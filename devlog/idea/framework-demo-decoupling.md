# Idea 1：框架层与 Demo 工程解耦及显式装配重构

> **状态**：构想记录（待立项实施）
> **前置边界**：以下设计决策基于当前 `layered-embedded-c` 仓库耦合分析，尚未在第二个真实车型工程中验证。

## 1. 背景与现状问题

当前仓库作为可复用嵌入式框架的演进起点，存在**框架层与当前四轮全向小车 Demo 强耦合**的问题，具体体现如下：

1. **Demo 选型固化在框架目录**：
   - `Framework/Chassis/Inc/car_omni.h` 与 `Framework/Chassis/Src/car_omni.c` 位于 `Framework/Chassis/` 目录下，但内部直接静态持有了具体 Demo 的硬件与协议选型：`motor_tb6612_t`、`imu_wht101_t`、`rc_joycon_t`、`telem_justfloat_t` / `telem_firewater_t` 以及固定的 16 通道遥测布局。
   - 相比之下，`Framework/Chassis/Inc/chassis_omni.h` 仅依赖多态基类 `motor_base_t *`、`imu_base_t *` 与 `kinematics_omni_t`，才是真正与具体器件型号解耦的通用底盘组件。
2. **命名歧义**：
   - `Car_Omni_*` 与通用的 `Chassis_Omni_*` 并列暴露于 `Framework/Chassis/`，使“可复用框架能力”和“单车 Demo 装配胶水”的边界模糊。
3. **中间配置翻译层臃肿（174 行 `Car_Omni_Init`）**：
   - `car_omni_cfg_t` 在底层驱动的原生 `*_cfg_t` 之上重新包装了一套扁平字段，导致 `Car_Omni_Init()` 必须做大量字段检验、**0 值默认值回退**，再逐个构造 `motor_tb6612_cfg_t`、`imu_wht101_cfg_t`、`rc_joycon_cfg_t`、`telem_*_cfg_t` 并转发调用。
4. **“按需装配（`NULL` 跳过）”引入静默漏配风险**：
   - 现有 `doc/architecture.md` 规定 `huart == NULL` 时自动跳过对应外设与看门狗，导致“刻意裁剪”与“遗漏配置”共用同一语义，无法在启动阶段暴露漏配错误，同时在运行时制造大量 `if (car->has_*)` 分支。

## 2. 关键架构决策与否决项

### 2.1 采纳方案

1. **剥离 `car_omni` 出 `Framework/` 层**：
   - `Framework/` 仅保留与具体车型、传感器型号无关的可复用组件：`BSP/`、`Framework/Core/`、`Framework/Control/`、`Framework/Drivers/`、`Framework/Chassis/`（`chassis_base`、`chassis_omni` 等）。
   - `car_omni` 整车装配及固定 16 通道遥测逻辑改名并下沉为 Demo 工程代码（如 `User/` 或独立的 `Examples/`），彻底解除框架对单一 Demo 选型的绑定。
2. **废除 `0` 值自动回退与 `NULL` 静默跳过，改为显式组合与严格报错**：
   - 使用方需要哪个模块，便显式调用该模块的原生 `Init`；未启用的模块不调用且不编入工程。
   - 一旦调用某模块的 `Init` 并传入 `NULL` 或非法参数，立即返回错误码并触发故障停机，杜绝静默降级。
   - 对当前 `User/board_cfg.c` 四轮参数的视觉重复，可通过文件内局部配置宏收口共享字段（如 PWM 定时器、PID 默认参数），消除纯文本噪音而不引入额外抽象层。

### 2.2 明确否决项

1. **否决将 `Car_Omni_Init()` 内所有驱动调用再封装为 1:1 公共初始化函数**：
   - `Dev_IMU_WHT101_Init`、`RC_JoyCon_Init`、`Dev_Telem_*_Init` 本身已是面向复用的驱动级 API；在其外再套一层公共初始化函数属于 1:1 转发壳，反而增加用户学习与记忆成本。
   - 仅“TB6612 + 正交编码器 + 外部 PID 闭环”这一组合在直流有刷闭环方案中具有局部辅助装配价值，可在确有跨工程复用需求时单独收敛。
2. **否决“大框架模板 + `USER CODE BEGIN/END` 槽位”模式**：
   - 文本槽位是代码生成器的妥协，而非 C 框架范式。框架内嵌用户代码块会导致下游工程升级框架时频繁发生 Git 合并冲突，并破坏单向依赖隔离与主机端 CTest。
   - 若未来出现 ≥2 个相似机器人工程需要复用运行态骨架（状态机、看门狗调度、遥测定时循环），应通过**依赖注入（基类指针）**与**回调/钩子注册**（类似 `RTOS_SetEmergencyHaltHook()`）实现反转控制，而非代码槽位。

## 3. 演进路线对比

| 维度 | 路线 A：纯组件库模式（当前首选） | 路线 B：通用运行骨架 + 依赖注入 | 路线 C：大框架模板 + `USER CODE` 填空 |
| :--- | :--- | :--- | :--- |
| **框架层职责** | 仅保留 `BSP/`、`Core/`、`Control/`、`Drivers/`、`Chassis/chassis_*` | 在路线 A 基础上，提供与型号无关的 `Robot_Runtime` 状态机与调度 | 框架包办全部初始化与主循环，中间留注释块 |
| **`car_omni` 归属** | 移出 `Framework/`，下沉为 Demo 工程代码 | 拆解为“框架侧通用骨架” + “Demo 侧驱动实例化与注入” | 膨胀为更庞大的单体模板 |
| **更换传感器或底盘构型的框架改动** | **0 改动**（仅改 Demo 工程装配代码） | **0 改动**（仅改注入对象） | 需修改框架模板内部逻辑 |
| **架构复杂度与误判风险** | **低**：契约清晰，避免提前抽象 | **中**：仅在 ≥2 个工程出现重复骨架时才有净收益 | **高**：破坏分层隔离与主机单测 |

## 4. 实施前提与契约同步清单

由于本方案会打破现有“0 值回退”与“`huart == NULL` 自动跳过”契约，正式实施时须同步调整以下目标以保持系统自洽：

1. **架构文档**：更新 `doc/architecture.md`，重新界定 `Framework/` 的纯组件库边界，移除 `Car_Omni` 的 `NULL` 静默跳过规则说明。
2. **整车门面与板级代码**：调整或迁移 `Framework/Chassis/Inc/car_omni.h`、`Framework/Chassis/Src/car_omni.c` 与 `User/board_cfg.c`。
3. **测试与门禁**：更新或移除依赖 `NULL` 跳过及 0 值默认回退行为的主机 CTest 用例，同步检查 `tools/check.py` 分层规则与体量预算门禁。

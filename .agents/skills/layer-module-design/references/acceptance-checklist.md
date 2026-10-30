# 分层模块验收清单

逐项检查并记录证据。清单中的规则来自仓库维护文档；若代码示例与本清单冲突，
以 `AGENTS.md` 和 `doc/` 为准并报告冲突。

## 1. 层级身份与依赖

- [ ] 模块位于正确层级，使用该层规定的 API 前缀，没有把用户逻辑下沉到共享层。
- [ ] 依赖方向为上层到下层；共享算法、设备和 Platform 没有反向依赖 App、RTOS
  或具体板级装配。
- [ ] `Platform/Inc/` 公共头不含 HAL、CMSIS 或 CubeMX handle；HAL 类型只出现在
  `Platform/Src/`。
- [ ] `DeviceDrivers/Base/` 只依赖 System 和标准库，不依赖具体驱动。
- [ ] 未修改 `Drivers/`、`Middlewares/`、第三方源码或 USER CODE 区之外的
  CubeMX 生成代码。

依据：`AGENTS.md`、`doc/architecture.md` 第 2、4、9 节、
`doc/conventions.md` 第 9 节、`doc/platform.md` 第 1、2 节。

## 2. 命名与公共接口

- [ ] 公共函数符合 `Layer_Module_Qualifier_VerbObject`，动作没有被额外下划线
  拆开；平行模块保持相同词序和动作顺序。
- [ ] 类型使用小写蛇形 `_t`，struct/enum/union 标签分别以 `_s`/`_e`/`_u`
  结尾；枚举值与宏使用大写蛇形。
- [ ] 长期对象指针使用与角色一致的 `<role>_h`，数组同类对象使用下标而非编号。
- [ ] 每个公共参数都说明单位、范围、所有权、调用上下文、并发限制和失败行为。
- [ ] 接口使用 SI 单位；`dt_s` 为秒且大于 0，`time_ms` 表示毫秒，量程和符号
  约定明确。
- [ ] 新增或修改的物理量参数、字段、局部变量和常量均以 `doc/units.md`
  登记的规范单位后缀结尾，例如 `target_speed_mps`、`control_period_s`。

依据：`doc/conventions.md` 第 1、5、6 节、`doc/units.md`。

## 3. 对象模型、初始化与错误

- [ ] base 是抽象接口而非独立实例；具体类型首成员是对应 base。
- [ ] 具体 init 校验参数并绑定静态 `*_ops_t`，base init 只处理通用状态和依赖
  注入；没有复制 base 或动态分配。
- [ ] 回调通过具体类型和角色句柄恢复对象，不把配置、状态或一次性输入作为 `_h`。
- [ ] 可能失败的初始化返回明确状态，调用方检查失败；没有静默忽略。
- [ ] 外部输入做有限值和范围检查；运行期错误使用状态枚举或返回值，`assert`
  只用于内部不变量。
- [ ] 错误路径不会留下半初始化对象、悬挂回调或无法重启的状态。

依据：`AGENTS.md`、`doc/architecture.md` 第 3、8、9 节、
`doc/conventions.md` 第 2、3、4 节、`doc/platform-contract.md` 第 2、3、5 节。

## 4. 调用逻辑与并发

- [ ] 调用链与本层职责一致：Controller 只计算控制量，Policy 只做运动解算，
  Chassis 下发目标，Motor 运行闭环，Filter 不采集硬件。
- [ ] 同层模块只通过规定的 base 视图协作，不越过接口直接读取具体实现字段。
- [ ] ISR 只入队、清标志或重新挂接收；解析、控制和阻塞操作位于任务上下文。
- [ ] ISR 与任务共享变量明确单写者/单读者并使用 `volatile`；没有无保护竞态。
- [ ] IT/DMA 缓冲区在最终完成、错误或 abort 事件前保持有效；回调所有者唯一。
- [ ] 时间戳、采样周期和超时单位在各层换算正确，没有重复消费一次性数据。

依据：`doc/architecture.md` 第 4、5、9 节、`doc/conventions.md` 第 5、6 节、
`doc/platform.md` 第 6、7、8、12、13 节、`doc/platform-contract.md` 第 5–7 节、
`doc/imu.md` 第 7 节、`doc/telemetry.md` 第 3.4、3.6 节。

## 5. 分层专项检查

### Platform

- [ ] 公共行为由 `Platform/Src/common/` 统一，系列目录只保留 handle 映射、
      HAL/时钟/位宽差异和事件转换。
- [ ] 实例身份独立于 HAL handle；未启用实例不进入公共枚举。
- [ ] 公共输入错误稳定返回 `PLATFORM_STATUS_INVALID_ARGUMENT`；Async 启动成功
      不表示传输完成。
- [ ] 阻塞接口超时为毫秒，IT/DMA 无隐藏阻塞超时；abort 和不支持能力有明确状态。
- [ ] 同一物理实例只有一个回调所有者，重复注册返回 BUSY，错误快照遵循统一规则。
- [ ] F1/F4 公共签名、状态、校验和生命周期没有系列差异；不支持能力返回
      `PLATFORM_STATUS_UNSUPPORTED`。

依据：`doc/platform.md`、`doc/platform-contract.md`、`doc/units.md`。

### DeviceDrivers Base

- [ ] base 只定义角色能力，`ops` 只暴露该角色需要的操作，不泄漏具体芯片字段。
- [ ] 生命周期、单位、所有权、回调和失败语义在接口处完整声明。
- [ ] 上层可替换具体实现，无需包含具体驱动头或修改调用逻辑。
- [ ] 抽象接口没有混入板级引脚、HAL handle 或协议实现细节。

依据：`doc/architecture.md` 第 3、4、9、10 节、`doc/conventions.md` 第 2 节。

### DeviceDrivers 具体实现

- [ ] 首成员是对应 base，ops 表为静态数据；初始化后对象可长期静态存活。
- [ ] 只通过 Platform、System 和 base 依赖访问下层，不直接包含 HAL。
- [ ] 引脚、时钟、DMA 和 NVIC 配置留在 CubeMX/Core；驱动不暗含未注入的板级资源。
- [ ] 协议解析、缓冲和回调所有权与具体设备契约一致，错误和忙状态可恢复。
- [ ] IMU 驱动只提交原始通道和真实时间戳；滤波、合并和快照由 `imu_base_t`
      负责。
- [ ] 遥测发送非阻塞、整帧接受或丢弃，协议格式、端序和缓冲生命周期明确。

依据：`doc/architecture.md` 第 4、5、9、10 节、`doc/imu.md`、
`doc/telemetry.md`、`doc/platform.md`。

### Controller

- [ ] `controller_base_t` + `controller_ops_t` 的 update/reset 职责单一。
- [ ] `update` 接收本槽位 `target_mps`、`measure_mps`、秒单位正 `dt_s`，
      输出执行器量，并返回 OK/SATURATED/ERROR 等明确状态。
- [ ] 控制律是纯函数或显式状态机；状态跨周期变化时 reset 能恢复确定初态。
- [ ] Controller 不调用 Policy、Chassis、具体驱动或硬件；组合器只并联求和。
- [ ] 输出限幅、步长和摩擦补偿的归属及量程由调用契约明确。

依据：`doc/architecture.md` 第 4、5、9、10 节、`doc/conventions.md` 第 2–5 节、
`doc/units.md`。

### Policy

- [ ] 使用对应底盘类型的 policy base，而不是通用 `void *` 或具体 Controller。
- [ ] MIMO `solve` 输入输出单位、方向符号、数组长度和底盘类型明确。
- [ ] Policy 独立于 Controller 和具体执行器，只做运动解算，不写电机或 PWM。
- [ ] 不合法输入、非法安装参数或除零风险有明确保护。
- [ ] 静态几何配置在 init 校验并可复现，输出槽位顺序与调用方一致。

依据：`doc/architecture.md` 第 4、5、9、10 节、`doc/units.md`。

### Filter

- [ ] 使用 `filter_sample_t` 的固定通道、`valid_mask` 和 `time_ms` 契约。
- [ ] `Filter_Base_Update` 先完整复制输入，再调用具体 update；模块遵循该预复制
      和路由覆盖语义。
- [ ] Filter 不采集硬件、不阻塞、不动态分配；滤波算法与设备 I/O 分离。
- [ ] Init 拒绝冲突路由、非法通道和不可满足的配置，失败时不留下部分状态。
- [ ] IMU 滤波器只声明实际更新通道，并保留输入帧时间戳。

依据：`doc/architecture.md` 第 4、5、9、10 节、`doc/imu.md`、`doc/units.md`。

### System

- [ ] 模块提供通用数学、延时、临界区、故障、日志或看门狗服务，不依赖上层。
- [ ] 日志后端使用 `sys_log_base_t` + ops；RTT 原始头只由允许的适配文件包含。
- [ ] 临界区和日志路径明确任务/ISR 边界，不在 ISR 中格式化或阻塞。
- [ ] 超时、时基和看门狗单位明确；底层寄存器操作没有越过 System 职责。

依据：`doc/architecture.md` 第 4、5、9、10 节、`doc/telemetry.md`、
`doc/units.md`。

## 6. 注释、构建、测试与范围

- [ ] 每个维护的 `.c/.h` 有中文 Doxygen 文件头和正确分组；所有函数注释都
      不得只有 `@brief`，每个参数有 `@param`，非 `void` 返回有 `@return`
      或逐状态 `@retval`，并按实际契约使用 `@pre`、`@post`、`@note` 或
      `@warning` 说明前置条件、后置状态、副作用、单位、范围、所有权、调用
      上下文、失败行为或并发限制；无参数且返回 `void` 的函数也至少说明
      `@post` 或 `@note`。
- [ ] 公共头声明与 `.c` 定义两处同步；私有函数前向声明和定义两处同步。
- [ ] 新增公共头加入 Doxygen `INPUT`，新增分组先登记到 `groups.dox`。
- [ ] 新增源文件和测试加入对应 `CMakeLists.txt`，没有遗漏构建接线。
- [ ] 行为变化有聚焦原生测试，覆盖非法参数、边界和错误路径；测试断言有效，
      未删减或放宽既有覆盖。
- [ ] 共享代码完成原生测试，并按影响运行 F1/F4 debug 编译检查；受影响目标完成
      Debug 构建，Doxygen 新增警告为零。
- [ ] 完整差异只包含本次任务；没有无关用户改动、生成物、调试残留或格式噪音。

依据：`AGENTS.md`、`doc/conventions.md` 第 7–10 节、
`cmake/platform-test/README.md`、`doc/doxygen/groups.dox`、
`doc/doxygen/Doxyfile`。

## 7. 验收结论

在最终回复中给出：

1. 每个适用检查项的状态与证据。
2. 执行过的构建、测试和文档命令及结果。
3. 无法在本机验证的硬件行为与剩余风险。
4. 若任意一项不通过，先给出阻断结论，再列出重新阅读文档、修正和全量复验要求。

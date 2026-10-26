# 测试门禁矩阵

测试按变更影响面选择。行为变更缺少匹配测试时不得通过审查。纯文档、注释或只读
配置可以免除新增单测，但仍需执行适用构建和文档检查。

## 1. 基本规则

- Bug 修复必须有回归测试，证明修复前失败、修复后通过。
- 新功能必须测试正常路径、边界值、非法参数和可恢复错误。
- 重构至少保证现有测试全过；触碰区域无测试时补最小契约测试。
- 测试必须验证行为或不变式，不能只匹配生成文本、文件名或头文件内容。
- 测试必须确定性运行，不依赖真实墙钟、随机调度或本机绝对路径。
- 不允许删除断言、弱化期望、增加无理由 skip 或修改测试掩盖实现错误。
- 修复后运行全部受影响检查，不只重跑失败项。
- 任何维护中的 `.c/.h` 或进入 `Doxyfile` 的文档发生变化时，必须重新生成 Doxygen，
  且警告为零。
- 受保护文档变更缺少当前会话中的明确人工审批时，直接判定失败，不执行后续测试来
  形成“通过”结论。

## 2. 变更类型与最低验证

| 变更类型 | 新增或更新测试 | 必须执行 |
| --- | --- | --- |
| 受保护规范、约定、架构或接口文档 | 通常不要求 | 人工审批核验；路径、链接和条款一致性；进入 `Doxyfile` 时生成 Doxygen |
| 普通注释、小型非规范性文档 | 通常不要求；公共 API 说明变化时检查示例 | 全量审查；路径与链接检查；进入 `Doxyfile` 时生成 Doxygen |
| 仅测试代码或 CTest 注册 | 验证测试本身能失败并正确通过 | 全量原生测试；受影响 Debug 构建 |
| HAL-free 算法、状态机、校验逻辑 | 新增 `<module>_contract_test.c` 或回归测试 | 全量 CTest；受影响 Debug 构建 |
| `Controller/`、`Policy/`、`Filter/` | 正常、饱和、边界、非有限值、状态复位 | 全量 CTest；F4 Debug；共享行为验证 F1 |
| `DeviceDrivers/Base/` | base/ops 转发、所有权、错误路径 | 全量 CTest；F4/F1 适用 Debug 构建 |
| DeviceDrivers 具体驱动 | 可用伪 backend 覆盖协议与状态机；硬件时序另验 | 全量 CTest；对应目标 Debug；F4/F1 共享源码检查 |
| IMU 与滤波管线 | 通道 schema、时间戳、异步合并、无效通道、并发边界 | 全量 CTest；F4/F1 适用 Debug；板上驱动验证 |
| `Platform/Src/common/` | 同一组 F1/F4 契约测试 | 全量 CTest；`f4-debug`、`f1-debug` 全实例检查 |
| Platform F1/F4 backend | HAL 可映射的公共校验测试；时序无法单测时记录硬件证据 | 对应 Debug/Release 构建；受影响全实例检查 |
| `System/` 共享服务 | 时间源、状态、错误路径和并发契约 | 全量 CTest；F4/F1 共享检查 |
| 遥测与日志 | 帧容量、忙状态、丢帧、无效值、DMA 所有权 | 全量 CTest；受影响 Debug；硬件通路验证 |
| RTOS/App 逻辑 | 纯逻辑抽成 host 可测函数；任务编排做集成验证 | F4/F1 目标构建；任务周期和 ISR 的板上证据 |
| Board、`.ioc`、启动文件、链接脚本 | 自检或板级测试按硬件条件补充 | 目标 Debug/Release、内存预算、烧录和板级验证 |
| CMake、工具链、preset | 验证 clean configure 和错误配置提示 | 干净构建目录配置；受影响 Debug/Release |
| 第三方、生成代码 | 不修改，除非生成器或 `USER CODE` 流程要求 | 重新生成、差异核对和受影响构建 |

## 3. 标准命令

原生 C 契约测试：

```powershell
cmake -S cmake/platform-test -B build/platform-test -G Ninja -DCMAKE_C_COMPILER=gcc
cmake --build build/platform-test
ctest --test-dir build/platform-test --output-on-failure
```

共享 Platform 或 System：

```powershell
cd cmake/platform-check
cmake --preset f4-debug
cmake --build --preset f4-debug
cmake --preset f1-debug
cmake --build --preset f1-debug
```

F407 正式目标：

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

F103 正式目标：

```powershell
cd Boards/f103c8t6-platform-test
cmake --preset Debug
cmake --build --preset Debug
```

维护中的 C/H、公共 API、Doxygen 或已纳入 `Doxyfile` 的文档变化：

```powershell
cmake --build build/Debug --target docs
```

## 4. 测试内容审查

新增测试至少检查适用的以下项目：

- 空指针、非法枚举、零长度、越界、未对齐和不支持能力。
- 数值边界、非有限值、饱和、回绕和状态复位。
- 初始化失败、重复注册、busy、timeout、error 和 abort 恢复。
- base 到具体 ops 的转发以及上下文所有权。
- 异步完成只产生一次最终事件，缓冲区生命周期正确。
- 快照复制、通道掩码、时间戳和并发读取一致性。
- 测试名称 `<module>_contract_test.c`，同时注册 `add_executable()` 和 `add_test()`。

## 5. 硬件与远端门禁

- 板级、CubeMX、启动、链接、DMA、中断和电气行为不能仅由主机测试证明。
- 本地 commit 必须先通过自动审查、对应测试和构建。
- 板上时序、电气、DMA 或中断改动在 push/PR 前提供日志、截图、波形或测试记录。
- 用户明确接受剩余风险时，可以在报告中记录未完成的硬件项，但不得伪称已通过。
- 缺少硬件证据且用户未接受风险时，push/PR 审查失败。

## 6. 文档审批证据

受保护文档变更记录：

```text
审批范围：doc/architecture.md 的第 4 节分层职责
人工审批：用户在当前会话中明确批准该章节的语义调整
核验结果：PASS
```

若没有这类记录，审查结论必须是：

> **审查不通过：受保护文档变更缺少人工审批，停止 Git 操作。**

> **强制动作：修改前必须重新打开并逐条阅读 `AGENTS.md` 和相关规范文档。**

普通注释或小型非规范性文档不要求人工审批，但仍需记录实际执行的全量审查结果。

## 7. 证据格式

报告中记录：

```text
检查：ctest --test-dir build/platform-test --output-on-failure
结果：18/18 passed

检查：F103 Debug build
结果：PASS，RAM 9,672 B / 20 KB，FLASH 16,156 B / 64 KB

未执行：板上 WS2812 时序验证
原因：当前无目标板
风险：DMA 时序和电平仍需硬件确认
```

命令、结果和剩余风险必须来自实际执行；不得根据历史构建结果推断当前通过。

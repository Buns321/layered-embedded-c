---
name: pre-git-review
description: Review intended repository changes against project architecture, API and naming rules, Chinese Doxygen contracts, protected-document approval rules, and required tests before commit, amend, merge, rebase, cherry-pick, revert, push, or tag. Use for mutating Git operations, not read-only Git commands, and do not treat review as permission to mutate Git state.
---

# 提交前审查

在会改变仓库历史的 Git 操作前审查完整目标变更。审查以仓库文档为准，不从本
skill 推导新的架构规则。审查通过只代表变更满足门禁，不授予提交或推送权限。

## 读取依据

始终读取：

- `AGENTS.md`
- `doc/conventions.md`
- `references/review-checklist.md`
- `references/test-matrix.md`

再按变更路径读取对应文档：

| 变更路径 | 必读文档 |
| --- | --- |
| `Platform/` | `doc/architecture.md`、`doc/platform.md`、`doc/platform-contract.md`、`doc/units.md` |
| `Controller/`、`Policy/`、`DeviceDrivers/` | `doc/architecture.md`、`doc/units.md`；按设备读取 `doc/imu.md` 或 `doc/telemetry.md` |
| `Filter/`、`System/` | `doc/architecture.md`、`doc/units.md`；日志读取 `doc/telemetry.md` |
| `RTOS/`、`App/` | `doc/architecture.md`、`doc/units.md`、`doc/telemetry.md` 和板级文档 |
| `Core/`、`Boards/`、CubeMX、启动或链接文件 | 对应 `Boards/<mcu>-<name>/README.md`、`hardware-map.md`、`doc/platform.md` |
| `cmake/`、测试或工具 | `AGENTS.md`、`cmake/platform-test/README.md`、`cmake/platform-check/README.md` 中适用项 |
| 公共 API、Doxygen 或 Markdown | `doc/doxygen/Doxyfile`、`doc/doxygen/groups.dox` 和对应模块文档 |

若仓库文档与代码不一致，先指出冲突并解决文档或实现。不得自行选择一方后静默提交。

## 确定范围

1. 运行 `git status --short`，区分预期变更、无关改动和未跟踪文件。
2. 对 `commit` 或 `amend`，优先审查已暂存差异；尚未暂存时，根据用户目标确定完整
   待提交集合。无法区分无关改动时停止并询问。
3. 对 `merge`、`rebase`、`cherry-pick` 或 `revert`，审查操作后的最终差异和冲突
   解决结果。
4. 对 `push`，审查即将推送的提交和相对上游的完整差异；不得只检查最后一条提交。
5. 对 `tag`，确认目标提交没有遗漏未审查变更；若 tag 指向已有提交，至少执行仓库
   状态、提交信息和测试证据检查。

不要自动暂存、取消暂存、还原或删除用户改动。无关改动不得混入目标提交。

## 文档审批分级

在继续审查前，先识别规范性文档变更：

- 受保护文档包括 `AGENTS.md`、项目 skill、`doc/architecture.md`、
  `doc/conventions.md`、`doc/platform.md`、`doc/platform-contract.md`、
  `doc/units.md`、`doc/imu.md`、`doc/telemetry.md`、板级接口与硬件规范，以及任何
  维护文档中新增、删除、放宽或重定义约束的章节。
- 受保护文档变更必须具有当前会话中的明确人工审批。审批必须指出文件、章节或具体
  语义变化；普通功能实现请求不构成修改治理规则的审批。
- 缺少审批时立即判定审查不通过并停止 Git 操作。不得自行还原受保护文档，也不得把
  审批缺失当成普通代码问题直接修复。
- 普通注释和非规范性小文档可以免审批修改，但仍须通过路径、链接、中文、Doxygen、
  一致性和测试矩阵审查。
- 注释若改变单位、范围、所有权、调用上下文、失败行为、并发限制或前置条件，视为
  规范性语义变化，需要人工审批。

## 执行审查

1. 逐文件阅读目标差异及其直接上下文，不只阅读 diff 中新增的行。
2. 完成文档分级和审批检查。受保护文档缺少审批时直接失败，不继续执行测试来掩盖阻断。
3. 按 `references/review-checklist.md` 检查架构、接口、命名、注释、单位、错误处理、
   并发、构建和文档。
4. 按 `references/test-matrix.md` 确定必测项。缺少必要测试本身就是审查失败。
5. 运行受影响检查并记录命令、退出状态和关键结果。
6. 检查 `git diff --check` 和最终 `git status --short`，确认没有空白错误、调试残留、
   生成物或无关文件。
7. 检查提交信息是否符合 Conventional Commits 和变更范围。

## 失败与重试

- 对明确、局部且不改变设计意图的违规，直接修复并补充测试。
- 修复后重新运行全部受影响检查，不只重跑失败项。
- 不通过降低编译警告、删除断言、修改错误期望、增加无理由 skip 或放宽参数校验来
  获得通过。
- 连续两次修复没有净改善，或需要改变公共契约、分层职责、硬件语义时停止并报告。
- 不得绕过检查。即使仓库以后增加 hook，也不得使用 `--no-verify` 或等价方式跳过。

## 审查失败输出

任何失败都必须先输出醒目的阻断结论：

> **审查不通过：停止 Git 操作。**

随后必须逐项列出：

1. 失败类别和严重级别。
2. 精确位置，使用 `文件:行`；测试失败还要给出命令、退出码和关键日志。
3. 违反的 `AGENTS.md` 或 `doc/` 条款原文或准确指代。
4. 具体修复动作和必须重新运行的检查。
5. 必须重新阅读的文档链接，精确到文件和相关章节。

失败报告必须包含以下醒目提示：

> **强制动作：修改前必须重新打开并逐条阅读上述文档。禁止凭记忆、猜测或旧结论修复。**

若失败原因是受保护文档缺少人工审批，报告必须明确：

- 哪些受保护文件或语义发生变更。
- 当前会话中缺少哪一项具体审批。
- 在获得用户对文件、章节或语义变化的明确批准前，不得继续修改相关规则或执行
  Git 操作。

不得只回复“规范不符”或“请修复”。必须让执行 agent 能直接定位失败项和依据文档。

## 通过标准

仅当以下条件全部满足时判定通过：

- 新增代码和触碰代码符合所有适用文档规范。
- 行为变更具有有效测试，错误路径和边界条件覆盖充分。
- 必需的原生测试、编译检查、Doxygen 和硬件验证证据完整。
- 受保护文档变更具有当前会话中的明确人工审批。
- 目标差异只包含本次任务相关内容。
- 提交信息准确描述行为和范围。

通过后先向用户报告违规修复、执行过的检查和剩余风险。只有用户原本要求对应 Git
操作时才执行该操作；否则停在审查结果。

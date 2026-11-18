# layered-embedded-c Agent 工程规范

面向 STM32F407 / STM32F103 的分层嵌入式 C 工程（共享框架 `Platform/`、`DeviceDrivers/`、`Controller/`、`Policy/`、`Filter/`、`System/`，以及 `Boards/<mcu>-<name>/` 下三套独立板级固件）。

---

## 1. TBD（Trunk-Based Development）铁律

- **严禁直接在 `main` 分支开发或提交**。所有工作必须在短生命周期工作分支（`feat/*`、`fix/*`、`refactor/*`、`docs/*`）上进行。
- **小步快跑**：保持变更聚焦、频繁同步 `main`（`git fetch && git rebase origin/main`）。
- **每轮自动 Checkpoint**：`.agents/hooks.json` 的 `Stop` hook 会在每轮对话结束后自动在当前工作分支创建 `[auto-commit]` checkpoint；合入 `main` 前应整理提交历史并跑通本地门禁。

---

## 2. 文档驱动设计实现（Doc-Driven Development）

- **先读后写**：修改任何源码或配置前，必须先阅读 [tools/doc_map.json](tools/doc_map.json) 中对应路径映射的文档（`PreToolUse` `doc_gate` hook 会对此进行硬性检查）。
- **核心文档入口**：
  - 编码规范：[doc/conventions.md](doc/conventions.md)
  - 架构与分层：[doc/architecture.md](doc/architecture.md)、[doc/framework-contract.md](doc/framework-contract.md)
  - 平台契约：[doc/platform-contract.md](doc/platform-contract.md)、[doc/platform.md](doc/platform.md)
  - 物理单位：[doc/units.md](doc/units.md)
- **文档同步**：修改公共接口、模块边界或板级硬件映射时，必须同步更新对应文档与 [tools/doc_map.json](tools/doc_map.json)。

---

## 3. 嵌入式 C 编码与分层底线

- **静态分配**：所有长期对象、RTOS 任务控制块与缓冲区必须静态分配；禁止在生产代码中使用 `malloc`/`calloc`/`realloc`/`free`。
- **分层隔离**：
  - `Platform/`、`DeviceDrivers/`、`Controller/`、`Policy/`、`Filter/`、`System/` 不得包含板级头文件或具体 MCU HAL（`Platform/` 后端与 `Core/` 除外）。
  - `App/` 只依赖领域接口、`app_*_dependencies_t` 注入服务与 `System/` 公共接口，禁止直接操作 HAL 或板级硬件映射。
  - `RTOS/` 固定使用 CMSIS-RTOS2，仅作薄任务入口与周期调度。
- **零警告**：所有新增/修改代码须满足 `-Wall -Wextra` 零警告及 `.clang-tidy` 检查。

---

## 4. 本地多维度验证

不依赖云端 CI/CD，合入 `main` 或交付任务前必须运行本地门禁：

```bash
python3 tools/check.py --profile quick   # 秒级：卫生、格式、约定、分层、文档映射
python3 tools/check.py --profile pr      # 提交前：quick + 主机契约测试 + clang-tidy + F1/F4 Platform 编译
python3 tools/check.py --profile full    # 全量：pr + 三块板级固件构建 + ELF 静态内存/体积检查
```

---

## 5. 环境与权限边界

- **禁止运行 `sudo`**：Agent 不得在自动化命令中使用 `sudo`（无交互式密码输入且存在安全边界）。若缺少系统工具（如 `clang-tidy`），向用户说明并请用户在本地终端手动执行安装命令。

---

## 6. 专项技能（按需加载）

详细操作流程位于 `.agents/skills/`：
- `tbd-workflow`：TBD 分支生命周期、checkpoint 整理与合入流程
- `doc-driven-dev`：文档映射查询、契约对齐与文档同步规范
- `local-verify`：本地多维度测试矩阵、`clang-tidy` 与交叉编译排障

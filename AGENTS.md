# layered-embedded-c Agent 工程规范

面向 STM32 全系列（当前实装 **STM32F407VET6**）的精简 C11 分层嵌入式工程（`BSP/` + `Framework/` + `User/` + `Core/`）。

---

## 1. 反过度抽象三条铁律

1. **硬件闭环优先**：先基于 `BSP/` 与 CubeMX 句柄打通硬件闭环，禁止提前构造无实际调用方的多层空壳。
2. **二次复用才抽象**：仅保留 5 处高价值领域多态（`controller_base_t`、`motor_base_t`、`imu_base_t`、`telemetry_base_t`、`chassis_base_t`），禁止 1:1 单实现虚表或枚举包装层。
3. **代码行数与体积预算红线**：
   - `BSP/` ≤ 18 文件且 ≤ 2,000 行（有效非注释代码行）
   - `User/` + 根 `CMakeLists.txt` ≤ 320 行（有效非注释代码行）
   - `Release` 固件 ≤ 63,520 B Flash、≤ 20,408 B RAM

---

## 2. TBD（Trunk-Based Development）铁律

- **严禁直接在 `main` 分支开发或提交**，所有工作必须在短生命周期分支（`feat/*`、`fix/*`、`refactor/*`、`docs/*`）上进行。
- 保持变更聚焦、频繁同步 `main`（`git fetch && git rebase origin/main`）；`Stop` hook 会自动创建 `[auto-commit]` checkpoint，合入前整理提交并跑通本地门禁。

---

## 3. 文档驱动设计与编码底线

- **先读后写**：修改源码或配置前，先阅读 [tools/doc_map.json](tools/doc_map.json) 映射的核心文档：
  - 编码约定：[doc/conventions.md](doc/conventions.md)
  - 架构与分层：[doc/architecture.md](doc/architecture.md)
  - 物理单位：[doc/units.md](doc/units.md)
  - 硬件映射：[doc/hardware-map.md](doc/hardware-map.md)
- **ISO C11 零警告**：`-std=c11 -Wall -Wextra -Werror`（主机与 ARM 双端）及 `.clang-tidy` 零告警；禁止 GNU 语句表达式等非标准扩展。
- **静态分配铁律**：0 处 `malloc`/`calloc`/`realloc`/`free`；FreeRTOS 维持 `configSUPPORT_STATIC_ALLOCATION=1`、`configSUPPORT_DYNAMIC_ALLOCATION=0` 与 `configCHECK_FOR_STACK_OVERFLOW=2`。
- **单向分层隔离**：
  - `Framework/Control/` 100% HAL-Free，禁止包含 `BSP/`、HAL、FreeRTOS 或 `Core/Inc/`。
  - `User/app_*.c` 禁止直接包含任何 STM32 HAL、`BSP/`、`FreeRTOS` 或 CubeMX 外设头文件。
  - `User/rtos_*.c` 固定使用 CMSIS-RTOS2 与 `rtos_task.h`，仅作薄任务声明与周期转发。

---

## 4. 本地多维度验证

合入 `main` 或交付前必须运行本地门禁（禁止在自动化命令中使用 `sudo`）：

```bash
python3 tools/check.py --profile quick --scope all   # 卫生、格式、静态分配、分层、文档、行数预算、Doxygen
python3 tools/check.py --profile pr --scope all      # quick + 主机 14 项 CTest + clang-tidy
python3 tools/check.py --profile full --scope all    # pr + Debug/Release 固件构建与 ELF 体积/符号检查
```

---

## 5. 专项技能（`.agents/skills/`）

- `tbd-workflow`：分支生命周期、checkpoint 整理与合入流程
- `doc-driven-dev`：文档映射查询、契约对齐与文档同步规范
- `local-verify`：本地多维度测试矩阵、`clang-tidy` 与交叉编译排障

---
name: doc-driven-dev
description: >-
  Doc-Driven Development workflow: look up required docs in tools/doc_map.json before
  designing or editing code, align with architecture/platform/unit contracts, and keep docs synced.
---

# 文档驱动设计实现流程

## 1. 改前必读（与 `doc_gate` Hook 协同）
修改任何文件前，先对照 [tools/doc_map.json](../../../tools/doc_map.json) 用 `read_file` 阅读对应文档：
- 任何 `.c`/`.h` -> `doc/conventions.md`
- `BSP/`、`Framework/`、`User/`、`cmake/`、`CMakeLists.txt` -> `doc/architecture.md`
- `Framework/(Control|Drivers|Chassis)/`、`User/board_cfg.*` -> `doc/units.md`
- `BSP/`、`Core/`、`User/board_cfg.*`、`.ioc`、`.ld`、`startup_*.s` -> `doc/hardware-map.md`

## 2. 设计对齐检查清单
- **API 命名与类型**：`BSP_*` / `Sys_*` / `RTOS_*` / `Ctrl_*` / `Filter_*` / `Kinematics_*` / `Dev_*` / `RC_*` / `Chassis_*` / `Car_*` / `Board_*` / `App_*`；struct/enum/union 标签必须带 `_s`/`_e`/`_u` 且与 `*_t` 词干一致，公共头禁匿名类型。
- **反过度抽象**：硬件闭环优先、二次复用才抽象（仅保留 5 处领域多态），严控 `BSP/` ≤ 2,000 行、`User/` + 根 `CMakeLists.txt` ≤ 320 行。
- **所有权与调用上下文**：公共头用 1–3 行简明 `@brief` 说明单位（SI 单位，见 `doc/units.md`）、范围、所有权及任务/ISR 上下文。
- **静态分配**：禁止 `malloc`/`free`，所有长期对象、环形缓冲与 RTOS 任务均静态分配。

## 3. 改后文档同步
- 新增模块、修改公共头契约或变更硬件映射时，同步更新 `doc/` 中对应文档。
- 新增目录或核心文档时，同步维护 `tools/doc_map.json`。

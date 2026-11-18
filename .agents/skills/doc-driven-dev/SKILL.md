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
- `Platform/` -> `doc/platform-contract.md`、`doc/platform.md`
- `Controller/`、`Policy/`、`Filter/` -> `doc/architecture.md`、`doc/units.md`
- `DeviceDrivers/` -> `doc/architecture.md`（IMU 相关加读 `doc/imu.md`，遥测/串口加读 `doc/telemetry.md`）
- `Boards/<board>/(App|RTOS|Core/Board)/` -> `doc/framework-contract.md` + 对应板卡 `README.md` 与 `hardware-map.md`

## 2. 设计对齐检查清单
- **API 命名与类型**：`Platform_*` / `Dev_*` / `Ctrl_*` / `Policy_*` / `Filter_*` / `Sys_*`；struct/enum/union 标签必须带 `_s`/`_e`/`_u` 且与 `*_t` 词干一致。
- **所有权与调用上下文**：接口注释须写明调用上下文（任务 / ISR）、缓冲所有权、单位（SI 单位，见 `doc/units.md`）。
- **静态分配**：禁止 `malloc`/`free`，长期对象静态分配并由 `Core/Board` 装配注入。

## 3. 改后文档同步
- 新增模块、修改公共头契约或变更硬件映射时，同步更新 `doc/` 或板卡 `hardware-map.md` / `README.md`。
- 新增目录或文档时，同步维护 `tools/doc_map.json`。

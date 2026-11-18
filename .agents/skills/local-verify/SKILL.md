---
name: local-verify
description: >-
  Local multi-dimensional verification workflow using tools/check.py: hygiene, formatting,
  conventions, layer boundaries, clang-tidy, host CTest contract tests, F1/F4 platform checks, and board ELF builds.
---

# 本地多维度测试流程

## 1. 分级执行命令
```bash
# 快速检查（日常每步修改后，秒级）
python3 tools/check.py --profile quick

# PR / 合入前检查（含主机契约测试、clang-tidy、F1/F4 Platform 全实例编译）
python3 tools/check.py --profile pr

# 全量检查（含 3 块板级固件构建与 ELF 静态分配/体积门禁）
python3 tools/check.py --profile full
```

## 2. 单独运行某一维度
```bash
python3 tools/check.py --only hygiene,conventions,layers
python3 tools/check.py --only host-test
python3 tools/check.py --only clang-tidy
python3 tools/check.py --only platform-check
python3 tools/check.py --only board-build
```

## 3. 结果语义
- `PASS`：检查通过。
- `FAIL`：存在阻断性问题，必须修复。
- `SKIP`：本机缺少外部工具（例如未安装 `clang-tidy` 或 `clang-format`），不会伪装成 `PASS`。

## 4. 缺失 `clang-tidy` / `clang-format` 时
若提示 `SKIP`（缺少 `clang-tidy`），请用户在本地终端手动安装：
```bash
sudo apt update && sudo apt install -y clang-tidy clang-format
```
安装后重新运行 `python3 tools/check.py --only clang-tidy` 即可自动生效。

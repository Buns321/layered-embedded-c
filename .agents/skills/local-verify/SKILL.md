---
name: local-verify
description: >-
  Local multi-dimensional verification workflow using tools/check.py: hygiene, formatting,
  static-alloc, layer boundaries, budgets, host CTest, clang-tidy, and Debug/Release ELF checks.
---

# 本地多维度测试流程

## 1. 分级执行命令
```bash
# 快速检查（秒级）：卫生、格式、静态分配、分层、文档映射、行数预算
python3 tools/check.py --profile quick --scope all

# PR / 合入前检查：quick + 14 项主机 CTest + clang-tidy
python3 tools/check.py --profile pr --scope all

# 全量检查：pr + 根目录 Debug/Release 构建 + ELF 动态符号/栈溢出钩子/体积红线检查
python3 tools/check.py --profile full --scope all
```

## 2. 单独运行某一维度
```bash
python3 tools/check.py --only hygiene,format,static-alloc,layers,docs,budgets --scope all
python3 tools/check.py --only host-test
python3 tools/check.py --only clang-tidy
python3 tools/check.py --only board-build
```

## 3. 结果语义
- `PASS`：检查通过。
- `FAIL`：存在阻断性问题，必须修复（含行数预算或 ELF 体积/符号超标）。
- `SKIP`：本机缺少外部工具（例如未安装 `clang-tidy` 或 `clang-format`），不会伪装成 `PASS`。

## 4. 缺失 `clang-tidy` / `clang-format` 时
若提示 `SKIP`，请用户在本地终端手动安装：
```bash
sudo apt update && sudo apt install -y clang-tidy clang-format
```
安装后重新运行 `python3 tools/check.py --only clang-tidy` 即可自动生效。

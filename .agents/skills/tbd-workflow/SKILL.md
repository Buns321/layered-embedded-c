---
name: tbd-workflow
description: >-
  Trunk-Based Development (TBD) workflow for layered-embedded-c: branching from main,
  small-step commits, handling [auto-commit] checkpoints, and merging back cleanly.
---

# TBD（主干开发）工作流

## 1. 开始工作前
1. 检查当前分支：`git branch --show-current`。
2. 若当前在 `main` 上，立即切出短生命周期分支：
   ```bash
   git fetch origin
   git checkout -b <type>/<short-topic> origin/main
   ```
   推荐前缀：`feat/`、`fix/`、`refactor/`、`docs/`、`test/`、`chore/`。

## 2. 开发过程中
- **小步快跑**：单分支聚焦单一主题，生命周期控制在 1–2 天内。
- **频繁同步主干**：
  ```bash
  git fetch origin && git rebase origin/main
  ```
- **每轮自动 Checkpoint**：对话结束时 `Stop` hook 会自动在当前工作分支提交带 `[auto-commit]` 的 checkpoint。

## 3. 合入 `main` 前
1. 整理零散的 `[auto-commit]` 提交（交互式 squash/reword）：
   ```bash
   git rebase -i origin/main
   ```
2. 运行本地门禁验证：
   ```bash
   python3 tools/check.py --profile pr
   ```
3. 合并到 `main`（推荐使用快进或 squash merge）并删除已合并的短分支。

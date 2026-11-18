#!/usr/bin/env python3
"""每轮对话结束自动提交 Hook (Stop)。

方案 A（Checkpoint 模式）：
- 仅在工作分支提交，严禁在 main / master / detached HEAD 上提交。
- 无改动、处于 merge/rebase 状态、或用户取消/报错中断时跳过。
- 基于实际变更文件生成简洁的 Conventional Commit 标题（附 [auto-commit]），
  并将用户指令摘要与变更列表写入 commit body。
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

PROTECTED_BRANCHES = {"main", "master", "HEAD"}
MAX_AUTO_STAGE_BYTES = 2 * 1024 * 1024  # 2 MB 安全上限，防止误提交二进制大文件


def run_git(args: list[str], cwd: Path) -> subprocess.CompletedProcess:
    return subprocess.run(
        ["git", *args],
        cwd=str(cwd),
        capture_output=True,
        text=True,
    )


def detect_scope(changed_files: list[str]) -> str:
    scopes: list[str] = []
    for f in changed_files:
        top = f.split("/", 1)[0]
        if top == "Boards" and len(f.split("/")) >= 2:
            board = f.split("/")[1]
            if "f407" in board:
                top = "f407"
            elif "f103" in board:
                top = "f103"
            else:
                top = "boards"
        elif top in (".agents", ".githooks", "AGENTS.md", ".clang-tidy", ".clang-format"):
            top = "agent"
        else:
            top = top.lower()
        if top not in scopes:
            scopes.append(top)
    if not scopes:
        return "repo"
    return scopes[0] if len(scopes) == 1 else "multi"


def detect_commit_type(changed_files: list[str], user_input: str) -> str:
    lower_in = user_input.lower()
    if all(f.startswith("doc/") or f.endswith(".md") for f in changed_files):
        return "docs"
    if any(k in lower_in for k in ("format", "clang-format", "格式化")):
        return "style"
    if any(k in lower_in for k in ("fix", "bug", "修复", "报错", "失败")):
        return "fix"
    if any(k in lower_in for k in ("refactor", "重构", "拆分", "迁移", "整理")):
        return "refactor"
    if any(k in lower_in for k in ("test", "测试", "check", "lint")):
        return "test"
    if all(
        f.startswith((".agents/", ".githooks/", "tools/"))
        or f in ("AGENTS.md", ".clang-tidy", ".clang-format", ".gitignore")
        for f in changed_files
    ):
        return "chore"
    return "feat"


def build_file_summary(status_lines: list[tuple[str, str]]) -> str:
    if not status_lines:
        return "checkpoint changes"

    verbs = {"A": "add", "M": "update", "D": "remove", "R": "rename"}
    codes = {code[0] for code, _ in status_lines}
    verb = verbs.get(next(iter(codes)), "update") if len(codes) == 1 else "update"

    names = [Path(p).name for _, p in status_lines]
    if len(names) == 1:
        return f"{verb} {names[0]}"
    if len(names) == 2:
        return f"{verb} {names[0]} and {names[1]}"
    return f"{verb} {names[0]}, {names[1]} (+{len(names) - 2} files)"


def build_commit_message(
    status_lines: list[tuple[str, str]],
    kept_files: list[str],
    user_input: str,
) -> str:
    ctype = detect_commit_type(kept_files, user_input)
    scope = detect_scope(kept_files)
    summary = build_file_summary(status_lines)
    title = f"{ctype}({scope}): {summary} [auto-commit]"

    body_lines: list[str] = []
    clean_prompt = re.sub(r"\s+", " ", user_input).strip()
    if clean_prompt:
        if len(clean_prompt) > 120:
            clean_prompt = clean_prompt[:117].rstrip() + "..."
        body_lines.append(f"Prompt: {clean_prompt}")
    if kept_files:
        body_lines.append("Files:")
        for f in kept_files[:15]:
            body_lines.append(f"- {f}")
        if len(kept_files) > 15:
            body_lines.append(f"- ... (+{len(kept_files) - 15} more)")

    if not body_lines:
        return title
    return title + "\n\n" + "\n".join(body_lines)


def main() -> None:
    try:
        payload = json.load(sys.stdin)
    except Exception:
        print(json.dumps({}))
        return

    if payload.get("continuationActive"):
        print(json.dumps({}))
        return
    if payload.get("terminationReason") in ("USER_CANCELED", "ERROR"):
        print(json.dumps({}))
        return

    cwd = Path(payload.get("cwd") or ".").resolve()

    # 1. 检查是否在 git 仓库且不在受保护分支
    br = run_git(["rev-parse", "--abbrev-ref", "HEAD"], cwd)
    if br.returncode != 0:
        print(json.dumps({}))
        return
    branch = br.stdout.strip()
    if branch in PROTECTED_BRANCHES:
        print(json.dumps({}))
        return

    # 2. 检查是否处于 merge / rebase / cherry-pick 中
    git_dir_res = run_git(["rev-parse", "--git-dir"], cwd)
    if git_dir_res.returncode == 0:
        git_dir = Path(git_dir_res.stdout.strip())
        if not git_dir.is_absolute():
            git_dir = cwd / git_dir
        for marker in ("MERGE_HEAD", "REBASE_HEAD", "CHERRY_PICK_HEAD", "rebase-merge", "rebase-apply"):
            if (git_dir / marker).exists():
                print(json.dumps({}))
                return

    # 3. 检查工作区是否有未提交更改
    status = run_git(["status", "--porcelain"], cwd)
    if status.returncode != 0 or not status.stdout.strip():
        print(json.dumps({}))
        return

    # 4. 暂存变更（排除超大文件）
    run_git(["add", "-A"], cwd)
    ns_res = run_git(["diff", "--cached", "--name-status"], cwd)
    status_lines: list[tuple[str, str]] = []
    for line in ns_res.stdout.splitlines():
        parts = line.split("\t")
        if len(parts) >= 2:
            code = parts[0]
            rel = parts[-1].strip()
            fpath = cwd / rel
            if fpath.is_file() and fpath.stat().st_size > MAX_AUTO_STAGE_BYTES:
                run_git(["reset", "HEAD", "--", rel], cwd)
            else:
                status_lines.append((code, rel))

    if not status_lines:
        print(json.dumps({}))
        return

    kept_files = [rel for _, rel in status_lines]
    user_input = payload.get("lastUserInput") or ""
    msg = build_commit_message(status_lines, kept_files, user_input)

    run_git(["commit", "--no-verify", "-m", msg], cwd)
    print(json.dumps({}))


if __name__ == "__main__":
    main()

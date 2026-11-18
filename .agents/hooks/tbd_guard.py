#!/usr/bin/env python3
"""TBD 分支保护与 sudo 拦截 Hook。

- SessionStart: 若当前处于 main/master 或 detached HEAD，注入系统提示要求先切出工作分支。
- PreToolUse(run_command):
  1. 禁止在 main/master 分支执行 git commit / git push / git merge。
  2. 禁止执行 sudo 命令，提示交由用户在终端手动运行。
"""

from __future__ import annotations

import json
import re
import subprocess
import sys

PROTECTED_BRANCHES = {"main", "master", "HEAD"}


def current_branch(cwd: str) -> str:
    try:
        out = subprocess.check_output(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            cwd=cwd,
            stderr=subprocess.DEVNULL,
            text=True,
        )
        return out.strip()
    except Exception:
        return ""


def handle_session_start(cwd: str) -> dict:
    branch = current_branch(cwd)
    if branch in PROTECTED_BRANCHES:
        msg = (
            f"[TBD 守卫] 当前处于受保护分支 '{branch}'。"
            "根据主干开发规范，严禁直接在 main 分支开发或提交。"
            "请在修改代码前先切出短生命周期工作分支（如 feat/*、fix/*、refactor/*、docs/*）。"
        )
        return {
            "injectSteps": [
                {"systemMessage": {"systemMessage": msg}}
            ]
        }
    return {}


def handle_pre_tool_use(payload: dict, cwd: str) -> dict:
    tool_call = payload.get("toolCall") or {}
    if tool_call.get("name") != "run_command":
        return {"decision": "allow"}

    cmd = (tool_call.get("args") or {}).get("CommandLine") or ""

    # 1. 拦截 sudo 命令
    if re.search(r"(^|[;&|\s])sudo(\s|$)", cmd):
        return {
            "decision": "deny",
            "reason": (
                "禁止在 Agent 自动化命令中运行 sudo（无交互式密码输入且越权）。"
                "请向用户说明所需安装的系统包或命令，由用户在本地终端手动执行。"
            ),
        }

    # 2. 拦截在受保护分支上的直接提交或推送
    branch = current_branch(cwd)
    if branch in PROTECTED_BRANCHES:
        if re.search(r"\bgit\s+(commit|push|merge|cherry-pick)\b", cmd):
            return {
                "decision": "deny",
                "reason": (
                    f"[TBD 守卫] 当前位于受保护分支 '{branch}'，严禁直接执行 git commit/push/merge。"
                    "请先通过 `git checkout -b <type>/<topic>` 切到工作分支。"
                ),
            }

    return {"decision": "allow"}


def main() -> None:
    try:
        payload = json.load(sys.stdin)
    except Exception:
        print(json.dumps({}))
        return

    event = payload.get("hookEventName", "")
    cwd = payload.get("cwd") or "."

    if event == "SessionStart":
        result = handle_session_start(cwd)
    elif event == "PreToolUse":
        result = handle_pre_tool_use(payload, cwd)
    else:
        result = {}

    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()

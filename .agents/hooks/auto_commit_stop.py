#!/usr/bin/env python3
"""每轮对话结束自动提交 Hook (Stop)。

方案 A（Checkpoint 模式）：
- 仅在工作分支提交，严禁在 main / master / detached HEAD 上提交。
- 无改动、处于 merge/rebase 状态、或用户取消/报错中断时跳过。
- 从当前轮次 transcript 与 git diff 提取上下文，调用 Opencode 的免费
  `opencode/deepseek-v4.1-flash-free` 模型生成保留动机、背景与改动意图的
  Conventional Commit 摘要（附 `[auto-commit]`）；若模型调用不可用或超时，
  则静默回退到本地规则生成简洁标题。
"""

from __future__ import annotations

import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

PROTECTED_BRANCHES = {"main", "master", "HEAD"}
MAX_AUTO_STAGE_BYTES = 2 * 1024 * 1024  # 2 MB 安全上限，防止误提交二进制大文件
OPENCODE_MODEL = "opencode/deepseek-v4.1-flash-free"
OPENCODE_TIMEOUT_SEC = 30
MAX_TURN_CONTEXT_CHARS = 5000
MAX_DIFF_CHARS = 6000

COMMIT_PROMPT = (
    "You are a Git commit message generator for an embedded C (STM32) repository.\n"
    "Analyze the conversation turn context and staged git diff from stdin, decide the\n"
    "most accurate Conventional Commit <type> and <scope> semantically, and output\n"
    "ONLY the formatted commit message.\n\n"
    "Output format (strictly follow this structure):\n"
    "<type>(<scope>): <concise summary of what & why> [auto-commit]\n\n"
    "Why: <1-2 sentences explaining motivation, background, or problem solved>\n"
    "What:\n"
    "- <key implementation or design change 1 (no bare filenames)>\n"
    "- <key implementation or design change 2 (optional, max 3 bullets)>\n\n"
    "Type decision rules (choose exactly one):\n"
    "- feat: new feature, driver, module, API, or capability\n"
    "- fix: bug fix, warning/error fix, hardware/timing/logic correction\n"
    "- refactor: code/interface/layering restructuring without changing behavior\n"
    "- docs: documentation, Doxygen comments, README, AGENTS.md, doc/\n"
    "- test: unit tests (Tests/), test cases, or verification checks\n"
    "- style: formatting (clang-format), whitespace, or naming-only cleanup\n"
    "- chore: build/CMake, .agents/ hooks/skills, tooling, or repo config\n\n"
    "Scope guidance:\n"
    "- Prefer lowercase module/layer name: bsp, framework, chassis, control,\n"
    "  drivers, core, user, f407, docs, tests, tools, agent, or multi.\n\n"
    "Constraints:\n"
    "- Write summary, Why, and What in concise Chinese (keep C/code identifiers in English).\n"
    "- Subject line <= 72 chars and MUST end with [auto-commit].\n"
    "- Do NOT list modified file paths (no 'Files:' section).\n"
    "- Do NOT echo raw user prompt (no 'Prompt:' line).\n"
    "- Plain text only: no markdown code fences, no preamble, no extra commentary."
)


def run_git(args: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
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
    """离线兜底：仅在 Opencode LLM 不可用或超时时用于推断提交类型。"""
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


def build_fallback_message(
    status_lines: list[tuple[str, str]],
    kept_files: list[str],
    user_input: str,
) -> str:
    ctype = detect_commit_type(kept_files, user_input)
    scope = detect_scope(kept_files)
    summary = build_file_summary(status_lines)
    return f"{ctype}({scope}): {summary} [auto-commit]"


def _clean_user_text(raw: str) -> str:
    m = re.search(r"<USER_REQUEST>\s*(.*?)\s*</USER_REQUEST>", raw, re.DOTALL)
    text = m.group(1) if m else raw
    text = re.sub(r"<[A-Z_]+>.*?</[A-Z_]+>", "", text, flags=re.DOTALL)
    return re.sub(r"\s+", " ", text).strip()


def extract_turn_context(transcript_path: str | None, fallback_user_input: str) -> str:
    """从 transcript.jsonl 提取最后一轮对话的用户输入、模型思考与最终回复。"""
    if not transcript_path:
        cleaned = _clean_user_text(fallback_user_input)
        return f"User: {cleaned}" if cleaned else ""

    path = Path(transcript_path)
    if not path.is_file():
        cleaned = _clean_user_text(fallback_user_input)
        return f"User: {cleaned}" if cleaned else ""

    entries: list[dict] = []
    try:
        with path.open("r", encoding="utf-8", errors="replace") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    entries.append(json.loads(line))
                except json.JSONDecodeError:
                    continue
    except OSError:
        return ""

    last_user_idx = -1
    for i, item in enumerate(entries):
        if item.get("type") == "USER_INPUT":
            last_user_idx = i

    turn_entries = entries[last_user_idx:] if last_user_idx >= 0 else entries[-10:]
    parts: list[str] = []

    for item in turn_entries:
        etype = item.get("type")
        if etype == "USER_INPUT":
            user_txt = _clean_user_text(item.get("content") or "")
            if user_txt:
                parts.append(f"User: {user_txt[:600]}")
        elif etype == "PLANNER_RESPONSE":
            thinking = (item.get("thinking") or "").strip()
            if thinking:
                parts.append(f"Assistant intent: {thinking[:500]}")
            tools = item.get("tool_calls") or []
            if tools:
                tool_names = ", ".join(t.get("name", "") for t in tools if t.get("name"))
                if tool_names:
                    parts.append(f"Tools used: {tool_names}")
            content = (item.get("content") or "").strip()
            if content:
                parts.append(f"Assistant reply: {content[:1200]}")

    joined = "\n".join(parts)
    if len(joined) > MAX_TURN_CONTEXT_CHARS:
        joined = joined[: MAX_TURN_CONTEXT_CHARS - 20].rstrip() + "\n...[truncated]"
    return joined


def collect_diff_context(cwd: Path) -> str:
    stat_res = run_git(["diff", "--cached", "--stat"], cwd)
    diff_res = run_git(["diff", "--cached", "-U2"], cwd)
    stat_txt = stat_res.stdout.strip()
    diff_txt = diff_res.stdout.strip()
    if len(diff_txt) > MAX_DIFF_CHARS:
        diff_txt = diff_txt[: MAX_DIFF_CHARS - 20].rstrip() + "\n...[diff truncated]"
    sections = []
    if stat_txt:
        sections.append(f"Diff stat:\n{stat_txt}")
    if diff_txt:
        sections.append(f"Staged diff:\n{diff_txt}")
    return "\n\n".join(sections)


def _sanitize_commit_msg(raw: str) -> str:
    text = raw.replace("\r\n", "\n").strip()
    # 剥离可能出现的代码块围栏
    text = re.sub(r"^```[a-zA-Z0-9_-]*\n", "", text)
    text = re.sub(r"\n```$", "", text).strip()
    if not text:
        return ""

    lines = text.splitlines()
    title = lines[0].strip()
    if not title:
        return ""
    if "[auto-commit]" not in title:
        title = f"{title} [auto-commit]"
    lines[0] = title
    return "\n".join(lines).strip()


def generate_commit_message_with_opencode(
    turn_context: str,
    diff_context: str,
) -> str | None:
    if not shutil.which("opencode"):
        return None

    stdin_payload = "\n\n".join(s for s in (turn_context, diff_context) if s)
    if not stdin_payload:
        return None

    try:
        proc = subprocess.run(
            [
                "opencode",
                "run",
                "-m",
                OPENCODE_MODEL,
                "--pure",
                "--format",
                "json",
                COMMIT_PROMPT,
            ],
            input=stdin_payload,
            cwd=tempfile.gettempdir(),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=OPENCODE_TIMEOUT_SEC,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None

    if proc.returncode != 0 or not proc.stdout:
        return None

    chunks: list[str] = []
    for line in proc.stdout.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            evt = json.loads(line)
        except json.JSONDecodeError:
            continue
        if evt.get("type") == "text":
            part = evt.get("part") or {}
            txt = part.get("text")
            if isinstance(txt, str):
                chunks.append(txt)

    return _sanitize_commit_msg("".join(chunks)) or None


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
    turn_context = extract_turn_context(payload.get("transcriptPath"), user_input)
    diff_context = collect_diff_context(cwd)

    msg = generate_commit_message_with_opencode(turn_context, diff_context)
    if not msg:
        msg = build_fallback_message(status_lines, kept_files, user_input)

    run_git(["commit", "--no-verify", "-m", msg], cwd)
    print(json.dumps({}))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""文档驱动门禁 Hook (PreToolUse: write_file | edit_file)。

根据 tools/doc_map.json 映射，检查当前会话 transcript 中是否已经通过 read_file
阅读过目标文件对应的必读文档。未读则 deny 并列出需先阅读的文档路径。
"""

from __future__ import annotations

import json
import os
import re
import sys
from pathlib import Path


def load_doc_map(repo_root: Path) -> list[dict]:
    map_path = repo_root / "tools" / "doc_map.json"
    if not map_path.is_file():
        return []
    try:
        data = json.loads(map_path.read_text(encoding="utf-8"))
        return data.get("rules", [])
    except Exception:
        return []


def match_required_docs(rel_path: str, rules: list[dict]) -> list[str]:
    docs: list[str] = []
    seen = set()
    for rule in rules:
        patterns = rule.get("paths", [])
        if any(re.search(pat, rel_path) for pat in patterns):
            for doc in rule.get("docs", []):
                if doc not in seen:
                    seen.add(doc)
                    docs.append(doc)
    return docs


def read_docs_from_transcript(transcript_path: str, repo_root: Path) -> set[str]:
    read_files: set[str] = set()
    if not transcript_path or not os.path.isfile(transcript_path):
        return read_files
    try:
        with open(transcript_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    step = json.loads(line)
                except Exception:
                    continue
                for tc in step.get("tool_calls") or []:
                    name = tc.get("name") or ""
                    if name not in ("read_file", "view_file"):
                        continue
                    args = tc.get("args") or {}
                    p = args.get("Path") or args.get("AbsolutePath") or ""
                    if not p:
                        continue
                    try:
                        rel = Path(p).resolve().relative_to(repo_root.resolve()).as_posix()
                        read_files.add(rel)
                    except Exception:
                        pass
    except Exception:
        pass
    return read_files


def main() -> None:
    try:
        payload = json.load(sys.stdin)
    except Exception:
        print(json.dumps({"decision": "allow"}))
        return

    cwd = Path(payload.get("cwd") or ".").resolve()
    tool_call = payload.get("toolCall") or {}
    args = tool_call.get("args") or {}
    target_raw = args.get("Path") or args.get("TargetFile") or ""

    if not target_raw:
        print(json.dumps({"decision": "allow"}))
        return

    target_path = Path(target_raw)
    if not target_path.is_absolute():
        target_path = (cwd / target_path).resolve()
    else:
        target_path = target_path.resolve()

    # 非仓库内文件（如 artifact/scratch 目录）直接放行
    try:
        rel_target = target_path.relative_to(cwd).as_posix()
    except ValueError:
        print(json.dumps({"decision": "allow"}))
        return

    # 跳过 .agents/ 与 tools/ 自身配置编辑，避免鸡生蛋死锁
    if rel_target.startswith((".agents/", ".githooks/", "AGENTS.md")):
        print(json.dumps({"decision": "allow"}))
        return

    rules = load_doc_map(cwd)
    required = [d for d in match_required_docs(rel_target, rules) if (cwd / d).is_file()]
    if not required:
        print(json.dumps({"decision": "allow"}))
        return

    transcript_path = payload.get("transcriptPath") or ""
    already_read = read_docs_from_transcript(transcript_path, cwd)
    missing = [d for d in required if d not in already_read]

    if missing:
        doc_list = ", ".join(missing)
        print(
            json.dumps(
                {
                    "decision": "deny",
                    "reason": (
                        f"[文档驱动门禁] 修改 '{rel_target}' 前，请先使用 read_file 阅读对应规范文档：{doc_list}"
                    ),
                },
                ensure_ascii=False,
            )
        )
        return

    print(json.dumps({"decision": "allow"}))


if __name__ == "__main__":
    main()

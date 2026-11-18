#!/usr/bin/env python3
"""layered-embedded-c 本地多维度验证入口。

Profiles:
  quick : hygiene, format, conventions, layers, docs
  pr    : quick + host-test, clang-tidy, platform-check
  full  : pr + board-build (含 ELF 体积与静态分配符号门禁)
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Callable

REPO_ROOT = Path(__file__).resolve().parent.parent

OWNED_TOP_DIRS = (
    "Platform",
    "DeviceDrivers",
    "Controller",
    "Policy",
    "Filter",
    "System",
)

THIRD_PARTY_SEGMENTS = ("/Drivers/", "/Middlewares/", "/build/", "/.git/")

PROFILES = {
    "quick": ["hygiene", "format", "conventions", "layers", "docs"],
    "pr": [
        "hygiene",
        "format",
        "conventions",
        "layers",
        "docs",
        "host-test",
        "clang-tidy",
        "platform-check",
    ],
    "full": [
        "hygiene",
        "format",
        "conventions",
        "layers",
        "docs",
        "host-test",
        "clang-tidy",
        "platform-check",
        "board-build",
    ],
}


class SkipCheck(Exception):
    """缺少外部工具或前置条件时标记为 SKIP。"""


class FailCheck(Exception):
    """检查未通过时抛出。"""


def run_cmd(cmd: list[str], cwd: Path = REPO_ROOT, verbose: bool = False) -> subprocess.CompletedProcess:
    if verbose:
        print(f"  $ {' '.join(cmd)} (cwd={cwd})")
    return subprocess.run(cmd, cwd=str(cwd), capture_output=True, text=True)


def is_owned_path(rel: str) -> bool:
    norm = "/" + rel.lstrip("/")
    if any(seg in norm for seg in THIRD_PARTY_SEGMENTS):
        return False
    if rel.startswith(OWNED_TOP_DIRS):
        return True
    if rel.startswith("Boards/"):
        parts = rel.split("/")
        # Boards/<board>/(App|RTOS|Core/Board)/...
        if len(parts) >= 3 and parts[2] in ("App", "RTOS"):
            return True
        if len(parts) >= 4 and parts[2] == "Core" and parts[3] == "Board":
            return True
    return False


def collect_files(scope: str) -> list[str]:
    if scope == "staged":
        res = run_cmd(["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"])
        files = [l.strip() for l in res.stdout.splitlines() if l.strip()]
    elif scope == "branch":
        base = run_cmd(["git", "merge-base", "HEAD", "main"])
        if base.returncode == 0 and base.stdout.strip():
            res = run_cmd(["git", "diff", "--name-only", "--diff-filter=ACMR", base.stdout.strip(), "HEAD"])
            committed = [l.strip() for l in res.stdout.splitlines() if l.strip()]
        else:
            committed = []
        wt = run_cmd(["git", "status", "--porcelain"])
        dirty = []
        for line in wt.stdout.splitlines():
            if not line.strip() or line.startswith("D "):
                continue
            path_part = line[3:].strip()
            if " -> " in path_part:
                path_part = path_part.split(" -> ", 1)[1]
            dirty.append(path_part)
        files = sorted(set(committed + dirty))
    else:
        res = run_cmd(["git", "ls-files"])
        files = [l.strip() for l in res.stdout.splitlines() if l.strip()]
    return [f for f in files if (REPO_ROOT / f).is_file()]


# ---------------------------------------------------------------------------
# 1. Hygiene
# ---------------------------------------------------------------------------
def check_hygiene(files: list[str], verbose: bool) -> str:
    errors: list[str] = []
    text_exts = {".c", ".h", ".md", ".py", ".json", ".cmake", ".txt", ".sh"}
    for rel in files:
        if not (is_owned_path(rel) or rel.startswith(("doc/", "tools/", ".agents/", ".githooks/"))):
            continue
        p = REPO_ROOT / rel
        if p.suffix not in text_exts and p.name != "CMakeLists.txt":
            continue
        raw = p.read_bytes()
        if b"\r\n" in raw:
            errors.append(f"{rel}: 存在 CRLF 换行符（要求 LF）")
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            errors.append(f"{rel}: 非 UTF-8 编码")
            continue
        for idx, line in enumerate(text.splitlines(), 1):
            if line.endswith((" ", "\t")):
                errors.append(f"{rel}:{idx}: 行尾有多余空白")
                break
    if errors:
        raise FailCheck("\n".join(errors[:20]))
    return f"checked {len(files)} file(s)"


# ---------------------------------------------------------------------------
# 2. Format (clang-format)
# ---------------------------------------------------------------------------
def check_format(files: list[str], verbose: bool) -> str:
    cf = shutil.which("clang-format")
    if not cf:
        raise SkipCheck("未安装 clang-format")
    c_files = [f for f in files if is_owned_path(f) and f.endswith((".c", ".h"))]
    if not c_files:
        return "no owned C/H files in scope"
    res = run_cmd([cf, "--dry-run", "--Werror", *c_files], verbose=verbose)
    if res.returncode != 0:
        raise FailCheck((res.stderr or res.stdout).strip()[:1500])
    return f"formatted OK ({len(c_files)} C/H files)"


# ---------------------------------------------------------------------------
# 3. Conventions
# ---------------------------------------------------------------------------
def check_conventions(files: list[str], verbose: bool) -> str:
    c_files = [f for f in files if is_owned_path(f) and f.endswith((".c", ".h"))]
    errors: list[str] = []
    alloc_re = re.compile(r"\b(malloc|calloc|realloc|free)\s*\(")
    anon_typedef_re = re.compile(r"^\s*typedef\s+(struct|enum|union)\s*\{")

    for rel in c_files:
        lines = (REPO_ROOT / rel).read_text(encoding="utf-8", errors="replace").splitlines()
        in_block_comment = False
        for idx, line in enumerate(lines, 1):
            stripped = line.strip()
            if "/*" in stripped:
                in_block_comment = True
            if in_block_comment:
                if "*/" in stripped:
                    in_block_comment = False
                continue
            if stripped.startswith("//"):
                continue
            m = alloc_re.search(line)
            if m:
                errors.append(f"{rel}:{idx}: 禁止动态内存调用 '{m.group(1)}()' [doc/architecture.md]")
            if rel.endswith(".h") and "/Inc/" in f"/{rel}":
                if anon_typedef_re.match(line):
                    errors.append(f"{rel}:{idx}: 公共头文件禁止匿名 typedef struct/enum/union [doc/conventions.md]")

    if errors:
        raise FailCheck("\n".join(errors[:20]))
    return f"checked {len(c_files)} owned C/H files"


# ---------------------------------------------------------------------------
# 4. Layers
# ---------------------------------------------------------------------------
def check_layers(files: list[str], verbose: bool) -> str:
    c_files = [f for f in files if is_owned_path(f) and f.endswith((".c", ".h"))]
    include_re = re.compile(r'^\s*#\s*include\s+[<"]([^">]+)[">]')
    errors: list[str] = []

    hal_or_rtos = ("stm32f1xx", "stm32f4xx", "FreeRTOS", "cmsis_os", "main.h")

    for rel in c_files:
        lines = (REPO_ROOT / rel).read_text(encoding="utf-8", errors="replace").splitlines()
        for idx, line in enumerate(lines, 1):
            m = include_re.match(line)
            if not m:
                continue
            inc = m.group(1)

            # 1. 纯领域/算法共享层禁止包含 MCU HAL / FreeRTOS / 板级 main.h
            if rel.startswith(("DeviceDrivers/", "Controller/", "Policy/", "Filter/")):
                if any(tok in inc for tok in hal_or_rtos):
                    errors.append(f"{rel}:{idx}: 共享层禁止直接包含 '{inc}' [doc/architecture.md]")

            # 2. Platform 公共头文件禁止暴露 HAL
            if rel.startswith("Platform/Inc/") and any(tok in inc for tok in ("stm32f1xx", "stm32f4xx")):
                errors.append(f"{rel}:{idx}: Platform/Inc 禁止包含芯片 HAL '{inc}' [doc/platform-contract.md]")

            # 3. App 层禁止直接包含 HAL 或 Core/Board
            if re.match(r"^Boards/[^/]+/App/", rel):
                if any(tok in inc for tok in ("stm32f1xx_hal", "stm32f4xx_hal", "core_board")):
                    errors.append(f"{rel}:{idx}: App 层禁止直接依赖 '{inc}' [doc/framework-contract.md]")

    if errors:
        raise FailCheck("\n".join(errors[:20]))
    return f"layer boundaries OK ({len(c_files)} files)"


# ---------------------------------------------------------------------------
# 5. Docs
# ---------------------------------------------------------------------------
def check_docs(files: list[str], verbose: bool) -> str:
    map_path = REPO_ROOT / "tools" / "doc_map.json"
    if not map_path.is_file():
        raise FailCheck("缺少 tools/doc_map.json")
    try:
        data = json.loads(map_path.read_text(encoding="utf-8"))
    except Exception as exc:
        raise FailCheck(f"tools/doc_map.json 解析失败: {exc}")

    missing: list[str] = []
    for rule in data.get("rules", []):
        for pat in rule.get("paths", []):
            try:
                re.compile(pat)
            except re.error as exc:
                raise FailCheck(f"doc_map.json 正则无效 '{pat}': {exc}")
        for doc in rule.get("docs", []):
            if not (REPO_ROOT / doc).is_file():
                missing.append(doc)
    if missing:
        raise FailCheck(f"doc_map.json 引用了不存在的文档: {', '.join(sorted(set(missing)))}")
    return f"{len(data.get('rules', []))} mapping rules valid"


# ---------------------------------------------------------------------------
# 6. Host Contract Tests
# ---------------------------------------------------------------------------
def check_host_test(files: list[str], verbose: bool) -> str:
    if not shutil.which("cmake") or not shutil.which("ctest"):
        raise SkipCheck("缺少 cmake 或 ctest")
    src = REPO_ROOT / "cmake" / "platform-test"
    bld = REPO_ROOT / "build" / "platform-test"
    cfg = run_cmd(
        ["cmake", "-S", str(src), "-B", str(bld), "-G", "Ninja", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"],
        verbose=verbose,
    )
    if cfg.returncode != 0:
        raise FailCheck((cfg.stderr or cfg.stdout).strip()[-1500:])
    bld_res = run_cmd(["cmake", "--build", str(bld)], verbose=verbose)
    if bld_res.returncode != 0:
        raise FailCheck((bld_res.stderr or bld_res.stdout).strip()[-1500:])
    tst = run_cmd(["ctest", "--test-dir", str(bld), "--output-on-failure"], verbose=verbose)
    if tst.returncode != 0:
        raise FailCheck((tst.stdout + "\n" + tst.stderr).strip()[-2000:])
    summary = [l for l in tst.stdout.splitlines() if "tests passed" in l]
    return summary[-1] if summary else "ctest passed"


# ---------------------------------------------------------------------------
# 7. clang-tidy
# ---------------------------------------------------------------------------
def check_clang_tidy(files: list[str], verbose: bool) -> str:
    tidy = shutil.which("clang-tidy")
    if not tidy:
        raise SkipCheck("未安装 clang-tidy（可由用户在终端执行 sudo apt install -y clang-tidy）")
    compdb = REPO_ROOT / "build" / "platform-test" / "compile_commands.json"
    if not compdb.is_file():
        raise SkipCheck("缺少 build/platform-test/compile_commands.json（请先运行 host-test）")

    entries = json.loads(compdb.read_text(encoding="utf-8"))
    target_sources = sorted(
        {
            str(Path(e["file"]).resolve())
            for e in entries
            if is_owned_path(
                Path(e["file"]).resolve().relative_to(REPO_ROOT).as_posix()
                if Path(e["file"]).resolve().is_relative_to(REPO_ROOT)
                else ""
            )
        }
    )
    if not target_sources:
        return "no owned sources in compile_commands.json"

    res = run_cmd([tidy, "-p", str(compdb.parent), *target_sources], verbose=verbose)
    if res.returncode != 0:
        raise FailCheck((res.stdout + "\n" + res.stderr).strip()[-2000:])
    return f"clang-tidy OK ({len(target_sources)} translation units)"


# ---------------------------------------------------------------------------
# 8. Platform Check (F4 & F1)
# ---------------------------------------------------------------------------
def check_platform_check(files: list[str], verbose: bool) -> str:
    if not shutil.which("arm-none-eabi-gcc"):
        raise SkipCheck("未安装 arm-none-eabi-gcc")
    pc_dir = REPO_ROOT / "cmake" / "platform-check"
    for preset in ("f4-debug", "f1-debug"):
        c = run_cmd(["cmake", "--preset", preset], cwd=pc_dir, verbose=verbose)
        if c.returncode != 0:
            raise FailCheck(f"[{preset}] configure failed:\n{(c.stderr or c.stdout)[-1200:]}")
        b = run_cmd(["cmake", "--build", "--preset", preset], cwd=pc_dir, verbose=verbose)
        if b.returncode != 0:
            raise FailCheck(f"[{preset}] build failed:\n{(b.stderr or b.stdout)[-1200:]}")
    return "f4-debug + f1-debug passed"


# ---------------------------------------------------------------------------
# 9. Board Builds + ELF Static Allocation Gate
# ---------------------------------------------------------------------------
def check_board_build(files: list[str], verbose: bool) -> str:
    if not shutil.which("arm-none-eabi-gcc"):
        raise SkipCheck("未安装 arm-none-eabi-gcc")
    nm = shutil.which("arm-none-eabi-nm")
    size_bin = shutil.which("arm-none-eabi-size")

    boards = [
        "Boards/f407vet6-omni-car",
        "Boards/f103c8t6-platform-test",
        "Boards/f103c8t6-fast-channel-task-1",
    ]
    sizes: list[str] = []
    forbidden_syms = {"malloc", "calloc", "realloc", "free"}

    for b in boards:
        bdir = REPO_ROOT / b
        c = run_cmd(["cmake", "--preset", "Debug"], cwd=bdir, verbose=verbose)
        if c.returncode != 0:
            raise FailCheck(f"[{b}] configure failed:\n{(c.stderr or c.stdout)[-1200:]}")
        bld = run_cmd(["cmake", "--build", "--preset", "Debug"], cwd=bdir, verbose=verbose)
        if bld.returncode != 0:
            raise FailCheck(f"[{b}] build failed:\n{(bld.stderr or bld.stdout)[-1200:]}")

        elfs = list((bdir / "build" / "Debug").glob("*.elf"))
        for elf in elfs:
            if nm:
                nm_res = run_cmd([nm, "-u", str(elf)])
                for line in nm_res.stdout.splitlines():
                    sym = line.strip().split()[-1] if line.strip() else ""
                    if sym in forbidden_syms:
                        raise FailCheck(f"{elf.name} 引用了禁止的动态内存符号: {sym}")
            if size_bin:
                sz = run_cmd([size_bin, str(elf)])
                lines = sz.stdout.strip().splitlines()
                if len(lines) >= 2:
                    sizes.append(f"{elf.name}: {lines[1].strip()}")

    return " | ".join(sizes) if sizes else "3 boards built"


STAGES: dict[str, Callable[[list[str], bool], str]] = {
    "hygiene": check_hygiene,
    "format": check_format,
    "conventions": check_conventions,
    "layers": check_layers,
    "docs": check_docs,
    "host-test": check_host_test,
    "clang-tidy": check_clang_tidy,
    "platform-check": check_platform_check,
    "board-build": check_board_build,
}


def main() -> int:
    parser = argparse.ArgumentParser(description="layered-embedded-c 本地多维度检查")
    parser.add_argument("--profile", choices=PROFILES.keys(), default="quick")
    parser.add_argument("--scope", choices=("staged", "branch", "all"), default="branch")
    parser.add_argument("--only", help="仅运行指定阶段（逗号分隔）")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    stage_names = [s.strip() for s in args.only.split(",") if s.strip()] if args.only else PROFILES[args.profile]
    unknown = [s for s in stage_names if s not in STAGES]
    if unknown:
        print(f"未知检查阶段: {', '.join(unknown)}", file=sys.stderr)
        return 2

    files = collect_files(args.scope)
    failed = False

    for name in stage_names:
        try:
            detail = STAGES[name](files, args.verbose)
            print(f"[PASS] {name:<15} {detail}")
        except SkipCheck as exc:
            print(f"[SKIP] {name:<15} {exc}")
        except FailCheck as exc:
            failed = True
            print(f"[FAIL] {name:<15}\n{exc}")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

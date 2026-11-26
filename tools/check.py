#!/usr/bin/env python3
"""layered-embedded-c (v2) 本地多维度验证入口。

Profiles:
  quick : hygiene, format, static-alloc, layers, docs, budgets
  pr    : quick + host-test, clang-tidy
  full  : pr + board-build (Debug/Release 构建 + ELF 静态符号与体积红线检查)
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

OWNED_CODE_PREFIXES = (
    "BSP/",
    "Framework/Core/",
    "Framework/Control/",
    "Framework/Drivers/",
    "Framework/Chassis/",
    "User/",
    "tests/",
)

EXCLUDED_PREFIXES = (
    "Core/",
    "Drivers/",
    "Middlewares/",
    "Framework/ThirdParty/",
    "cmake/stm32cubemx/",
    "build/",
    ".git/",
)

OWNED_TEXT_PREFIXES = ("doc/", "tools/", ".agents/", ".githooks/", "cmake/")
OWNED_ROOT_TEXT_FILES = {
    "CMakeLists.txt",
    "CMakePresets.json",
    "AGENTS.md",
    "README.md",
}

BSP_MAX_FILES = 18
BSP_MAX_LINES = 2000
USER_MAX_LINES = 320
TESTS_MAX_LINES = 2800
F407_RELEASE_MAX_FLASH_BYTES = 63520
F407_RELEASE_MAX_RAM_BYTES = 20408

PROFILES = {
    "quick": ["hygiene", "format", "static-alloc", "layers", "docs", "budgets"],
    "pr": [
        "hygiene",
        "format",
        "static-alloc",
        "layers",
        "docs",
        "budgets",
        "host-test",
        "clang-tidy",
    ],
    "full": [
        "hygiene",
        "format",
        "static-alloc",
        "layers",
        "docs",
        "budgets",
        "host-test",
        "clang-tidy",
        "board-build",
    ],
}


class SkipCheck(Exception):
    """缺少外部工具或前置条件时标记为 SKIP。"""


class FailCheck(Exception):
    """检查未通过时抛出。"""


def run_cmd(
    cmd: list[str], cwd: Path = REPO_ROOT, verbose: bool = False
) -> subprocess.CompletedProcess:
    if verbose:
        print(f"  $ {' '.join(cmd)} (cwd={cwd})")
    return subprocess.run(cmd, cwd=str(cwd), capture_output=True, text=True)


def is_excluded_path(rel: str) -> bool:
    return rel.startswith(EXCLUDED_PREFIXES)


def is_owned_code_path(rel: str) -> bool:
    return (not is_excluded_path(rel)) and rel.startswith(OWNED_CODE_PREFIXES)


def is_owned_text_path(rel: str) -> bool:
    if is_excluded_path(rel):
        return False
    return (
        is_owned_code_path(rel)
        or rel.startswith(OWNED_TEXT_PREFIXES)
        or rel in OWNED_ROOT_TEXT_FILES
    )


def collect_files(scope: str) -> list[str]:
    if scope == "staged":
        res = run_cmd(["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"])
        files = [l.strip() for l in res.stdout.splitlines() if l.strip()]
    elif scope == "branch":
        base = run_cmd(["git", "merge-base", "HEAD", "main"])
        if base.returncode == 0 and base.stdout.strip():
            res = run_cmd(
                [
                    "git",
                    "diff",
                    "--name-only",
                    "--diff-filter=ACMR",
                    base.stdout.strip(),
                    "HEAD",
                ]
            )
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
        res = run_cmd(["git", "ls-files", "--cached", "--others", "--exclude-standard"])
        files = [l.strip() for l in res.stdout.splitlines() if l.strip()]
    return [f for f in files if (REPO_ROOT / f).is_file()]


# ---------------------------------------------------------------------------
# 1. Hygiene
# ---------------------------------------------------------------------------
def check_hygiene(files: list[str], verbose: bool) -> str:
    del verbose
    errors: list[str] = []
    text_exts = {".c", ".h", ".md", ".py", ".json", ".cmake", ".txt", ".sh", ".ps1"}
    checked = 0
    for rel in files:
        if not is_owned_text_path(rel):
            continue
        p = REPO_ROOT / rel
        if p.suffix not in text_exts and p.name != "CMakeLists.txt":
            continue
        checked += 1
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
    return f"checked {checked} owned text file(s)"


# ---------------------------------------------------------------------------
# 2. Format (clang-format)
# ---------------------------------------------------------------------------
def check_format(files: list[str], verbose: bool) -> str:
    cf = shutil.which("clang-format")
    if not cf:
        raise SkipCheck("未安装 clang-format")
    c_files = [f for f in files if is_owned_code_path(f) and f.endswith((".c", ".h"))]
    if not c_files:
        return "no owned C/H files in scope"
    res = run_cmd([cf, "--dry-run", "--Werror", *c_files], verbose=verbose)
    if res.returncode != 0:
        raise FailCheck((res.stderr or res.stdout).strip()[:1500])
    return f"formatted OK ({len(c_files)} C/H files)"


# ---------------------------------------------------------------------------
# 3. Static-Alloc & C Header Conventions
# ---------------------------------------------------------------------------
def is_public_header(rel: str) -> bool:
    if not rel.endswith(".h"):
        return False
    if rel.startswith("User/"):
        return True
    if rel.startswith(("BSP/Inc/", "Framework/")) and "/Inc/" in f"/{rel}":
        return True
    return False


def check_static_alloc(files: list[str], verbose: bool) -> str:
    del verbose
    c_files = [f for f in files if is_owned_code_path(f) and f.endswith((".c", ".h"))]
    errors: list[str] = []
    alloc_re = re.compile(r"\b(malloc|calloc|realloc|free)\s*\(")
    anon_typedef_re = re.compile(r"^\s*typedef\s+(struct|enum|union)\s*\{")

    for rel in c_files:
        lines = (REPO_ROOT / rel).read_text(encoding="utf-8", errors="replace").splitlines()
        in_block_comment = False
        pub_header = is_public_header(rel)
        for idx, line in enumerate(lines, 1):
            stripped = line.strip()
            if in_block_comment:
                if "*/" in stripped:
                    in_block_comment = False
                continue
            if "/*" in stripped:
                if "*/" not in stripped.split("/*", 1)[1]:
                    in_block_comment = True
                code_part = line.split("/*", 1)[0]
                if not code_part.strip():
                    continue
                line_to_check = code_part
            else:
                line_to_check = line

            if line_to_check.strip().startswith("//"):
                continue
            if "//" in line_to_check:
                line_to_check = line_to_check.split("//", 1)[0]

            m = alloc_re.search(line_to_check)
            if m:
                errors.append(
                    f"{rel}:{idx}: 禁止动态内存调用 '{m.group(1)}()' [doc/architecture.md]"
                )
            if pub_header and anon_typedef_re.match(line_to_check):
                errors.append(
                    f"{rel}:{idx}: 公共头文件禁止匿名 typedef struct/enum/union [doc/conventions.md]"
                )

    if errors:
        raise FailCheck("\n".join(errors[:20]))
    return f"checked {len(c_files)} owned C/H files (0 alloc, 0 anon typedef)"


# ---------------------------------------------------------------------------
# 4. Layers
# ---------------------------------------------------------------------------
def collect_header_names(directory: Path) -> set[str]:
    if not directory.is_dir():
        return set()
    return {p.name for p in directory.glob("*.h")}


def check_layers(files: list[str], verbose: bool) -> str:
    del verbose
    c_files = [f for f in files if is_owned_code_path(f) and f.endswith((".c", ".h"))]
    include_re = re.compile(r'^\s*#\s*include\s+[<"]([^">]+)[">]')
    errors: list[str] = []

    bsp_headers = collect_header_names(REPO_ROOT / "BSP" / "Inc")
    control_headers = collect_header_names(REPO_ROOT / "Framework" / "Control" / "Inc")
    driver_headers = collect_header_names(REPO_ROOT / "Framework" / "Drivers" / "Inc")
    chassis_headers = collect_header_names(REPO_ROOT / "Framework" / "Chassis" / "Inc")
    user_headers = collect_header_names(REPO_ROOT / "User")
    core_mx_headers = collect_header_names(REPO_ROOT / "Core" / "Inc")

    hal_prefixes = ("stm32f", "stm32g", "stm32h")
    rtos_names = {"FreeRTOS.h", "task.h", "cmsis_os.h", "cmsis_os2.h"}
    core_mx_non_os = core_mx_headers - {"FreeRTOSConfig.h"}

    for rel in c_files:
        lines = (REPO_ROOT / rel).read_text(encoding="utf-8", errors="replace").splitlines()
        for idx, line in enumerate(lines, 1):
            m = include_re.match(line)
            if not m:
                continue
            inc = m.group(1)
            inc_name = Path(inc).name
            is_hal = inc_name.startswith(hal_prefixes) or inc.startswith("BSP/")
            is_bsp = inc.startswith("BSP/") or inc_name in bsp_headers
            is_rtos = inc_name in rtos_names or "FreeRTOS" in inc
            is_core_mx = inc.startswith("Core/") or inc_name in core_mx_non_os

            if rel.startswith("BSP/"):
                if inc_name in (control_headers | driver_headers | chassis_headers | user_headers):
                    errors.append(f"{rel}:{idx}: BSP 禁止反向包含 '{inc}' [doc/architecture.md]")

            elif rel.startswith("Framework/Core/"):
                if inc_name in (control_headers | driver_headers | chassis_headers | user_headers):
                    errors.append(
                        f"{rel}:{idx}: Framework/Core 禁止反向包含 '{inc}' [doc/architecture.md]"
                    )

            elif rel.startswith("Framework/Control/"):
                if (
                    is_hal
                    or is_bsp
                    or is_rtos
                    or is_core_mx
                    or inc_name in (driver_headers | chassis_headers | user_headers)
                ):
                    errors.append(
                        f"{rel}:{idx}: Framework/Control 禁止包含 '{inc}'（须 100% HAL-Free）[doc/architecture.md]"
                    )

            elif rel.startswith("Framework/Drivers/"):
                if inc_name in (chassis_headers | user_headers) or inc_name == "main.h":
                    errors.append(
                        f"{rel}:{idx}: Framework/Drivers 禁止包含 '{inc}' [doc/architecture.md]"
                    )

            elif rel.startswith("Framework/Chassis/"):
                if inc_name in user_headers or inc_name == "main.h":
                    errors.append(
                        f"{rel}:{idx}: Framework/Chassis 禁止包含 '{inc}' [doc/architecture.md]"
                    )

            elif rel.startswith("User/app_"):
                if is_hal or is_bsp or is_rtos or is_core_mx:
                    errors.append(
                        f"{rel}:{idx}: User/app_* 禁止包含 HAL/BSP/FreeRTOS/CubeMX 头 '{inc}' [doc/architecture.md]"
                    )

            elif rel.startswith("User/rtos_"):
                if (
                    is_hal
                    or is_bsp
                    or inc_name in (driver_headers | chassis_headers | control_headers)
                    or inc_name in (core_mx_non_os - {"FreeRTOSConfig.h"})
                ):
                    errors.append(
                        f"{rel}:{idx}: User/rtos_* 禁止直接依赖 '{inc}' [doc/architecture.md]"
                    )

    if errors:
        raise FailCheck("\n".join(errors[:20]))
    return f"layer boundaries OK ({len(c_files)} files)"


# ---------------------------------------------------------------------------
# 5. Docs
# ---------------------------------------------------------------------------
def check_docs(files: list[str], verbose: bool) -> str:
    del files, verbose
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
# 6. Line / File Budgets (始终全仓统计)
# ---------------------------------------------------------------------------
def count_lines(path: Path) -> int:
    return len(path.read_text(encoding="utf-8", errors="replace").splitlines())


def check_budgets(files: list[str], verbose: bool) -> str:
    del files, verbose
    bsp_files = sorted(
        list((REPO_ROOT / "BSP" / "Inc").glob("*.h"))
        + list((REPO_ROOT / "BSP" / "Src").glob("*.[ch]"))
    )
    bsp_lines = sum(count_lines(p) for p in bsp_files)

    user_files = sorted(
        list((REPO_ROOT / "User").glob("*.h"))
        + list((REPO_ROOT / "User").glob("*.c"))
        + [REPO_ROOT / "CMakeLists.txt"]
    )
    user_lines = sum(count_lines(p) for p in user_files if p.is_file())

    test_files = sorted((REPO_ROOT / "tests").glob("test_*.c"))
    test_lines = sum(count_lines(p) for p in test_files)

    errors: list[str] = []
    if len(bsp_files) > BSP_MAX_FILES:
        errors.append(f"BSP 文件数超标: {len(bsp_files)} > {BSP_MAX_FILES}")
    if bsp_lines > BSP_MAX_LINES:
        errors.append(f"BSP 总行数超标: {bsp_lines} > {BSP_MAX_LINES}")
    if user_lines > USER_MAX_LINES:
        errors.append(f"User/ + CMakeLists.txt 总行数超标: {user_lines} > {USER_MAX_LINES}")
    if test_lines > TESTS_MAX_LINES:
        errors.append(f"tests/test_*.c 总行数超标: {test_lines} > {TESTS_MAX_LINES}")

    if errors:
        raise FailCheck("; ".join(errors))
    return (
        f"BSP={len(bsp_files)}f/{bsp_lines}L (<={BSP_MAX_FILES}f/{BSP_MAX_LINES}L), "
        f"User+CMake={user_lines}L (<={USER_MAX_LINES}L), "
        f"Tests={len(test_files)}f/{test_lines}L (<={TESTS_MAX_LINES}L)"
    )


# ---------------------------------------------------------------------------
# 7. Host CTest
# ---------------------------------------------------------------------------
def check_host_test(files: list[str], verbose: bool) -> str:
    del files
    if not shutil.which("cmake") or not shutil.which("ctest"):
        raise SkipCheck("缺少 cmake 或 ctest")

    cfg = run_cmd(["cmake", "--preset", "Test"], verbose=verbose)
    if cfg.returncode != 0:
        raise FailCheck((cfg.stderr or cfg.stdout).strip()[-1500:])

    bld_res = run_cmd(["cmake", "--build", "--preset", "Test"], verbose=verbose)
    if bld_res.returncode != 0:
        raise FailCheck((bld_res.stderr or bld_res.stdout).strip()[-1500:])

    tst = run_cmd(["ctest", "--preset", "Test", "--output-on-failure"], verbose=verbose)
    if tst.returncode != 0:
        raise FailCheck((tst.stdout + "\n" + tst.stderr).strip()[-2000:])
    summary = [l for l in tst.stdout.splitlines() if "tests passed" in l]
    return summary[-1] if summary else "ctest passed"


# ---------------------------------------------------------------------------
# 8. clang-tidy
# ---------------------------------------------------------------------------
def check_clang_tidy(files: list[str], verbose: bool) -> str:
    del files
    tidy = shutil.which("clang-tidy")
    if not tidy:
        raise SkipCheck("未安装 clang-tidy（可由用户在终端执行 sudo apt install -y clang-tidy）")
    compdb = REPO_ROOT / "build" / "Test" / "compile_commands.json"
    if not compdb.is_file():
        raise SkipCheck("缺少 build/Test/compile_commands.json（请先运行 host-test）")

    entries = json.loads(compdb.read_text(encoding="utf-8"))
    target_sources = sorted(
        {
            str(Path(e["file"]).resolve())
            for e in entries
            if Path(e["file"]).resolve().is_relative_to(REPO_ROOT)
            and is_owned_code_path(
                Path(e["file"]).resolve().relative_to(REPO_ROOT).as_posix()
            )
        }
    )
    if not target_sources:
        return "no owned sources in build/Test/compile_commands.json"

    res = run_cmd([tidy, "-p", str(compdb.parent), *target_sources], verbose=verbose)
    combined = res.stdout + "\n" + res.stderr
    diag_lines = [
        l for l in combined.splitlines() if re.search(r":\d+:\d+:\s+(warning|error):", l)
    ]
    if res.returncode != 0 or diag_lines:
        raise FailCheck((combined.strip())[-2000:])
    return f"clang-tidy OK ({len(target_sources)} translation units, 0 warnings)"


# ---------------------------------------------------------------------------
# 9. Board Builds + ELF Static/Size Gate
# ---------------------------------------------------------------------------
def check_board_build(files: list[str], verbose: bool) -> str:
    del files
    if not shutil.which("arm-none-eabi-gcc"):
        raise SkipCheck("未安装 arm-none-eabi-gcc")
    nm = shutil.which("arm-none-eabi-nm")
    size_bin = shutil.which("arm-none-eabi-size")
    if not nm or not size_bin:
        raise FailCheck("缺少 arm-none-eabi-nm 或 arm-none-eabi-size")

    forbidden_syms = {
        "malloc",
        "calloc",
        "realloc",
        "free",
        "ucHeap",
        "pvPortMalloc",
        "vPortFree",
    }
    summaries: list[str] = []

    for preset in ("Debug", "Release"):
        c = run_cmd(["cmake", "--preset", preset], verbose=verbose)
        if c.returncode != 0:
            raise FailCheck(f"[{preset}] configure failed:\n{(c.stderr or c.stdout)[-1500:]}")
        bld = run_cmd(["cmake", "--build", "--preset", preset], verbose=verbose)
        if bld.returncode != 0:
            raise FailCheck(f"[{preset}] build failed:\n{(bld.stderr or bld.stdout)[-1500:]}")

        elf = REPO_ROOT / "build" / preset / "f407vet6.elf"
        if not elf.is_file():
            raise FailCheck(f"[{preset}] 未找到输出文件 {elf.relative_to(REPO_ROOT)}")

        nm_u = run_cmd([nm, "-u", str(elf)])
        if nm_u.returncode != 0:
            raise FailCheck(f"[{preset}] nm -u failed: {nm_u.stderr}")
        for line in nm_u.stdout.splitlines():
            sym = line.strip().split()[-1] if line.strip() else ""
            if sym in forbidden_syms:
                raise FailCheck(f"[{preset}] {elf.name} 引用禁止动态内存符号: {sym}")

        nm_d = run_cmd([nm, "--defined-only", str(elf)])
        if nm_d.returncode != 0:
            raise FailCheck(f"[{preset}] nm --defined-only failed: {nm_d.stderr}")
        has_stack_hook = False
        for line in nm_d.stdout.splitlines():
            parts = line.strip().split()
            if len(parts) >= 3:
                sym_type, sym_name = parts[1], parts[2]
                if sym_name in forbidden_syms:
                    raise FailCheck(f"[{preset}] {elf.name} 定义了禁止动态内存符号: {sym_name}")
                if sym_name == "vApplicationStackOverflowHook" and sym_type == "T":
                    has_stack_hook = True
        if not has_stack_hook:
            raise FailCheck(f"[{preset}] {elf.name} 缺少强文本符号 vApplicationStackOverflowHook")

        sz = run_cmd([size_bin, str(elf)])
        if sz.returncode != 0 or len(sz.stdout.strip().splitlines()) < 2:
            raise FailCheck(f"[{preset}] arm-none-eabi-size 执行失败")
        cols = sz.stdout.strip().splitlines()[1].split()
        text_b, data_b, bss_b = int(cols[0]), int(cols[1]), int(cols[2])
        flash_b = text_b + data_b
        ram_b = data_b + bss_b
        summaries.append(f"{preset}(Flash={flash_b}B, RAM={ram_b}B)")

        if preset == "Release":
            if flash_b > F407_RELEASE_MAX_FLASH_BYTES:
                raise FailCheck(
                    f"[Release] Flash 超红线: {flash_b} B > {F407_RELEASE_MAX_FLASH_BYTES} B"
                )
            if ram_b > F407_RELEASE_MAX_RAM_BYTES:
                raise FailCheck(
                    f"[Release] RAM 超红线: {ram_b} B > {F407_RELEASE_MAX_RAM_BYTES} B"
                )

    return " | ".join(summaries)


STAGES: dict[str, Callable[[list[str], bool], str]] = {
    "hygiene": check_hygiene,
    "format": check_format,
    "static-alloc": check_static_alloc,
    "layers": check_layers,
    "docs": check_docs,
    "budgets": check_budgets,
    "host-test": check_host_test,
    "clang-tidy": check_clang_tidy,
    "board-build": check_board_build,
}


def main() -> int:
    parser = argparse.ArgumentParser(description="layered-embedded-c (v2) 本地多维度检查")
    parser.add_argument("--profile", choices=PROFILES.keys(), default="quick")
    parser.add_argument("--scope", choices=("staged", "branch", "all"), default="branch")
    parser.add_argument("--only", help="仅运行指定阶段（逗号分隔）")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    stage_names = (
        [s.strip() for s in args.only.split(",") if s.strip()]
        if args.only
        else PROFILES[args.profile]
    )
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

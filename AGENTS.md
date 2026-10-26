# Repository Guidelines

## Project Structure & Module Organization

Dependencies flow from each board's `App/` and `RTOS/` through shared device/algorithm layers to `Platform/` and HAL; never reverse them.

- `Platform/`: HAL-free contracts in `Inc/`, shared logic in `Src/common/`, and MCU adapters in `Src/stm32f4/` and `Src/stm32f1/`.
- `DeviceDrivers/`: abstract interfaces in `Base/`; concrete drivers in `Inc/` and `Src/`.
- `Controller/`, `Policy/`, `Filter/`, `System/`: shared algorithms and services.
- `Boards/<mcu>-<name>/`: self-contained firmware projects. Each project owns its `.ioc`, `Core/`, `Drivers/`, `Middlewares/`, `cmake/`, `App/`, `RTOS/`, startup, linker script, hardware mapping, and CMake presets.
- `Middlewares/Third_Party/SEGGER_RTT/`: optional RTT logger shared by board builds.
- `cmake/platform-test/`: native C contract tests. `cmake/platform-check/`: F1/F4 compile-only checks. Documentation lives in `doc/`.

## Build, Test, and Development Commands

Install CMake 3.22+, Ninja, and `arm-none-eabi-gcc` on `PATH`.

```powershell
cd Boards/f407vet6-omni-car
cmake --preset Debug
cmake --build --preset Debug
```

Use the same commands in `Boards/f103c8t6-platform-test` for the F103 target. Substitute
`Release` for release builds. Flash ELF files with ST-Link/OpenOCD; see the
board READMEs for examples.

```powershell
cmake -S cmake/platform-test -B build/platform-test -G Ninja -DCMAKE_C_COMPILER=gcc
cmake --build build/platform-test
ctest --test-dir build/platform-test --output-on-failure
```

Any CMake-supported C11 compiler works. Build `cmake/platform-check` with its
`f4-debug` and `f1-debug` presets when changing shared code, board App, or board
RTOS. Generate API docs from `Boards/f407vet6-omni-car` with
`cmake --build --preset Debug --target docs`.

## Coding Style & Naming Conventions

Use C11, UTF-8, and two-space indentation (`.editorconfig`). Public functions follow `Layer_Module_Qualifier_VerbObject`, such as `Platform_UART_ReceiveIT`. Public types use `snake_case` plus `_t`; struct/enum/union tags end in `_s`/`_e`/`_u`, and handles use `<role>_h`.

Embed the matching `*_base_t` as the first member of concrete types. Use static `*_ops_t` tables, avoid dynamic memory, and keep HAL types inside `Platform/Src/`. Maintained comments and documentation use Simplified Chinese; identifiers remain English. Do not hand-edit `Drivers/`, `Middlewares/`, or generated CubeMX code outside `USER CODE` sections.

## Testing Guidelines

Native tests are plain C programs using `assert` and CTest; GCC/Clang targets compile with `-Wall -Wextra -Werror`. Name files `<module>_contract_test.c` and register new executables with both `add_executable()` and `add_test()` in `cmake/platform-test/CMakeLists.txt`.

No coverage threshold applies. Add focused tests for changed HAL-free behavior, including invalid arguments and error paths. For shared code, run native tests and both F1/F4 debug checks.

## Commit & Pull Request Guidelines

Follow the predominant Conventional Commits style: `feat(imu): ...`, `fix(platform): ...`, `docs: ...`, `refactor: ...`, or `chore: ...`. Use a concise imperative description.

Pull requests should explain behavior and scope, list verification commands, and link relevant issues. Include logs or photographs for board-facing changes. Call out CubeMX, linker, startup, or generated-file changes; exclude unrelated generated churn.

## Git Review Gate

- Before any mutating Git operation (`commit`, `commit --amend`, `merge`, `rebase`, `cherry-pick`, `revert`, `push`, or `tag`), run `$pre-git-review`.
- Read-only commands such as `git status`, `git diff`, `git log`, and `git show` do not invoke the review.
- Review the complete intended change set against this file and the affected repository documentation. New code must have zero violations, and existing code touched by the change must be brought into compliance. Full-repository historical debt is a separate audit and must not be silently bundled into an unrelated commit.
- Changes that add, remove, relax, or redefine constraints, conventions, architecture, interface contracts, units, ownership, safety rules, or hardware rules in `AGENTS.md`, project skills, or maintained repository documentation require explicit human approval in the current conversation. Approval must identify the affected file or semantic change; an implementation request does not imply approval to rewrite governing rules. Missing approval fails the review.
- Ordinary code comments and non-normative small documentation edits do not require human approval, but they must still pass the full review. A comment or documentation edit that changes normative semantics is treated as a protected documentation change.
- Every failed review must identify each failure with file and line, violated rule or document clause, required fix, and the exact documentation that must be reread. The report must prominently stop the Git operation and require the agent to consult the documentation before editing again.
- Behavioral changes require focused tests or regression coverage. Without matching test evidence, the review fails. Never weaken, delete, skip, or falsify tests to obtain a pass.
- If review fails, fix the violations and rerun every affected check. Stop and ask when behavior is ambiguous or two consecutive attempts produce no net improvement.
- For board-facing changes, automated review, tests, and builds gate local commits. Push and pull requests additionally require hardware verification evidence unless the user explicitly accepts the remaining risk.
- Do not bypass the review with `--no-verify`, skipped tests, relaxed warnings, or equivalent workarounds.
- The review does not grant permission to mutate Git state. Execute a commit, push, or other Git operation only when the user requested that operation.

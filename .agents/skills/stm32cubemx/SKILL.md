---
name: stm32cubemx
description: Generate STM32CubeMX code from a board .ioc or flash a built ELF with ST-Link/OpenOCD. Use for CubeMX synchronization and firmware flashing, not ordinary application-code changes.
---

# STM32CubeMX

使用 `scripts/stm32-cubemx.ps1` 执行两项操作：

```powershell
./scripts/stm32-cubemx.ps1 generate -Ioc boards/f103c8t6/f103c8t6.ioc
./scripts/stm32-cubemx.ps1 flash -Ioc boards/f103c8t6/f103c8t6.ioc
```

## 修改与生成

- 只修改 `.ioc` 中的 CubeMX 配置键，不直接修改生成文件或第三方代码。
- `generate` 会在临时副本中运行 CubeMX，并且只回写 `.ioc`、`.mxproject` 和 `Core/` 生成内容。
- `Core/Inc/stm32f*xx_hal_conf.h` 是受保护文件，不回写，因为项目会在其中启用 Platform 需要的额外 HAL 模块。
- 不自动回写 `Drivers/`、`Middlewares/`、`cmake/`、链接脚本、启动文件或根 CMake 文件。
- CubeMX 可能覆盖 `cmake/stm32cubemx/CMakeLists.txt`。新增 `Core/Src/*.c` 后，必须手工把它加入现有定制源文件列表，并保留原有 HAL 模块。
- 默认遇到任何生成对话框都会失败并保存截图。只有确认截图确实是 FreeRTOS 与 SysTick 时基警告后，才能使用 `-AcceptRtosSystickWarning` 重试。
- 生成失败时保留运行目录和日志，供定位问题。

## 烧录

- 仅在用户明确要求烧录时执行 `flash`。
- 脚本从 `.ioc` 识别 STM32F1/F4，通过 OpenOCD 和 ST-Link 执行 `program verify reset`。
- 未指定 `-Elf` 时，从 `build/Debug/<project>.elf` 开始查找。
- ELF 早于目标相关源码时默认拒绝烧录；只有确认风险后才使用 `-AllowStaleElf`。
- 使用 `-DryRun` 可以只检查工具、目标、ELF 和命令，不操作板卡。

## 工具路径

脚本优先使用显式参数和以下环境变量：

```text
STM32CUBEMX_EXE
OPENOCD_EXE
```

之后才搜索 `PATH` 和常见安装目录。不要把这些本机绝对路径写进项目配置。

本 skill 不负责编译、GDB 调试、RTT 或提交代码。生成后若要验证，继续使用仓库现有的 CMake 构建命令。

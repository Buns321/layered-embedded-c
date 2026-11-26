# layered-embedded-c

面向 **STM32F407VET6** 四轮全向小车的精简 **ISO C11** 分层嵌入式工程（根目录单工程布局，旧版 v1 多板与 `Platform/` 架构归档于 Git 标签 `v1-layered-archive`）。

## 核心特性

- **极薄 `BSP/` 层**：直连 CubeMX 句柄的 `bsp_dwt` / `bsp_gpio` / `bsp_pwm` / `bsp_encoder` / `bsp_uart` / `bsp_i2c` / `bsp_flash`，消除枚举包装壳。
- **三大静态核心服务**（`Framework/Core/`）：
  - `sys_daemon`：静态离线看门狗，支持 `INIT_GRACE` 宽限与临界区外边沿回调。
  - `sys_topic`：严格 ISO C11 静态类型安全 Pub-Sub 通道宏。
  - `rtos_task`：集中式静态周期任务宏 `RTOS_TASK_PERIODIC`、DWT 耗时/栈水位监控与强实现 `vApplicationStackOverflowHook` 紧急断电钩子。
- **5 处领域多态边界**（首成员嵌入 `base` + 静态只读 `ops`）：
  `controller_base_t`、`motor_base_t`、`imu_base_t`、`telemetry_base_t`、`chassis_base_t`。
- **`Car_Omni` 一键门面**：静态装配四轮 TB6612 + 编码器 + PID + 全向运动学，按 `huart == NULL` 自动裁剪可选外设（WHT101 / JoyCon / JustFloat / FireWater），支持悬空分步调试与失联急停锁存。
- **零动态内存**：生产代码与 ELF 禁止 `malloc`/`calloc`/`realloc`/`free`/`ucHeap`/`pvPortMalloc`/`vPortFree`，开启 `configCHECK_FOR_STACK_OVERFLOW=2`。

## 5 分钟快速开始

依赖：CMake ≥ 3.22、Ninja、`arm-none-eabi-gcc`（交叉编译）及宿主机 GCC/Clang（单测与检查）。

```bash
# 1. 宿主机 CTest 单元测试（14 项）
cmake --preset Test && cmake --build --preset Test && ctest --preset Test --output-on-failure

# 2. STM32F407VET6 固件构建（Debug / Release）
cmake --preset Debug   && cmake --build --preset Debug
cmake --preset Release && cmake --build --preset Release

# 3. 本地全维度门禁（卫生 / 格式 / 静态分配 / 分层 / 预算 / CTest / clang-tidy / ELF）
python3 tools/check.py --profile full --scope all
```

烧录 `build/Debug/f407vet6.elf` 并查看 SEGGER RTT 日志（ST-Link + OpenOCD）：

```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program build/Debug/f407vet6.elf verify reset exit"
```

Windows 下通过 PowerShell 接收 RTT（详见 [RTT 日志接收](tools/rtt/README.md)）：

```powershell
.\tools\rtt\Receive-Rtt.ps1 -Board f407vet6
```

## 目录结构

| 目录 / 文件 | 职责 |
| --- | --- |
| `BSP/` | Layer 1：极薄 STM32 HAL 封装（≤ 18 文件，≤ 2,000 行） |
| `Framework/Core/` | 数学/角度/环形缓冲/日志/Flash 存储、`sys_daemon`、`sys_topic`、`rtos_task` |
| `Framework/Control/` | 100% HAL-Free 控制律（PID/Linear/Compose）、滤波器与 `kinematics_omni` |
| `Framework/Drivers/` | TB6612、WHT101、MPU6050、JoyCon、JustFloat/FireWater、LED |
| `Framework/Chassis/` | `chassis_base`、`chassis_omni` 与 `car_omni` 一键整车门面 |
| `Framework/ThirdParty/` | SEGGER RTT 源码 |
| `User/` | Layer 3：用户板级映射 `board_cfg`、纯业务入口 `app_car` 与周期任务 `rtos_tasks` |
| `Core/` / `Drivers/` / `Middlewares/` | CubeMX 生成工程、STM32F4 HAL 与 FreeRTOS（禁止手改非 USER CODE 区） |
| `cmake/` | `car_framework.cmake` 统一编译子图、`verify_static_elf.cmake` 与 CubeMX 清单 |
| `tests/` | 宿主机桩（`tests/stubs/`）与 14 项 CTest 单元测试 |
| `tools/` | `check.py` 多维度门禁、`doc_map.json` 文档映射与 `rtt/` 接收脚本 |
| `doc/` | 架构、硬件映射、新手上手指南、编码约定与物理单位规范 |

## 文档导航

- [文档索引](doc/README.md)
- [V2 三层架构说明](doc/architecture.md)
- [新硬件接线与上手指南](doc/quickstart-new-board.md)
- [STM32F407VET6 硬件与接口规范](doc/hardware-map.md)
- [编码约定](doc/conventions.md)
- [物理单位与量纲](doc/units.md)
- [V2 架构精简重构详案](doc/v2-refactor-plan.md)

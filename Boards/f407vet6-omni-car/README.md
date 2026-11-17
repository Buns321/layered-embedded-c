# STM32F407VET6 板级工程

本目录提供 STM32F407VET6 的 CubeMX 工程、F4 HAL、Cortex-M4F FreeRTOS 端口、
启动文件、链接脚本、板内 App / RTOS 和 CMake 构建目标。

当前引脚、时钟和外设分配见 [硬件与接口规范](hardware-map.md)。
快速接线表见 [PinMap](pinmap.md)。

## 硬件基线

| 项目 | 配置 |
| --- | --- |
| MCU | STM32F407VET6 |
| 内核 | Cortex-M4F |
| FLASH / SRAM | 512 KB / 192 KB |
| SYSCLK | 168 MHz |
| HAL 时基 | TIM7 |
| 调试 | SWD，PA13 / PA14 |
| RTOS | FreeRTOS CM4F，CMSIS-RTOS V2 |
| 日志 | SEGGER RTT |

## 工程结构

| 路径 | 内容 |
| --- | --- |
| `f407vet6.ioc` | CubeMX 配置 |
| `Core/` | CubeMX 生成代码和用户初始化 |
| `Core/Board/` | 手写板级对象、硬件映射和 App 依赖注入 |
| `Drivers/` | STM32F4 HAL、CMSIS 和设备头 |
| `Middlewares/` | 板内 FreeRTOS 与 CMSIS-RTOS V2 |
| `App/` | 本板业务入口 |
| `RTOS/` | 本板任务入口 |
| `cmake/` | ARM GCC 工具链与 CubeMX 构建描述 |
| `STM32F407xx_FLASH.ld` | 链接脚本 |
| `startup_stm32f407xx.s` | 启动文件 |

共享 System、Platform F4、DeviceDrivers、Controller、Policy、Filter 和
`Middlewares/Third_Party/SEGGER_RTT/` 从仓库根目录接入。本板不复制这些共享层。

F407 小车已完成 App/Core/Board 分层迁移：

- `Core_Board_Init()` 定义在 `Core/Board/Src/core_board.c`。
- App 通过 `app_car_dependencies_t` 接收底盘基础接口、电机数组、IMU、输入和遥测服务。
- `layered_app`、`layered_rtos` 和 `layered_core_board` 在板级 CMake 中建立。
- CubeMX 生成三个静态任务及其 TCB/栈；`RTOS/` 中的薄入口把任务绑定到 App runtime。
- `app_car.c` 只负责初始化和任务入口协调；命令、启动、安全、控制和遥测
  分别位于独立 App 源文件。
- `app_car_motion` 封装底盘 twist、电机闭环、使能/停止/急停和轮速读取。
- App 只依赖 `chassis_base_t` 和运行时电机数量，不引用 Omni 具体类型或轮数常量。
- App 以车体系 `vx_body_mps`、`vy_body_mps`、`wz_radps` 提交底盘命令。
- 编码器每米计数、轮距、PWM 量程等板级物理参数位于 Core/Board 私有配置。
- `layered_app` include 白名单只保留 App 公共头、base 接口和 System 公共头。

## CubeMX 基线

当前生成配置包括：

- HSI 16 MHz 经 PLL 运行在 168 MHz。
- SWD 调试，保留 PA13 / PA14。
- TIM7 作为 HAL 时基，FreeRTOS 使用 SysTick。
- USART1：WHT101，9600。
- USART2：JustFloat 到蓝牙 RX，115200，TX DMA。
- USART3：JoyCon 从蓝牙 TX 接收，115200，RX 中断。
- TIM1 四通道 PWM，TIM2–TIM5 编码器接口。
- CRC、RNG 和 FreeRTOS 静态任务配置。ControlTask、UARTTask 和 MonitorTask 的
  静态栈及 TCB 由 CubeMX 生成。
- 每个外设生成独立的 `MX_*_Init()`。

当前 STM32CubeMX 6.18.1 对 CRC/RNG 这类无引脚 IP 可能回退为保留已有源码。
重新生成后必须检查 `stm32f4xx_hal_conf.h` 仍启用 CRC/RNG，且板级
`cmake/stm32cubemx/CMakeLists.txt` 仍包含对应 HAL 源文件和 `Core/Src/crc.c`、
`Core/Src/rng.c`。

四个轮位 A、B、C、D 从车头开始逆时针排列。TB6612 的 STBY 按 A/B 和 C/D
两组共享。固件启动时保持四轮禁用，等待 WHT101 完成静止确认和传感器归零后，
再由有效 JoyCon 命令使能闭环。

控制任务以 100 Hz 运行四轮速度 PID，UARTTask 每 2 ms 处理 WHT101 和 JoyCon，
MonitorTask 以 100 Hz 运行检查并按 50 Hz 节流发送 JustFloat，通过 SEGGER RTT
输出低频状态日志。三个任务均使用静态 TCB 和静态栈，RTOS 在调用 App 前统一把
tick 转换为 `now_ms`。

公共 `RTOS/Inc/` 接口只使用 CMSIS-RTOS2 与标准 C 类型。CubeMX 在
`Core/Src/freertos.c` 中生成任务对象、静态 TCB、静态栈和 `osThreadNew()` 调用；
`RTOS/Src/rtos_control.c`、`rtos_uart.c`、`rtos_monitor.c` 只提供薄任务入口，
不向 App 暴露 FreeRTOS 类型。

FreeRTOS 动态分配已关闭，`heap_4.c` 不参与链接；Idle 和 Timer 任务使用
CMSIS-RTOS2 适配层提供的静态 TCB/栈。STM32CubeMX 6.18.1 的 CMSIS-RTOS2
模板即使选择 `Static`，仍可能生成 `configSUPPORT_DYNAMIC_ALLOCATION=1`；受保护的
`FreeRTOSConfig.h` 在 USER CODE 区域将其覆盖为 0。重新生成 CubeMX 后必须确认该
覆盖仍存在、`configSUPPORT_DYNAMIC_ALLOCATION` 仍为 0，且 `heap_4.c` 未被重新加入。
板级构建在链接后检查三个 `RTOS_*_StartTask` 是强符号，并拒绝动态 FreeRTOS 堆符号。

HSE 引脚已出现在 CubeMX 引脚配置中，但当前 `SystemClock_Config()` 使用 HSI，
因此不能把外部晶振当作当前时钟源。

## 验证状态

本轮已完成：

- Debug 构建和链接。
- Release 构建和链接。
- App 业务模块拆分：命令、启动、安全、控制和遥测。
- F407/F103 Platform 全层 Debug 与 Release 编译检查。
- `cmake/platform-test` 全部主机契约测试。
- App/RTOS 禁止依赖扫描。
- `layered_app_portability_fixture` 主机执行与 F1/F4 编译检查。

本轮未执行真实板卡烧录和运行验证。WHT101、JoyCon、JustFloat、电机闭环和
启动归零流程的实板行为仍需后续联调确认。

## 构建

依赖 CMake 3.22 或更高版本、Ninja 和 `arm-none-eabi-gcc`，并确保
`arm-none-eabi-` 位于 `PATH`。

```powershell
cd Boards/f407vet6-omni-car
cmake --preset Debug
cmake --build --preset Debug
```

Release 构建：

```powershell
cmake --preset Release
cmake --build --preset Release
```

全分层编译检查：

```powershell
cd ../../cmake/platform-check
cmake --preset f4-debug
cmake --build --preset f4-debug
cmake --preset f4-release
cmake --build --preset f4-release
```

主要产物：

```text
Boards/f407vet6-omni-car/build/Debug/f407vet6.elf
Boards/f407vet6-omni-car/build/Debug/f407vet6.map
Boards/f407vet6-omni-car/build/Release/f407vet6.elf
Boards/f407vet6-omni-car/build/Release/f407vet6.map
```

## 调试与烧录

ST-Link 与 OpenOCD 示例：

```powershell
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg `
  -c "program build/Debug/f407vet6.elf verify reset exit"
```

RTT 通过 SWD 通道工作。烧录带 RTT 的固件后，在仓库根目录运行：

```powershell
$env:OPENOCD_EXE = "D:\path\to\openocd.exe"
.\tools\rtt\Receive-Rtt.ps1 -Board f407vet6
```

## 相关文档

- [根工程说明](../../README.md)
- [架构说明](../../doc/architecture.md)
- [Platform 契约](../../doc/platform.md)
- [硬件与接口规范](hardware-map.md)
- [快速接线 PinMap](pinmap.md)
- [编码约定](../../doc/conventions.md)

# STM32F103C8T6 板级工程

本目录提供 STM32F103C8T6 集成板的 CubeMX 工程、F1 HAL、Cortex-M3 FreeRTOS
端口、启动文件、链接脚本和 CMake 构建目标。

完整硬件连接、片上资源分配、电气边界和验证顺序见
[硬件与接口规范](hardware-map.md)。

## 硬件基线

| 项目 | 配置 |
| --- | --- |
| MCU | STM32F103C8T6 |
| 内核 | Cortex-M3 |
| FLASH / SRAM | 64 KB / 20 KB |
| 外部时钟 | 8 MHz HSE |
| SYSCLK | 72 MHz |
| 调试 | SWD，PA13 / PA14 |
| HAL 时基 | SysTick |
| RTOS | FreeRTOS CM3，CMSIS-RTOS V2 |
| 日志 | SEGGER RTT |

## 工程结构

| 路径 | 内容 |
| --- | --- |
| `f103c8t6.ioc` | CubeMX 配置 |
| `Core/` | CubeMX 生成代码和用户初始化 |
| `Drivers/` | STM32F1 HAL、CMSIS 和设备头 |
| `Middlewares/` | FreeRTOS 与 CMSIS-RTOS V2 |
| `cmake/` | ARM GCC 工具链与 CubeMX 构建描述 |
| `STM32F103xx_FLASH.ld` | 链接脚本 |
| `startup_stm32f103xb.s` | 启动文件 |

共享工程层的源文件从仓库根目录接入：

- System
- Platform F1
- DeviceDrivers base 与公共驱动
- Controller
- Policy
- Filter
- RTOS
- App 占位入口

## CubeMX 基线

当前 `.ioc` 生成以下基础配置：

- 8 MHz HSE 和 72 MHz PLL。
- SWD 调试，保留 PA13 / PA14。
- SysTick 作为 HAL 与 FreeRTOS 时基。
- CRC 独立外设初始化。
- FreeRTOS CMSIS-RTOS V2 和默认任务。
- 独立 `gpio.c` / `gpio.h`、`crc.c` / `crc.h`、`freertos.c` /
  `freertos.h`。

板级接口的目标资源和引脚分配以
[硬件与接口规范](hardware-map.md)为准。新增外设时继续由 CubeMX 生成
独立的 `MX_*_Init()`、引脚、时钟、DMA 和 NVIC 配置。

## Platform 外设状态

正式板级构建当前启用：

- GPIO
- EXTI
- CRC
- IWDG
- WWDG

F1 的 PWM、Encoder、USART1–3、I2C1–2、SPI1–2、ADC1–2、CAN1、RTC
和 Flash 实现已位于共享 Platform 层。I2C、SPI、ADC、CAN、RTC、UART
和 TIM 的 HAL 头文件与源文件也已补入本板级驱动目录。

这些实现通过 `cmake/platform-check/` 的 F1 全实例目标编译验证。正式板上
启用某个实例时，仍需由 CubeMX 在 `.ioc` 中生成引脚、时钟、DMA、NVIC、
`MX_*_Init()` 和 HAL handle，并在板级 CMake 中选择对应实例与
`PLATFORM_BUILD_*` 开关。未配置实例不会出现在正式目标的公共枚举中。

## 构建

依赖：

- CMake 3.22 或更高版本
- Ninja
- `arm-none-eabi-gcc`
- `arm-none-eabi-` 位于 `PATH`

在板级目录执行：

```powershell
cd boards/f103c8t6
cmake --preset Debug
cmake --build --preset Debug
```

Release 构建：

```powershell
cmake --preset Release
cmake --build --preset Release
```

F1 全实例 Platform 编译检查：

```powershell
cd ../../cmake/platform-check
cmake --preset f1-debug
cmake --build --preset f1-debug
cmake --preset f1-release
cmake --build --preset f1-release
```

关闭 RTT 日志的 Debug 构建：

```powershell
cmake --preset Debug -DSYS_LOG_USE_RTT=OFF -DBOARD_SELFTEST=OFF
cmake --build --preset Debug
```

主要产物：

```text
boards/f103c8t6/build/Debug/f103c8t6.elf
boards/f103c8t6/build/Debug/f103c8t6.map
boards/f103c8t6/build/Release/f103c8t6.elf
boards/f103c8t6/build/Release/f103c8t6.map
```

## 调试与烧录

ST-Link 与 OpenOCD 示例：

```powershell
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg `
  -c "program build/Debug/f103c8t6.elf verify reset exit"
```

RTT 通过 SWD 通道工作。烧录带 RTT 的固件后，在仓库根目录运行：

```powershell
$env:OPENOCD_EXE = "D:\path\to\openocd.exe"
.\tools\rtt\Receive-Rtt.ps1 -Board f103c8t6
```

脚本会自动查找 `SEGGER RTT` 控制块并输出日志，不执行烧录。
详细参数见 [`tools/rtt/README.md`](../../tools/rtt/README.md)。

### 板载自检

F103 板级目标默认启用非破坏性自检，启动后会通过 RTT 输出：

```text
[5][I][TEST] BEGIN board=f103c8t6
[5][I][TEST] clock PASS
[5][I][TEST] tick elapsed=5 ms
[5][I][TEST] tick PASS
[5][I][TEST] log PASS
[5][I][TEST] crc PASS
[5][I][TEST] SUMMARY pass=4 fail=0
```

当前覆盖 72 MHz 内核时钟、FreeRTOS tick、RTT 日志健康和硬件 CRC。
测试不操作继电器、电机、舵机、蜂鸣器和灯。可通过 `-DBOARD_SELFTEST=OFF` 关闭。
该选项默认跟随 `SYS_LOG_USE_RTT`；关闭 RTT 时默认关闭自检，显式强制二者冲突会
在配置阶段失败。

## 当前验证状态

- Debug、Release 和关闭 RTT 的 Debug 配置均已完成编译与链接。
- F103 目标已接入共享 System、Platform F1、DeviceDrivers base、Controller、
  Policy、Filter、RTOS 和 App 占位入口。
- F1 支持的全部 Platform 外设已通过 Debug、Release 独立编译检查。
- 板上非破坏性自检已通过：72 MHz 时钟、FreeRTOS tick、RTT 日志健康和硬件 CRC，
  结果为 `SUMMARY pass=4 fail=0`。
- 板上时钟、RTT、FreeRTOS 调度、CRC、IWDG、WWDG 以及硬件接口验证按
  [硬件与接口规范](hardware-map.md)第 11 节执行。

## 相关文档

- [根工程说明](../../README.md)
- [架构说明](../../doc/architecture.md)
- [Platform 契约](../../doc/platform.md)
- [硬件与接口规范](hardware-map.md)
- [编码约定](../../doc/conventions.md)

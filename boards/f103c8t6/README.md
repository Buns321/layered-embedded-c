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

本板独立持有 `App/`、`RTOS/`、`Core/`、`Drivers/`、`Middlewares/` 和
构建配置，不复制共享层。

## CubeMX 基线

当前 `.ioc` 生成以下基础配置：

- 8 MHz HSE 和 72 MHz PLL。
- SWD 调试，保留 PA13 / PA14。
- SysTick 作为 HAL 与 FreeRTOS 时基。
- CRC 独立外设初始化。
- USART2 / USART3、I2C1、ADC1、TIM1–4、DMA1 和对应 NVIC。
- FreeRTOS CMSIS-RTOS V2 和默认任务。
- 每个外设生成独立的 `*.c` / `*.h`。

板级接口的目标资源和引脚分配以
[硬件与接口规范](hardware-map.md)为准。新增外设时继续由 CubeMX 生成
独立的 `MX_*_Init()`、引脚、时钟、DMA 和 NVIC 配置。

## Platform 外设状态

正式板级构建始终启用：

- GPIO
- EXTI
- CRC
- IWDG
- WWDG

`F103_TEST=ON` 时额外启用并实测：

- ADC1、I2C1、UART2 / UART3
- PWM TIM2 / TIM3 / TIM4
- Encoder TIM1

F1 的 USART1–3、I2C1–2、SPI1–2、ADC1–2、CAN1、RTC 和 Flash 实现位于
共享 Platform 层。正式产品构建启用其他实例时，仍需同时选择对应
`PLATFORM_BUILD_*` 开关和实例宏。

这些实现通过 `cmake/platform-check/` 的 F1 全实例目标编译验证。
未配置实例不会出现在正式目标的公共枚举中。

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
cmake --preset Debug -DSYS_LOG_USE_RTT=OFF -DF103_TEST=OFF
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

### F103 测试固件

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
基础自检不操作继电器、电机、舵机、蜂鸣器和灯。可通过
`-DF103_TEST=OFF` 关闭。
该选项默认跟随 `SYS_LOG_USE_RTT`；关闭 RTT 时默认关闭自检，显式强制二者冲突会
在配置阶段失败。

板载硬件实测还会检查 GPIO 输出回读、I2C1 的 `0x38` / `0x3D` 设备、ADC1 双通道
DMA、TIM1 编码器、TIM2 / TIM3 / TIM4 定时器、USART3 探测，并通过 USART2
验证 CH343P 收发、IWDG 和 WWDG 复位。定时器和执行器测试使用零占空比、短时
中位脉冲或零数据 DMA，结束后恢复安全状态。运行：

```powershell
.\tools\test\Invoke-F103HardwareTest.ps1
```

脚本默认自动选择 CH343P 串口。IWDG 和 WWDG 测试会让 MCU 主动复位；只测试常规项时
使用 `-SkipResetTests`。

## 当前验证状态

- Debug、Release 和关闭 RTT 的 Debug 配置均已完成编译与链接。
- F103 目标已接入共享 System、Platform F1、DeviceDrivers base、Controller、
  Policy、Filter、RTOS 和 App 占位入口。
- F1 支持的全部 Platform 外设已通过 Debug、Release 独立编译检查。
- 板上非破坏性自检已通过：72 MHz 时钟、FreeRTOS tick、RTT 日志健康和硬件 CRC，
  结果为 `SUMMARY pass=4 fail=0`。
- 板载硬件实测已通过：GPIO 回读、I2C1 的 AHT20 / CH1116 地址、ADC1 DMA、
  TIM1 编码器、TIM2 / TIM3 / TIM4、USART2 PING / TEST、IWDG 复位和 WWDG
  复位，结果为 `TEST pass=8 fail=0 skip=1`。USART3 因未观察到 DX-BT24 响应而
  跳过，需连接模块后复测。
- 定时器占空比、总线时序、WS2812 光输出、舵机运动、继电器和电机实际负载仍需
  按 [硬件与接口规范](hardware-map.md)第 11 节结合示波器或实际负载确认。

## 相关文档

- [根工程说明](../../README.md)
- [架构说明](../../doc/architecture.md)
- [Platform 契约](../../doc/platform.md)
- [硬件与接口规范](hardware-map.md)
- [编码约定](../../doc/conventions.md)

# 板级配置

## 0. 当前板级目标

| Target | MCU | 内核 | 时钟 | 状态 |
| --- | --- | --- | --- | --- |
| F407 | STM32F407VET6 | Cortex-M4F | 168 MHz | 主开发目标 |
| F103 | STM32F103C8T6 | Cortex-M3 | 72 MHz | 基础移植与验证目标 |

F103 工程位于 `boards/f103c8t6/`，使用独立的 F1 HAL、CMSIS、FreeRTOS CM3
端口、启动文件和链接脚本，不覆盖根目录 F407 Core。完整元件和引脚映射见
`boards/f103c8t6/hardware-map.md`。

> 当前 Board 层尚未集中实现。本文记录 CubeMX / Core 中已经存在的配置，
> 以及四轮、STBY、标定等仍待确定的部分。

## 1. MCU

- 型号：STM32F407xx
- 启动文件：`startup_stm32f407xx.s`
- 链接脚本：`STM32F407XX_FLASH.ld`
- RAM：128 KB；CCMRAM：64 KB；FLASH：512 KB
- 编译宏：`USE_HAL_DRIVER`、`STM32F407xx`、`STM32F4xx`、`USE_FREERTOS`

## 2. 时钟

- HSI 16 MHz → PLL
- PLLM = 8，PLLN = 168，PLLP = 2，PLLQ = 4
- SYSCLK = 168 MHz
- AHB / 1；APB1 / 4（42 MHz）；APB2 / 2（84 MHz）
- APB1 定时器时钟 84 MHz；APB2 定时器时钟 168 MHz
- Flash latency：5

## 3. 定时器

| 定时器 | 用途 | 配置 |
| --- | --- | --- |
| TIM1 | 四路 PWM | PSC = 8-1，ARR = 1000-1，PWM1 模式，约 21 kHz；PE9 / PE11 / PE13 / PE14 |
| TIM2 | 编码器 | 32 位，TI12，PA5 / PB3 |
| TIM3 | 编码器 | 16 位，TI12，PA6 / PA7 |
| TIM4 | 编码器 | 16 位，TI12，PD12 / PD13 |
| TIM5 | 编码器 | 32 位，TI12，PA0 / PA1 |

Platform 当前启用的实例与上述配置一致：

| 功能 | 启用实例 |
| --- | --- |
| PWM | `PLATFORM_PWM_TIM1` |
| Encoder | `PLATFORM_ENCODER_TIM2` 到 `PLATFORM_ENCODER_TIM5` |
| UART | `PLATFORM_USART_1`、`PLATFORM_USART_2` |
| CRC | 硬件 CRC1，由 `MX_CRC_Init()` 初始化 |
| RNG | 硬件 RNG1，由 `MX_RNG_Init()` 初始化 |
| IWDG / WWDG | Platform 运行期按超时策略启动 |

## 4. 串口

| 串口 | 用途 | 配置 |
| --- | --- | --- |
| USART1 | WHT101 IMU | 9600 8N1，PA9 / PA10，DMA2 Stream2 RX / Stream7 TX，NVIC 已启用 |
| USART2 | 遥测传输 / 预留命令链路 | 9600 8N1，PA2 / PA3，DMA1 Stream5 RX / Stream6 TX |

注意：USART2 的 NVIC 目前未在 CubeMX 中启用，IT / 错误回调暂不可用。
FireWater / JustFloat 仅使用 DMA TX 时不需要 USART2 NVIC；如果要同时接收命令，
必须启用 USART2 NVIC 或将驱动改为循环 DMA。

遥测开启前应把 USART2 提高到 `921600` 或 `1 Mbps`。9600 baud 只适合极低频
调试，不足以承载 16 通道 50 Hz 的 JustFloat，更不足以承载 FireWater。

SEGGER RTT 使用 SWD 调试通道，不占用 USART。当前 VS Code 调试配置使用
OpenOCD，需要在 OpenOCD 中启用 RTT；使用 J-Link RTT Viewer 时应改用 J-Link
GDB Server，而不是 OpenOCD 的 J-Link interface。

## 5. GPIO

CubeMX 当前将以下引脚配置为推挽输出：

- PD0 .. PD9
- PE5
- PC13

这些引脚的具体含义（电机方向、STBY、LED 等）尚未在 Board 层集中定义。

## 6. 待定映射

以下内容需要在 Board 层确定并写死：

- 四轮编号：`wheel[0..3]` 与物理位置的对应关系。
- 每个轮子的 PWM 通道、IN1/IN2、STBY。
- STBY 分组：计划为 `m0/m1` 一组、`m2/m3` 一组；单电机停机不能依赖共享 STBY。
- 编码器与轮子的对应关系。
- LED 的 GPIO / PWM 映射。
- `Error_Handler` 中切断电机输出的路径。

## 7. 标定常数

待标定：

- 四轮安装角 `wheel_theta[4]`
- 方向符号 `wheel_sign[4]`
- 轮距 `wheel_distance`
- 编码器每圈计数、轮径、减速比 → `counts_per_meter`
- 控制周期
- 轮速 PID 参数（PID 实现后）

## 8. 任务

| 任务 | 栈 | 优先级 |
| --- | --- | --- |
| defaultTask | 512 B | Normal |
| MonitorTask | 2048 B | Low |
| ControlTask | 4096 B | Realtime |
| USARTTask | 4096 B | Normal |

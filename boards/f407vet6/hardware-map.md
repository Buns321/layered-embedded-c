# STM32F407VET6 硬件与接口规范

本文记录 `boards/f407vet6/` 当前 CubeMX 配置确定的引脚、时钟和外设边界。
改动 `.ioc` 或生成代码后必须同步更新本文。

## 1. 芯片与时钟

| 项目 | 当前值 |
| --- | --- |
| MCU | STM32F407VET6 |
| 内核 | Cortex-M4F |
| FLASH / SRAM | 512 KB / 192 KB |
| 时钟源 | HSI 16 MHz |
| PLL | M=8，N=168，P=2，Q=4 |
| SYSCLK / HCLK | 168 MHz |
| APB1 / APB1 timer | 42 MHz / 84 MHz |
| APB2 / APB2 timer | 84 MHz / 168 MHz |
| HAL 时基 | TIM7 |

PH0 / PH1 已保留 HSE 引脚，但当前系统时钟没有选择 HSE。使用外部晶振前必须同时
核对板上晶振频率和 CubeMX 时钟树。

## 2. 调试与启动

| 信号 | 引脚 | 说明 |
| --- | --- | --- |
| SWDIO | PA13 | 保留调试 |
| SWCLK | PA14 | 保留调试 |
| BOOT0 | 芯片引脚 | 由板级硬件决定 |
| NRST | 芯片引脚 | 由板级硬件决定 |

不要把这些引脚复用为普通 GPIO。

## 3. GPIO 输出

以下引脚当前初始化为推挽输出并默认输出低电平：

| 端口 | 引脚 |
| --- | --- |
| GPIOE | PE5 |
| GPIOC | PC13 |
| GPIOD | PD0–PD9 |

相关逻辑电平和负载能力必须以实际原理图为准。PD8 / PD9 也可以在后续 CubeMX
配置中复用为 USART3，但当前不是串口。

## 4. USART 与 DMA

| 实例 | TX | RX | 波特率 | RX DMA | TX DMA |
| --- | --- | --- | --- | --- | --- |
| USART1 | PA9 | PA10 | 9600 | DMA2 Stream2 Ch4，循环 | DMA2 Stream7 Ch4，单次 |
| USART2 | PA2 | PA3 | 9600 | DMA1 Stream5 Ch4，循环 | DMA1 Stream6 Ch4，单次 |

USART1 启用 NVIC。DMA 缓冲区的所有权和生命周期遵循 Platform UART 契约。

## 5. 定时器

| 实例 | 用途 | 引脚 |
| --- | --- | --- |
| TIM1 | 四通道 PWM | PE9 CH1，PE11 CH2，PE13 CH3，PE14 CH4 |
| TIM2 | 编码器 | PA5 CH1，PB3 CH2 |
| TIM3 | 编码器 | PA6 CH1，PA7 CH2 |
| TIM4 | 编码器 | PD12 CH1，PD13 CH2 |
| TIM5 | 编码器 | PA0 CH1，PA1 CH2 |
| TIM7 | HAL 时基 | 内部 |

TIM1 PWM 当前周期为 1000−1，预分频为 8−1。编码器输入滤波器当前为 15。
修改后需要同时验证 Platform 实例宏和板上边沿计数。

## 6. 其他片上资源

- CRC：已生成 `MX_CRC_Init()`。
- RNG：已生成 `MX_RNG_Init()`。
- FreeRTOS：CMSIS-RTOS V2，板内任务入口位于 `RTOS/`。
- RTT：原始源码位于仓库根 `Middlewares/Third_Party/SEGGER_RTT/`。

当前板级 `.ioc` 没有启用 I2C、SPI、ADC、DAC、CAN、RTC、IWDG 或 WWDG 实例。
共享 Platform 对这些实例提供编译检查，不代表它们已经在当前板级初始化。

## 7. 验证边界

迁移完成后至少需要验证：

- Debug 和 Release 固件均能完成编译与链接。
- F4 全分层编译检查覆盖共享层、板内 App 和板内 RTOS。
- RTT 能从 `f407vet6.elf` 的 `_SEGGER_RTT` 控制块读取日志。
- USART、PWM 和编码器引脚与实际原理图一致。
- DMA 通道、NVIC 优先级和外部设备电气电平没有冲突。

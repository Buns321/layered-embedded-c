# 板级配置

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

## 4. 串口

| 串口 | 用途 | 配置 |
| --- | --- | --- |
| USART1 | WHT101 IMU | 9600 8N1，PA9 / PA10，DMA2 Stream2 RX / Stream7 TX，NVIC 已启用 |
| USART2 | 预留命令链路 | 9600 8N1，PA2 / PA3，DMA1 Stream5 RX / Stream6 TX |

注意：USART2 的 NVIC 目前未在 CubeMX 中启用，IT / 错误回调暂不可用。

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

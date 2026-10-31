# STM32F407VET6 硬件与接口规范

本文记录 `Boards/f407vet6-omni-car/` 当前 CubeMX 配置确定的引脚、时钟和外设边界。
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

以下引脚当前初始化为推挽输出并默认输出低电平。PC13 和 PE5 是 TB6612 的成组
STBY，不是可独立停机的电机使能：

| 信号 | 轮位 | 引脚 | 说明 |
| --- | --- | --- | --- |
| AIN1_A | A | PD0 | TIM1 CH1 对应轮 |
| AIN2_A | A | PD1 | A 轮方向 |
| AIN1_B | B | PD2 | TIM1 CH2 对应轮 |
| AIN2_B | B | PD3 | B 轮方向 |
| AIN1_C | C | PD4 | TIM1 CH3 对应轮 |
| AIN2_C | C | PD5 | C 轮方向 |
| AIN1_D | D | PD6 | TIM1 CH4 对应轮 |
| AIN2_D | D | PD7 | D 轮方向 |
| STBY_AB | A、B | PC13 | A/B 两路 TB6612 共用 |
| STBY_CD | C、D | PE5 | C/D 两路 TB6612 共用 |

轮位 A、B、C、D 从车顶向下看按顺时针排列，安装方向依次为 1:30、4:30、
7:30、10:30。以车头为 0°、向左为正、逆时针增加时，四轮角度为
A=315°、B=225°、C=135°、D=45°。正 duty 的最终机械方向仍由电机接线和
`wheel_sign` 标定共同决定。

## 4. USART 与 DMA

| 实例 | TX | RX | 波特率 | RX DMA | TX DMA |
| --- | --- | --- | --- | --- | --- |
| USART1 | PA9 | PA10 | 9600 | DMA2 Stream2 Ch4，循环 | DMA2 Stream7 Ch4，单次 |
| USART2 | PA2 | PA3 | 115200 | DMA1 Stream5 Ch4，循环 | DMA1 Stream6 Ch4，单次 |
| USART3 | PD8 | PD9 | 115200 | 未启用 | 未启用 |

USART1 和 USART3 启用 NVIC。DMA 缓冲区的所有权和生命周期遵循 Platform UART
契约。USART3 的 `JoyCon` 接收使用单字节中断和设备层环形队列。

| 数据链路 | MCU 引脚 | 对端信号 | 方向 |
| --- | --- | --- | --- |
| WHT101 | PA9 / PA10 | WHT101 RX / TX | 双向，9600 |
| JustFloat | PA2 | 蓝牙模块 RX | MCU 到蓝牙，115200 |
| JoyCon | PD9 | 蓝牙模块 TX | 蓝牙到 MCU，115200 |

一个普通透明传输蓝牙模块的 TX/RX 分别连接 USART3_RX 和 USART2_TX。PA3 和
PD8 虽然由 CubeMX 配置为 UART 复用，但当前接线不使用。所有 UART 必须使用
3.3 V TTL 电平并共地；不得把 5 V 信号直接接入 MCU。

## 5. 定时器

| 实例 | 用途 | 引脚 |
| --- | --- | --- |
| TIM1 | A–D 四通道 PWM | PE9 CH1，PE11 CH2，PE13 CH3，PE14 CH4 |
| TIM2 | A 轮编码器 | PA5 CH1，PB3 CH2 |
| TIM3 | B 轮编码器 | PA6 CH1，PA7 CH2 |
| TIM4 | C 轮编码器 | PD12 CH1，PD13 CH2 |
| TIM5 | D 轮编码器 | PA0 CH1，PA1 CH2 |
| TIM7 | HAL 时基 | 内部 |

TIM1 PWM 当前周期为 1000−1，预分频为 8−1。编码器输入滤波器当前为 15。
修改后需要同时验证 Platform 实例宏和板上边沿计数。

## 6. 其他片上资源

- CRC：已生成 `MX_CRC_Init()`。
- RNG：已生成 `MX_RNG_Init()`。
- FreeRTOS：CMSIS-RTOS V2，板内任务入口位于 `RTOS/`。
- RTT：原始源码位于仓库根 `Middlewares/Third_Party/SEGGER_RTT/`。
- WHT101：配置为 10 Hz，并在启动时写入设备侧 yaw 零点；写入完成后执行软件
  残余零点修正。
- JoyCon：ASCII 报文 `[j,lx,ly,rx,ry]`，数值范围 `[-100,100]`，换行可选。
- JustFloat：16 通道、50 Hz，通过 USART2 TX DMA 输出。

F407 JustFloat 通道定义：

| 通道 | 含义 | 单位 |
| --- | --- | --- |
| 0–3 | A–D 目标轮速 | m/s |
| 4–7 | A–D 实测轮速 | m/s |
| 8 | 整车世界系目标平动速度 | m/s |
| 9 | 整车世界系目标方向 | rad |
| 10 | 整车目标角速度 | rad/s |
| 11 | WHT101 yaw | deg |
| 12 | WHT101 gyro-Z | deg/s |
| 13 | JoyCon `lx` 归一化值 | `[-1,1]` |
| 14 | JoyCon `ly` 归一化值 | `[-1,1]` |
| 15 | JoyCon `rx` 归一化值 | `[-1,1]` |

当前板级 `.ioc` 没有启用 I2C、SPI、ADC、DAC、CAN 或 RTC 实例。
共享 Platform 对这些实例提供编译检查，不代表它们已经在当前板级初始化。

## 7. 验证边界

迁移完成后至少需要验证：

- Debug 和 Release 固件均能完成编译与链接。
- F4 全分层编译检查覆盖共享层、板内 App 和板内 RTOS。
- RTT 能从 `f407vet6.elf` 的 `_SEGGER_RTT` 控制块读取日志。
- USART、PWM 和编码器引脚与实际原理图一致。
- DMA 通道、NVIC 优先级和外部设备电气电平没有冲突。

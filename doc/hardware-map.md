# STM32F407VET6 硬件与接口规范

本文记录根目录 [f407vet6.ioc](../f407vet6.ioc) 与 [User/board_cfg.c](../User/board_cfg.c) 确定的硬件引脚、时钟、中断、DMA 与应用任务规格。改动 `.ioc` 或硬件映射时必须同步更新本文。

## 1. 芯片、时钟与调试

| 项目 | 当前规格 |
| --- | --- |
| MCU / 内核 | STM32F407VET6 / Cortex-M4F（512 KB Flash，192 KB SRAM） |
| 时钟树 | HSI 16 MHz → PLL（M=8, N=168, P=2, Q=4）→ SYSCLK/HCLK 168 MHz |
| APB1 / APB2 | APB1 42 MHz（Timer 84 MHz）/ APB2 84 MHz（Timer 168 MHz） |
| HAL 时基 / OS 时基 | `TIM7`（HAL `uwTick`）/ `SysTick`（FreeRTOS） |
| SWD 调试 | `PA13`（SWDIO）、`PA14`（SWCLK）；禁止启用完整 JTAG 占用 `PB3` |

## 2. 四轮位姿与 TB6612 / 编码器映射

俯视轮位按顺时针排列（车头为 `0°`，逆时针为正）：A = `315°`（前右 `1:30`）、B = `225°`（后右 `4:30`）、C = `135°`（后左 `7:30`）、D = `45°`（前左 `10:30`）。

| 轮位 | `theta_rad` | PWM（TIM1） | 方向 IN1 / IN2 | STBY（共用） | 编码器定时器（A/B 相） |
| --- | --- | --- | --- | --- | --- |
| A（`wheels[0]`） | `1.75 * PI` | `PE9`（CH1） | `PD0` / `PD1` | `PC13` | `TIM2`（`PA5` / `PB3`） |
| B（`wheels[1]`） | `1.25 * PI` | `PE11`（CH2） | `PD2` / `PD3` | `PC13` | `TIM3`（`PA6` / `PA7`） |
| C（`wheels[2]`） | `0.75 * PI` | `PE13`（CH3） | `PD4` / `PD5` | `PE5` | `TIM4`（`PD12` / `PD13`） |
| D（`wheels[3]`） | `0.25 * PI` | `PE14`（CH4） | `PD6` / `PD7` | `PE5` | `TIM5`（`PA0` / `PA1`） |

- `TIM1` PWM：`Prescaler = 8 - 1`，`Period = 1000 - 1`（`board_cfg.c` `max_duty = 1000`）。
- 编码器：`TIM2`–`TIM5` 编码器接口模式，输入滤波 `15`。
- 电源安全：`VM` 接电机电源，逻辑 `3.3 V` 与 MCU/WHT101/蓝牙共地；`STBY` 初始拉低由固件按运行状态接管。

## 3. USART、DMA 与 NVIC 中断配置

| 实例 | TX / RX 引脚 | 波特率 | DMA / 中断模式 | 对端与用途 |
| --- | --- | --- | --- | --- |
| `USART1` | `PA9` / `PA10` | `9600` | RX `DMA2_Stream2`（Ch4 Circular）、TX `DMA2_Stream7`（Ch4 Normal）、`USART1_IRQn`（5, 0） | WHT101 IMU（驱动以 `BSP_UART_RX_IT_BYTE` 接收，上电自动配置 10 Hz 与归零） |
| `USART2` | `PA2` / `PA3` | `115200` | RX `DMA1_Stream5`（Ch4 Circular）、TX `DMA1_Stream6`（Ch4 Normal）、`USART2_IRQn`（5, 0） | 蓝牙 RX（JustFloat 仅 TX，`PA3` 悬空）；**已补齐 `USART2_IRQn` 与 `USART2_IRQHandler()`** 触发 `TC` 完成回调 |
| `USART3` | `PD8` / `PD9` | `115200` | 无 DMA，`USART3_IRQn`（5, 0） | 蓝牙 TX（JoyCon `[j,lx,ly,rx,ry]` 单字节中断接收，`PD8` 悬空） |

> [!IMPORTANT]
> 所有 UART 信号必须为 `3.3 V TTL` 并共地；5 V 串口蓝牙模块须经电平转换后再接 `PD9`。

## 4. 遥测通道与片上资源

- **JustFloat（`USART2` DMA，50 Hz，16 通道）**：
  - `ch0..3`：A–D 目标轮速（`m/s`）
  - `ch4..7`：A–D 实测轮速（`m/s`）
  - `ch8..10`：目标车体系速度 `vx`（`m/s`）、`vy`（`m/s`）、`wz`（`rad/s`）
  - `ch11..12`：WHT101 `yaw`（`deg`）、`gyro_z`（`deg/s`）
  - `ch13..15`：JoyCon 归一化 `lx`、`ly`、`rx`（`[-1, 1]`）
- **SEGGER RTT**：源码位于 [Framework/ThirdParty/SEGGER_RTT/](../Framework/ThirdParty/SEGGER_RTT/)，通过 SWD 通道 0 输出系统日志（SRAM 控制块地址扫描范围 `0x20000000` 长度 `0x20000`）。
- **片上校验/外设**：已启用 `MX_CRC_Init()` 与 `MX_RNG_Init()`；当前 `.ioc` 未启用 I2C/SPI/ADC/DAC/CAN/RTC。

## 5. FreeRTOS 静态任务与安全配置

- 配置约束：`configSUPPORT_STATIC_ALLOCATION = 1`、`configSUPPORT_DYNAMIC_ALLOCATION = 0`、`configCHECK_FOR_STACK_OVERFLOW = 2`（由 [Core/Inc/FreeRTOSConfig.h](../Core/Inc/FreeRTOSConfig.h) `USER CODE Defines` 强制覆盖），不链接 `heap_4.c`。
- 任务清单（定义于 [User/rtos_tasks.c](../User/rtos_tasks.c)）：

| 任务变量 | 周期 | 优先级 | 静态栈（words / bytes） | 入口函数 |
| --- | --- | --- | --- | --- |
| `s_control_task` | `10 ms`（100 Hz） | `osPriorityRealtime` | `1024` / `4096 B` | `App_Car_StepControl` |
| `s_input_task` | `2 ms`（500 Hz） | `osPriorityNormal` | `768` / `3072 B` | `App_Car_StepInput` |
| `s_service_task` | `10 ms`（100 Hz） | `osPriorityLow` | `512` / `2048 B` | `App_Car_StepService` |

- `Car_Omni_Init()` 自动向 `rtos_task` 注册紧急断电回调；若触发栈溢出，强实现的 `vApplicationStackOverflowHook()` 立即切断四轮 PWM 与 STBY 并停机。

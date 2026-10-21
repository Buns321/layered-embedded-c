# F103C8T6 Demo 硬件映射

本文记录集成开发板的元件、MCU 引脚、计划使用的片上外设，以及接线时必须
注意的电气和资源冲突。本文是 Board/App 接线和 CubeMX 配置的依据。

## 1. 基础资源

| 项目 | 配置 |
| --- | --- |
| MCU | STM32F103C8T6，LQFP48 |
| 内核 | Cortex-M3 |
| FLASH / SRAM | 64 KB / 20 KB |
| 外部高速时钟 | 8 MHz HSE，PLL x9，SYSCLK 72 MHz |
| 调试 | SWD，PA13 / PA14 |
| HAL timebase | SysTick，当前与 CMSIS-RTOS2 共用 |
| FreeRTOS | CMSIS-RTOS V2，GCC ARM_CM3 端口 |
| 日志 | SEGGER RTT，经 SWD |

## 2. 元件与引脚

| 功能 | 元件 / 器件 | MCU 外设 | 引脚 | 说明 |
| --- | --- | --- | --- | --- |
| 调试串口 | CH343P USB-TTL | USART2 | PA2 TX / PA3 RX | CH343P TXD 接 PA3，RXD 接 PA2 |
| BLE 串口 | BLE 模块 | USART3 | PB10 TX / PB11 RX | 波特率和协议待确认 |
| 温湿度 | AHT20 | I2C1 | PB6 SCL / PB7 SDA | 7 位地址 0x38，HAL 8 位地址 0x70 |
| OLED | CH1116 | I2C1 | PB6 SCL / PB7 SDA | 7 位地址 0x3D，HAL 8 位地址 0x7A |
| NTC 温度 | NTC 分压 | ADC1_IN4 | PA4 | 分压参数、NTC B 值待确认 |
| 可调电阻 | 电位器 | ADC1_IN5 | PA5 | 用于 ADC/Filter Demo |
| 循迹 | TCRT 模块 | GPIO 输入 | PB14 | 数字输出；有效电平待确认，可使用 EXTI14 |
| 超声测距 | HC-SR04 | GPIO | PA11 TRIG / PA10 ECHO | TRIG 输出，ECHO 输入 |
| 按键 1 | KEY_1 | GPIO / EXTI12 | PB12 | 建议 IPU，低有效 |
| 按键 2 | KEY_2 | GPIO / EXTI13 | PB13 | IPU，低有效 |
| 舵机 | Servo | TIM4_CH3 | PB8 | 50 Hz，0.5 到 2.5 ms 脉宽 |
| 直流电机 | DRV8833 | TIM2_CH1 / TIM2_CH2 | PA0 / PA1 | 两路 PWM 或方向控制 |
| 旋转编码器 | EC11 | TIM1 Encoder | PA8 CH1 / PA9 CH2 | 建议 TI12 编码器模式 |
| 编码器按键 | EC11 BTN | GPIO / EXTI15 | PB15 | 建议 IPU，低有效 |
| 无源蜂鸣器 | Buzzer | TIM4_CH4 | PB9 | 与舵机共用 TIM4 |
| RGB 灯串 | 10 x WS2812C | TIM3_CH1 + DMA | PB4 | GRB，5 V 供电，开漏输出 |
| 三色灯珠 | RGB LED | GPIO / PWM 待定 | R PB0 / G PA7 / B PB6 | PB6 与 I2C1_SCL 冲突 |
| 单路继电器 | SRD-05VDC-SL-C + SS8050 | GPIO 推挽输出 | PB5 | 高电平吸合，低电平释放 |

USB Type-C 当前标记为不可用，不纳入 Demo。继电器现已可用。

### 三色灯珠资源冲突

三色灯珠当前映射：

| 通道 | 引脚 | 可用复用 | 冲突 |
| --- | --- | --- | --- |
| Red | PB0 | GPIO / ADC1_IN8 / TIM3_CH3 | 无直接冲突 |
| Green | PA7 | GPIO / SPI1_MOSI / TIM3_CH2 | SPI1 当前未使用 |
| Blue | PB6 | GPIO / I2C1_SCL | 与 AHT20、CH1116 OLED 的 I2C1_SCL 直接冲突 |

PB6 不能同时作为 I2C1_SCL 和蓝色 GPIO/PWM 输出。可选处理方式：

1. 将蓝色通道改接到其他空闲 GPIO，这是推荐方案。
2. 在使用三色灯珠时关闭 I2C1，不再访问 AHT20 和 OLED。
3. 在 I2C 空闲时临时切换 PB6 功能，但不推荐。总线状态会被破坏，AHT20 和 OLED
   访问必须完全停止。

如果只把三色灯珠作为普通开关灯使用，可以直接配置三个 GPIO。若需要 PWM 调光：

- PA7 和 PB0 对应 TIM3_CH2/TIM3_CH3，但 TIM3 已规划给 WS2812 的 72 MHz
  PWM+DMA。不能同时按另一组 PWM 时基使用。
- 因此当前硬件下，三色灯珠优先采用 GPIO 开关，或仅对红色、绿色使用其他可用的
  软件/定时器方案。
- 还需确认灯珠是共阴还是共阳，以及高电平还是低电平点亮。当前映射没有提供有效
  电平和限流电阻信息。

## 3. I2C1 总线

AHT20 和 CH1116 OLED 共用 I2C1：

- SCL: PB6
- SDA: PB7
- 建议先用 100 kHz 标准模式完成验证，再评估 400 kHz。
- 两条设备地址不冲突。
- HAL API 通常接收左移后的 8 位地址：
  - AHT20: `0x70`
  - CH1116: `0x7A`
- 7 位地址分别为 `0x38` 和 `0x3D`。日志和文档中必须标明使用的是哪一种。
- AHT20 和 OLED 的访问必须串行化。FreeRTOS 环境下需要总线互斥和错误恢复策略。
- CH1116 的初始化命令、显存布局和分辨率尚未提供，不能直接假定完全兼容某个
  SSD1306 驱动。

## 4. 串口资源

### USART2: CH343P

- MCU TX: PA2
- MCU RX: PA3
- CH343P 的 TXD 接 MCU RX，RXD 接 MCU TX。
- 建议作为主调试、命令和遥测串口。
- 初始波特率可先使用 115200 8N1，实际以 CH343P 和上位机配置为准。

### USART3: BLE

- MCU TX: PB10
- MCU RX: PB11
- BLE 模块型号、AT 协议、波特率、流控和配对方式未提供。
- 在协议明确前，只建立 USART3 收发和回环测试，不实现 BLE 业务。

两个串口不能共享同一个 `usart_port_t` 回调实例。USART2 和 USART3 各自持有独立
回调 owner。

## 5. ADC

| 通道 | 引脚 | 用途 |
| --- | --- | --- |
| ADC1_IN4 | PA4 | NTC 分压 |
| ADC1_IN5 | PA5 | 可调电阻 |

计划使用 ADC1 扫描和 DMA 周期采样：

1. 将两路原始 counts 送入 Filter。
2. NTC 使用分压公式和 B 值换算温度。
3. 可调电阻用于验证 Filter、遥测和阈值事件。

NTC 分压电阻、上拉电压、B 值和目标温度范围尚未提供，因此温度换算暂不写死。

## 6. 数字输入

| 信号 | 引脚 | 建议模式 |
| --- | --- | --- |
| TCRT DO | PB14 | 输入，上拉或浮空取决于模块输出类型 |
| KEY_1 | PB12 | 输入上拉，低有效 |
| KEY_2 | PB13 | 输入上拉，低有效 |
| EC11 BTN | PB15 | 输入上拉，低有效 |
| HC-SR04 ECHO | PA10 | 输入 |

按键需要软件消抖。EC11 按键与 Phase_A/Phase_B 建议进入同一输入事件处理模块。

HC-SR04 的 ECHO 通常为 5 V 输出。即使 PA10 具有 FT 特性，也建议确认板级电平
和限流，必要时使用分压或电平转换。

## 7. 定时器资源

| 定时器 | 用途 | 引脚 | 备注 |
| --- | --- | --- | --- |
| TIM1 | EC11 编码器 | PA8 / PA9 | TI12，保留高级定时器 |
| TIM2 | DRV8833 PWM | PA0 / PA1 | TIM2_CH1 / TIM2_CH2 |
| TIM3 | WS2812 DMA | PB4 | TIM3_CH1 partial remap |
| TIM4 | Servo + Buzzer | PB8 / PB9 | CH3 / CH4 共用时基 |

### TIM4 资源冲突

舵机要求 50 Hz、20 ms 周期，脉宽约 0.5 到 2.5 ms。

无源蜂鸣器通常需要几百 Hz 到几 kHz 的 PWM，而 TIM4_CH3 和 TIM4_CH4 共用同一个
PSC/ARR。它们不能在同一时刻使用不同频率：

- 同频方案会让蜂鸣器工作在 50 Hz，基本不可用。
- 动态改 TIM4 时基会让舵机输出中断或抖动。
- 推荐 Demo 方案：TIM4 固定 50 Hz 驱动舵机，蜂鸣器改由软件定时 GPIO 翻转，
  或只在舵机停止时临时切换 TIM4。
- 如果必须同时使用舵机和高频 PWM 蜂鸣器，需要调整硬件，把蜂鸣器移到其他定时器
  通道或使用外部驱动。

这个冲突必须在 App/Board 接线前解决，不能由驱动层偷偷修改 TIM4 周期。

## 8. DRV8833

| DRV8833 | MCU |
| --- | --- |
| PIN3 | PA1 / TIM2_CH2 |
| PIN4 | PA0 / TIM2_CH1 |

需要确认 PIN3/PIN4 对应 IN1/IN2 还是其他控制输入，并确认：

- nSLEEP 是否已硬件上拉。
- nFAULT 是否连接到 MCU，当前映射未提供。
- 电机电源、电流限制和共地方式。
- 是否使用两路 PWM 还是 PWM + 方向模式。

## 9. EC11 编码器

- Phase_A: PA8 / TIM1_CH1
- Phase_B: PA9 / TIM1_CH2
- Button: PB15 / EXTI15

计划使用 TIM1 的 TI12 编码器模式。PA9 虽然也是 USART1_TX 的复用引脚，但当前
Demo 不使用 USART1，因此不冲突。

## 10. 超声测距

- TRIG: PA11
- ECHO: PA10

PA11 默认也是 USB_DM，但本板 USB Type-C 不可用，因此可释放为 GPIO 输出。

实现方式优先级：

1. TIM 输入捕获测量 ECHO 高电平时间。
2. 简单阻塞测距，仅用于早期点灯和串口验证。

不能在高优先级控制任务中使用长时间阻塞测距。

## 11. 单路 5 V 继电器

### 电路拓扑

| 项目 | 连接 |
| --- | --- |
| MCU 控制 | PB5 |
| 基极限流 | R9 = 3 kΩ |
| 三极管 | SS8050，NPN |
| 继电器 | SRD-05VDC-SL-C |
| 线圈电源 | 5 V |
| 续流二极管 | FR107，反向并联在线圈两端 |
| 输出端子 | COM、OPEN/NO、CLOSE/NC |

电路为高边继电器线圈，低边 NPN 开关：

- PB5 高电平：Q3 导通，线圈通电，COM 与 OPEN 吸合。
- PB5 低电平：Q3 截止，线圈断电，COM 与 CLOSE 释放。
- 控制逻辑为高有效。
- 继电器没有辅助触点或状态反馈，属于纯开环控制。

### GPIO 电气配置

- PB5 必须配置为推挽输出。
- 禁止使用开漏输出。开漏高电平为高阻态，无法给 NPN 基极提供电流。
- 输出速度使用中速或低速即可，不需要高速翻转。
- CubeMX 初始化时必须先写输出低电平，再配置 GPIO 输出模式，或配置后立刻拉低。
- 软件初始化阶段必须再次显式写入低电平，确保继电器处于释放状态。

### 上电安全

STM32 复位后 GPIO 默认是高阻输入，软件第一次执行前不能完全保证 PB5 为低。
如果外部负载不允许上电瞬间误动作，硬件上应在 PB5 与 GND 之间，或三极管基极与
GND 之间增加下拉电阻，建议 10 kΩ。软件拉低不能替代硬件下拉。

### 安全限制

- 继电器触点侧绝对不要接入超过 36 V 的电压。
- 首次验证只使用万用表或低压小负载，不要直接连接市电或大功率设备。
- 强电侧和 MCU 侧必须保持足够爬电距离和隔离。
- FR107 的方向必须是线圈断电时正向导通，不能反接。

### 驱动定义

按项目命名约定，规划 API 与用户提出的接口对应如下：

| 用户接口 | 项目规划接口 | 语义 |
| --- | --- | --- |
| `Relay_Init()` | `Dev_Relay_GPIO_Init()` | 绑定 GPIO，并确保默认关闭 |
| `Relay_Control(1)` | `Dev_Relay_Base_ON()` | 输出高电平，继电器吸合 |
| `Relay_Control(0)` | `Dev_Relay_Base_OFF()` | 输出低电平，继电器释放 |
| `Relay_Toggle()` | `Dev_Relay_Base_Toggle()` | 翻转软件记录的状态 |
| 无 | `Dev_Relay_Base_ReadStatus()` | 读取最近一次命令状态 |

规划对象继续采用 base + GPIO 具体实现：

```c
typedef enum {
  DEV_RELAY_OFF = 0U,
  DEV_RELAY_ON,
} Dev_Relay_State;
```

`Dev_Relay_GPIO_Init()` 只负责把 Platform GPIO 和继电器对象绑定，并把输出写成
关闭状态。PB5 的时钟、推挽模式和初始电平仍由 CubeMX/Core 配置。

### 验证方法

1. 不接负载，确认上电后 COM 与 CLOSE 导通。
2. 调用 ON，确认 COM 与 OPEN 导通，并伴随吸合声。
3. 调用 OFF，确认回到 COM 与 CLOSE 导通。
4. 连续 Toggle，检查软件状态和实际触点一致。
5. 最后再接入不超过 36 V 的低压测试负载。

## 12. WS2812C 灯串

### 规模

- 10 颗灯珠，L2 到 L11 级联。
- PB4 接第一颗 DI。
- 前一颗 DO 接下一颗 DI。
- 每颗灯 24 bit，全部灯珠共 240 bit。

### 数据格式

- 顺序：G7..G0, R7..R0, B7..B0。
- MSB first。
- 逻辑 0: 高约 0.4 us，低约 0.85 us。
- 逻辑 1: 高约 0.8 us，低约 0.45 us。
- Reset: 低电平至少 50 us。

### PB4 配置

- PB4 默认是 JTAG NJTRST，必须在 GPIO 初始化前关闭 JTAG 并保留 SWD：
  `__HAL_AFIO_REMAP_SWJ_NOJTAG()`。
- PB4 输出速度必须为最高速度。
- 要求使用开漏输出，但开漏只能主动拉低，线路上必须有外部上拉到 5 V。
- 5.1 kΩ 串联电阻不是上拉电阻。若板上没有上拉，开漏模式无法产生有效高电平。
- FT 表示输入可耐受 5 V，不代表输出可以主动输出 5 V。
- 应检查 WS2812 DI 的 VIH。如果 3.3 V 高电平裕量不足，需要使用电平转换器或
  有上拉的开漏方案。
- 10 颗灯全白电流可能接近 600 mA，需要足够容量的 5 V 电源、共地以及就近储能
  电容。每颗灯 100 nF 去耦不能替代总电源储能。

### 推荐驱动拓扑

采用 TIM3_CH1 PWM + DMA：

- TIM3 时钟 72 MHz。
- ARR = 89，单 bit 周期 90 tick，约 1.25 us。
- 逻辑 0 的 CCR 约 28，高电平约 0.389 us。
- 逻辑 1 的 CCR 约 56，高电平约 0.778 us。
- 每个灯 24 个 CCR 值，10 颗灯共 240 项。
- 帧尾追加至少 45 个零占空比周期，建议 64 项，约 80 us Reset。
- DMA 内存到 TIM3_CCR1，16 位宽度。
- DMA 完成后才能修改发送缓冲区或启动下一帧。

具体 DMA channel、TIM3 partial remap 和 AFIO 配置必须由 CubeMX 生成并在板上
确认。

### 规划 API

按项目命名约定，落地函数使用 `Dev_WS2812_*` 前缀，语义对应用户提出的
`WS2812_*`：

```c
Platform_StatusTypeDef Dev_WS2812_Init(ws2812_t *this);
void Dev_WS2812_SetColor(
  ws2812_t *this,
  uint8_t index,
  uint8_t r,
  uint8_t g,
  uint8_t b
);
Platform_StatusTypeDef Dev_WS2812_Show(ws2812_t *this);
```

- `SetColor` 只修改内存缓冲区，不启动 DMA。
- `Show` 非阻塞启动整帧 DMA。
- DMA 忙时返回 `Platform_BUSY`。
- 帧缓冲、完成同步和状态均按静态对象设计。

### main 测试示例

```c
static ws2812_t g_ws2812;

if (Dev_WS2812_Init(&g_ws2812) != Platform_OK) {
  Error_Handler();
}

for (;;) {
  for (uint8_t i = 0; i < 10u; i++) {
    Dev_WS2812_Clear(&g_ws2812);
    Dev_WS2812_SetColor(&g_ws2812, i, 0u, 0u, 32u);
    (void)Dev_WS2812_Show(&g_ws2812);
    osDelay(150u);
  }
}
```

正式实现中应等待上一帧 DMA 完成后再提交下一帧，不能覆盖正在发送的缓冲区。

## 13. Demo 任务建议

| 任务 | 职责 | 建议周期 |
| --- | --- | --- |
| ControlTask | EC11、电机、舵机、超声 | 10 到 20 ms |
| SensorTask | AHT20、ADC、NTC、TCRT | 50 到 100 ms |
| DisplayTask | OLED、WS2812、蜂鸣器 | 100 ms 或事件驱动 |
| USARTTask | CH343P 命令/遥测、BLE 预留 | 10 ms |
| MonitorTask | IWDG、健康状态、故障停机 | 100 ms |

F103C8T6 只有 20 KB SRAM。任务栈必须逐项测量，最终应尽量改为静态分配。

## 14. 验证顺序

1. SWD + RTT + 72 MHz + FreeRTOS 心跳。
2. USART2 CH343P 回环和命令。
3. I2C 扫描，确认 0x38 和 0x3D。
4. AHT20 温湿度读取。
5. CH1116 OLED 初始化和显示。
6. ADC 两路采样和 Filter。
7. KEY、EC11、TCRT 输入。
8. 三色灯珠，先确认 PB6 冲突处理方案。
9. 超声测距。
10. 继电器无负载吸合和释放。
11. 舵机 PWM。
12. DRV8833 电机方向和速度。
13. 蜂鸣器，按 TIM4 冲突方案验证。
14. WS2812 10 灯跑马灯。
15. IWDG 实际复位。
16. USART3 BLE 回环，业务协议后续实现。

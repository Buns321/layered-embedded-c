# Platform 层契约

## 1. 职责边界

Platform 是静态 HAL 适配层。上层通过枚举身份调用外设，不接触 HAL handle。

CubeMX/Core 负责结构性初始化：

- 引脚复用、上下拉和速度。
- RCC 时钟与分频。
- DMA stream、方向和模式。
- NVIC 优先级与使能。
- `MX_*_Init()`、`HAL_*_MspInit()` 和全局 HAL handle。

Platform 负责运行期操作：

- 枚举身份到 HAL handle 的私有映射。
- 启动、停止、校准和传输。
- 计数器、状态和错误快照读取。
- HAL 回调到 Platform 回调的分发。

除运行期启动不可逆的 IWDG/WWDG 外，Platform 实现不得调用 `HAL_*_Init()`、
`HAL_GPIO_Init()`、`HAL_NVIC_SetPriority()` 或 `__HAL_RCC_*_CLK_ENABLE()`。

## 2. 公共与私有文件

`Platform/Inc/` 只包含：

- 枚举和普通数据结构。
- 回调类型。
- 状态码和不依赖 HAL 类型的常量。
- 公共函数声明。

`Platform/Src/` 可以包含 HAL、CMSIS 和 CubeMX 生成的 handle。

`platform_hal_internal.h` 是私有状态转换头，不得被 DeviceDrivers、
Controller、Policy、Filter、RTOS 或 App 包含。

## 3. 实例可用性

每个功能使用独立的实例类型：

- `Platform_PWM_Instance`
- `Platform_PWM_Channel`
- `Platform_Encoder_Instance`
- `Platform_UART_Instance`
- `Platform_I2C_Instance`
- `Platform_SPI_Instance`
- `Platform_ADC_Instance`
- `Platform_DAC_Instance`
- `Platform_CAN_Instance`
- `Platform_RTC_Instance`
- `Platform_FLASH_Instance`

未启用的实例不会进入公共枚举。当前板级通过 CMake 定义启用：

```text
PLATFORM_ENABLE_PWM_TIM1=1
PLATFORM_ENABLE_ENCODER_TIM2=1
PLATFORM_ENABLE_ENCODER_TIM3=1
PLATFORM_ENABLE_ENCODER_TIM4=1
PLATFORM_ENABLE_ENCODER_TIM5=1
PLATFORM_ENABLE_UART1=1
PLATFORM_ENABLE_UART2=1
PLATFORM_ENABLE_I2C1=1
PLATFORM_ENABLE_SPI1=1
PLATFORM_ENABLE_ADC1=1
PLATFORM_ENABLE_CAN1=1
PLATFORM_ENABLE_RTC=1
PLATFORM_ENABLE_FLASH=1
PLATFORM_ENABLE_CRC=1
PLATFORM_ENABLE_RNG=1
PLATFORM_ENABLE_IWDG=1
PLATFORM_ENABLE_WWDG=1
```

如果启用了实例但 CubeMX 没有生成对应 handle，Platform 编译失败。这样可以在
编译期发现 `.ioc`、HAL 源文件和 Platform 配置不一致。

## 4. PWM 契约

- `Platform_PWM_Start/Stop` 只接受一个 PWM 通道。
- `Platform_PWM_StartAll/StopAll` 显式表达“全部通道”。
- `Platform_PWM_SetCompare/SetDuty` 不接受 `ALL`，避免误写成只修改 CCR4。
- 比较值会夹紧到 ARR。
- `Platform_PWM_SetDuty` 使用 64 位中间量计算 CCR。

## 5. Encoder 契约

- 编码器模式由 CubeMX 配置，Platform 不选择 TI1、TI2 或 TI12。
- `Platform_Encoder_Start()` 固定启动定时器的两个编码器通道。
- PWM 定时器不能通过编码器 API 启动，反之亦然。

## 6. UART 回调所有权

- `usart_port_t` 是物理 UART 的 Platform 回调所有者。
- WHT101、FireWater 或 JustFloat 是 `usart_base_t` 的回调所有者。
- 一个物理 UART 只能有一套 Platform 回调。
- 回调必须在启动 IT/DMA 传输前注册。
- 注册时复制回调结构，context 本身不被 Platform 拥有。
- 回调在中断上下文执行，不能阻塞。

## 7. UART 缓冲区契约

- Platform 不复制 IT/DMA 缓冲区。
- IT/DMA 启动成功后，缓冲区必须保持有效，直到完成、错误或 abort 回调。
- 半完成回调时只能访问已经完成的那一半。
- Platform 不自动重新挂接收，设备驱动负责重新挂载。
- DMA 发送错误后，设备驱动应使用显式 abort 接口恢复状态。

## 8. 超时与错误

- 阻塞接口的 `timeout` 单位为毫秒。
- `PLATFORM_TIMEOUT_INFINITE` 表示永久阻塞。
- IT/DMA 接口没有阻塞超时，超时由设备层状态机判断。
- `Platform_StatusTypeDef` 只表达粗粒度结果。
- `Platform_UART_GetLastError()` 返回 PE/NE/FE/ORE/DMA 等错误位。
- 新操作成功启动后，HAL ErrorCode 由 HAL 或显式 abort/clear 路径复位。

## 9. CRC、RNG 与看门狗

CRC 和 RNG 的结构性初始化由 CubeMX/Core 负责：

- `MX_CRC_Init()` 生成 `hcrc` 并开启 CRC 时钟。
- `MX_RNG_Init()` 生成 `hrng` 并开启 RNG 时钟。
- Platform 只提供 CRC 重置、32 位字累积/计算和 RNG 取数/错误快照。

IWDG 和 WWDG 是运行期安全策略，具有不可逆启动语义：

- 不在 `main()` 早期自动调用 CubeMX 的看门狗启动函数。
- `Platform_IWDG_Start()` 和 `Platform_WWDG_Start()` 在监控任务准备好后配置并启动。
- IWDG 根据目标毫秒数选择预分频和重装值，超时受 LSI 偏差影响。
- WWDG 根据 PCLK1 选择预分频，并验证刷新窗口。
- 这是 Platform 内允许执行运行期结构配置和时钟使能的明确例外。
- `Sys_Watchdog` 只依赖 Platform IWDG，不再直接访问 IWDG 寄存器。

## 10. 新增外设流程

1. 在 `.ioc` 中配置外设、引脚、时钟、DMA 和 NVIC。
2. 由 CubeMX 生成 `MX_*_Init()` 和 handle。
3. 在 `platform_config.h` 中声明可用实例。
4. 在 CMake 中启用对应实例和 HAL 源文件。
5. 增加不依赖 HAL 类型的公共头。
6. 在对应 MCU 的私有实现中把实例映射到 handle。
7. 补齐回调、DMA 生命周期、超时和错误契约。
8. 完成 Debug、Release 和硬件验证。

## 11. 外设能力矩阵

| 外设 | F407VET6 | F103C8T6 | 说明 |
| --- | --- | --- | --- |
| I2C | I2C1–3 | I2C1–2 | 7 位地址主机传输 |
| SPI | SPI1–3 | SPI1–2 | 主机模式，CS 归 Board/设备驱动 |
| ADC | ADC1–3 | ADC1–2 | 转换模式由 CubeMX 配置 |
| DAC | DAC CH1–2 | 不支持 | F1 不提供虚实现 |
| CAN | CAN1–2 | CAN1 | 仅经典 CAN |
| RTC | RTC | RTC | F1 无唤醒定时器，仅有每日闹钟 |
| Flash | 内部 Flash | 内部 Flash | 仅底层原语，不含参数策略 |
| USART/UART | USART1–6 | USART1–3 | UART HAL handle 统一映射 |

未列出的实例不会出现在公共枚举中。F1 的 DAC、I2C3、CAN2、ADC3、SPI3
和 USART4–6 不伪造实现。

## 12. 传输类外设契约

### I2C

- 只提供主机模式，公共地址为 7 位 `0x00..0x7F`，实现内部左移一位。
- 阻塞接口超时单位为毫秒；IT/DMA 传输没有内部超时。
- 一次只允许一个活动传输，一个实例只有一个回调所有者。
- 缓冲区在完成、错误或 abort 前必须保持有效。
- 错误通过 `Platform_I2C_GetLastError()` 返回 BERR/ARLO/AF/OVR/DMA/TIMEOUT
  等位，恢复使用显式 `Platform_I2C_Abort_IT()`。

### SPI

- 只提供主机模式，不操作片选。片选时序属于 Board 或具体设备驱动。
- 缓冲区大小统一按字节描述。16 位模式下，实现把字节数换算为 HAL 帧数，
  并要求长度为偶数、地址对齐。
- IT/DMA 缓冲区在完成、错误或 abort 前必须保持有效。
- `Platform_SPI_Abort()` 用于任务上下文同步恢复，
  `Platform_SPI_Abort_IT()` 用于中断完成路径。

### CAN

- 只支持经典 CAN，不扩展 CAN FD。
- 过滤器使用 32 位掩码/列表模式，所有标识符和掩码都是 CAN 标识符位，
  不是 HAL 原始寄存器值。掩码模式使用 `id/mask`，列表模式使用 `id/id2`；
  列表中的两个标识符共享 `id_type` 和 `remote`。
- `Platform_CAN_Transmit/Receive` 接收毫秒超时，调用方必须在任务上下文使用。
  实现不调用延时 API，只轮询 HAL mailbox/FIFO 状态。
- 中断接收事件携带完整帧，事件指针只在回调期间有效。
- bus-off 后由调用方显式 `Stop`、重新配置并 `Start`，Platform 不静默恢复。

## 13. 采样、时钟与存储契约

### ADC 与 DAC

- 单次、连续、扫描、通道顺序、触发源和 DMA 模式由 CubeMX 决定。
- `Platform_ADC_Read()` 只适用于单通道、单次转换配置。
- 多通道扫描必须使用 DMA；常规组中断只会在序列结束时报告最后一次转换。
- STM32F1 的独立 DMA 请求仅属于 ADC1，ADC2 的 DMA 启动请求返回
  `Platform_ERROR`。
- ADC DMA 的 `length` 是转换次数，不是字节数。
- DAC DMA 的 `length` 是样本数，样本对齐方式必须与传入的
  `Platform_DAC_Alignment` 一致。
- ADC/DAC 的 DMA 缓冲区在完成、错误或 stop 前必须保持有效。

### RTC

- 日期时间使用二进制字段，不向调用方暴露 BCD。
- 年份范围为 2000..2099，weekday 为 1..7。
- 跨系列闹钟契约是每日闹钟，只比较时、分、秒。
- F4 唤醒定时器使用 RTC 唤醒时钟和计数值，具体周期由所选时钟决定。
- RTC 时钟源、备份域访问和 `MX_RTC_Init()` 由 CubeMX/Core 负责。

### Flash

- 只提供读取、4 字节对齐写入和擦除范围原语。
- F4 按扇区、F1 按页擦除。调用方先用
  `Platform_FLASH_GetEraseUnitSize(PLATFORM_FLASH_1, address)` 查询粒度。
- 写入前必须由调用方完成擦除。
- 参数区布局、CRC 校验、默认值回退和磨损策略属于 System 参数存储层，
  不得下沉到 Platform Flash。
- Flash 操作使用 System 临界区保护忙状态，只允许任务上下文调用。

## 14. 构建与验证

F4 与 F1 的全实例 Platform 检查位于 `cmake/platform-check/`：

```powershell
cd cmake/platform-check
cmake --preset f4-debug
cmake --build --preset f4-debug
cmake --preset f1-debug
cmake --build --preset f1-debug
```

`f4-release` 和 `f1-release` 使用独立构建目录验证优化构建。正式目标通过
`PLATFORM_BUILD_I2C`、`PLATFORM_BUILD_SPI`、`PLATFORM_BUILD_ADC`、
`PLATFORM_BUILD_DAC`、`PLATFORM_BUILD_CAN`、`PLATFORM_BUILD_RTC` 和
`PLATFORM_BUILD_FLASH` 选择是否加入实现源文件。启用模块后还必须同时声明
目标实际拥有的实例宏，否则编译期检查会失败。

F103 新增 HAL 来自本机安装的 `STM32Cube FW_F1 V1.8.7` 固件包。实际板级
`.ioc` 尚未启用的实例只参与独立 Platform 检查，不应在正式目标中声明可用。

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
- 状态码和 HAL-free 常量。
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

未启用的实例不会进入公共枚举。当前板级通过 CMake 定义启用：

```text
PLATFORM_ENABLE_PWM_TIM1=1
PLATFORM_ENABLE_ENCODER_TIM2=1
PLATFORM_ENABLE_ENCODER_TIM3=1
PLATFORM_ENABLE_ENCODER_TIM4=1
PLATFORM_ENABLE_ENCODER_TIM5=1
PLATFORM_ENABLE_UART1=1
PLATFORM_ENABLE_UART2=1
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
5. 增加 HAL-free 公共头。
6. 在 F4 私有实现中把实例映射到 handle。
7. 补齐回调、DMA 生命周期、超时和错误契约。
8. 完成 Debug、Release 和硬件验证。

# 日志与遥测

## 1. 分层

日志和遥测使用两个独立通道：

```text
System/
  system_log_base   日志输出后端接口
  system_log        日志门面与格式化
  system_log_rtt    SEGGER RTT 具体后端

DeviceDrivers/
  Base/telemetry_base
  firewater_usart
  justfloat_usart
```

`System` 不依赖具体设备协议；DeviceDrivers 不依赖 App 或 RTOS。SEGGER RTT
原始源码位于 `Middlewares/Third_Party/SEGGER_RTT/`，只允许
`system_log_rtt.c` 直接包含其头文件。

## 2. System 日志

### 2.1 对象模型

- `sys_log_base_t` 是输出后端接口。
- `system_log_rtt_t` 首成员是 `sys_log_base_t`。
- `system_log.c` 维护唯一的日志门面和当前后端指针。
- 日志对象、通道和配置均为静态数据，不使用动态内存。

### 2.2 初始化

```c
#if defined(SYS_LOG_USE_RTT)
#include "system_log.h"
#include "system_log_rtt.h"

static system_log_rtt_t g_log_rtt;
#endif

#if defined(SYS_LOG_USE_RTT)
Sys_Log_RTT_Init(&g_log_rtt, "rtt", 0u);
Sys_Log_Init(&g_log_rtt.base);
Sys_Log_SetLevel(SYS_LOG_LEVEL_INFO);
SYS_LOG_INFO("BOOT", "system ready");
#endif
```

`main()` 已经在调度器启动前完成该初始化。日志内部锁使用短 PRIMASK 临界区，
因此不依赖 FreeRTOS 调度器是否已经启动。

### 2.3 使用规则

```c
SYS_LOG_ERROR("MOTOR", "left motor disabled: %u", id);
SYS_LOG_WARN("IMU", "stale yaw, age=%lu ms", (unsigned long)age_ms);
SYS_LOG_INFO("CTRL", "entered closed-loop mode");
```

- 普通日志只允许在任务上下文调用。
- 日志不阻塞。RTT 缓冲不足或另一个任务正在格式化时直接丢弃。
- 正常日志不要高频输出浮点数；高频浮点数据使用 `JustFloat`。
- `Sys_Log_EmergencyRaw()` 不格式化、不加锁，只用于 Fault 等最后手段。
- `Sys_Log_GetDropBytes()` 和 `Sys_Log_GetContentionCount()` 用于诊断日志链路。

### 2.4 RTT 配置

项目配置位于
`Middlewares/Third_Party/SEGGER_RTT/Config/SEGGER_RTT_Conf.h`：

- `BUFFER_SIZE_UP = 1024`
- 只启用 1 个上行通道和 1 个下行通道
- `SEGGER_RTT_MODE_NO_BLOCK_SKIP`
- 使用 C 实现，不编译 RTT 汇编文件

调试器必须能够发现 ELF 中的 `_SEGGER_RTT` 符号。当前 VS Code 配置使用
OpenOCD；OpenOCD 需要额外执行 RTT setup/start 并启动 RTT server。若使用
J-Link RTT Viewer，应改用 J-Link GDB Server，而不是当前的 OpenOCD 服务类型。

## 3. 遥测设备

### 3.1 Telemetry Base

`telemetry_sample_t` 固定最多 16 个通道，包含：

- `value[]`：通道值
- `valid_mask`：有效通道位掩码
- `time_ms`：采样时间戳

`Dev_Telemetry_Base_Send()` 会检查掩码和非有限值。`send` 约定为：

- 非阻塞
- 完整帧接受或整帧丢弃
- 返回 `OK`、`BUSY` 或 `ERROR`

### 3.2 JustFloat

`justfloat_usart_t` 输出：

```text
float ch0
float ch1
...
float chN
00 00 80 7F
```

所有 float 和帧尾均为小端序。无效通道发送 `NAN`。每帧固定为
`channel_count * 4 + 4` 字节。这是高频波形遥测的默认选择。

### 3.3 FireWater

`firewater_usart_t` 输出带通道名的文本：

```text
target0:1.230,current0:1.180,yaw:45.200\n
```

实现不调用 `snprintf` 格式化浮点数，而是使用本地定点转换，避免引入软件双精度
格式化开销。`precision` 范围为 0..6。

FireWater 比 JustFloat 占用更多 CPU 和串口带宽，适合低频调试；高频数据应使用
JustFloat。

### 3.4 USART 和 DMA

两个实现都通过 `usart_base_t` 发送 DMA，不直接依赖 HAL：

```c
static usart_port_t g_usart2;
static justfloat_usart_t g_telemetry;

Dev_USART_Port_Init(&g_usart2, "usart2", PLATFORM_USART_2);
Dev_JustFloat_USART_Init(
  &g_telemetry,
  "justfloat2",
  &g_usart2.usart_base,
  12u
);
```

约束：

- TX 缓冲由具体设备对象持有，DMA 完成前不能被下一帧覆盖。
- USART2 当前只启用 DMA 和 DMA IRQ，USART2 NVIC 未启用。
- 只发送遥测时不需要 USART2 NVIC；需要接收命令和错误回调时必须启用。
- 当前 `usart_base_t` 每个实例只有一套回调。`firewater_usart` 或
  `justfloat_usart` 初始化后会接管该 USART 的回调槽，不能和另一个独立回调用户
  同时使用。
- 发送缓冲必须位于普通 SRAM，不能放 CCMRAM，因为 F4 DMA 无法访问 CCMRAM。

### 3.5 速率和波特率

USART2 当前配置仍为 9600 baud。使用遥测前应在 CubeMX 中改为 `921600` 或
`1 Mbps`，并同步修改上位机配置。

参考带宽：

- JustFloat，16 通道，50 Hz：约 3.4 KB/s。
- FireWater，16 通道，3 位小数，50 Hz：通常约 15 KB/s 以上。

### 3.6 调度建议

- ControlTask 只生成或更新遥测快照。
- MonitorTask 以 20..50 Hz 调用 `Dev_Telemetry_Base_Send()`。
- 不要在 ControlTask 中格式化文本或等待 DMA。
- 不要为了遥测调用 `Dev_Encoder_Base_GetDelta()`，该接口会消费控制周期所需的增量。
- 串口忙时接受整帧丢弃，通过 `telemetry_status_t` 暴露丢帧和错误计数。

## 4. 构建开关

CMake 选项：

```cmake
SYS_LOG_USE_RTT=ON
```

关闭 RTT 后端：

```bash
cmake --preset Debug -DSYS_LOG_USE_RTT=OFF
cmake --build --preset Debug
```

关闭后不编译 `system_log_rtt.c` 和 SEGGER RTT 源码，`main()` 也不初始化 RTT。

## 5. 当前边界

已完成：

- System 日志 base、门面、RTT 后端
- RTT 非阻塞输出与丢包统计
- Telemetry base
- FireWater / JustFloat USART DMA 协议设备
- Debug、Release 和无 RTT 构建

尚未完成：

- 在 `rtos_shared` 中定义具体遥测对象并选择协议
- 从电机、IMU、底盘生成固定通道快照
- 在 MonitorTask 中周期性发送
- 将 USART2 波特率提高到遥测速率
- 在 OpenOCD 或 J-Link 工具链中实际验证主机端接收

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
  Base/joycon_base
  firewater_uart
  justfloat_uart
  joycon_uart
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

调试器必须能够发现 ELF 中的 `_SEGGER_RTT` 符号。仓库提供 OpenOCD 主机接收
脚本，烧录带 RTT 的固件后运行：

```powershell
$env:OPENOCD_EXE = "D:\path\to\openocd.exe"
.\tools\rtt\Receive-Rtt.ps1 -Board f103c8t6
```

脚本在目标 RAM 中查找 `SEGGER RTT` 控制块，启动本地 RTT server，并将日志
显示到当前终端。脚本只短暂 halt 目标完成 RTT 初始化，随后恢复运行。
若使用 J-Link RTT Viewer，应改用 J-Link GDB Server。

## 3. 遥测设备

### 3.1 Telemetry Base

`telemetry_sample_t` 默认最多 16 个通道，可在板级构建中通过
`TELEMETRY_CHANNEL_CAPACITY` 覆盖到最大 32。速通任务一使用 32 通道。每个样本包含：

- `value[]`：通道值
- `valid_mask`：有效通道位掩码
- `time_ms`：采样时间戳

`Dev_Telemetry_Base_Send()` 会检查掩码和非有限值。`send` 约定为：

- 非阻塞
- 完整帧接受或整帧丢弃
- 返回 `OK`、`BUSY` 或 `ERROR`

### 3.2 JustFloat

`justfloat_uart_t` 输出：

```text
float ch0
float ch1
...
float chN
00 00 80 7F
```

所有 float 和帧尾均为小端序。无效通道发送 `NAN`。每帧固定为
`channel_count * 4 + 4` 字节。这是高频波形遥测的默认选择。

### 3.3 JoyCon

`joycon_uart_t` 从普通透明传输蓝牙串口接收 ASCII 控制报文：

```text
[j,lx,ly,rx,ry]
```

- `j` 是 JoyCon 固定标识，大小写兼容。
- `lx`、`ly`、`rx`、`ry` 的原始范围均为 `[-100,100]`。
- 不包含校验字段，`]` 的结束方括号是完整帧边界。
- `\r\n` 可选，帧外空白不参与解析。
- 设备层输出归一化到 `[-1,1]` 的结构化快照，不解释电机或底盘单位。
- UART 中断只写环形队列；状态机、数值解析和快照发布在 UARTTask 中完成。

F407 的 JustFloat 使用 USART2 TX，JoyCon 使用 USART3 RX，两者属于同一透明传输
蓝牙模块的不同方向，但各自拥有独立的 `uart_base_t` 回调所有者。

### 3.4 FireWater

`firewater_uart_t` 输出带通道名的文本：

```text
target0:1.230,current0:1.180,yaw:45.200\n
```

实现不调用 `snprintf` 格式化浮点数，而是使用本地定点转换，避免引入软件双精度
格式化开销。`precision` 范围为 0..6。

FireWater 比 JustFloat 占用更多 CPU 和串口带宽，适合低频调试；高频数据应使用
JustFloat。

### 3.5 UART 和 DMA

两个实现都通过 `uart_base_t` 发送 DMA，不直接依赖 HAL：

```c
static uart_port_t g_uart_h;
static justfloat_uart_t g_telemetry;

Dev_UART_Port_Init(&g_uart_h, "uart2", PLATFORM_UART_2);
Dev_JustFloat_UART_Init(
  &g_telemetry,
  "justfloat2",
  &g_uart_h.uart_base,
  16u
);
```

约束：

- TX 缓冲由具体设备对象持有，DMA 完成前不能被下一帧覆盖。
- F103C8T6 的 USART2 当前同时启用 DMA、DMA IRQ 和 USART2 NVIC，支持命令接收
  与 DMA 发送。
- F407VET6 的 JustFloat 使用 USART2 TX DMA；JoyCon 使用 USART3 RX 中断，并启用
  对应 NVIC 和 `USART3_IRQHandler`。
- 当前 `uart_base_t` 每个实例只有一套回调。`firewater_uart` 或
  `justfloat_uart` 初始化后会接管该 UART 的回调槽，不能和另一个独立回调用户
  同时使用。
- 发送缓冲必须位于普通 SRAM，不能放 CCMRAM，因为 F4 DMA 无法访问 CCMRAM。

### 3.6 速率和波特率

USART2 当前配置按板区分：

- F103C8T6：`115200 baud`。
- F407VET6：USART2 JustFloat 和 USART3 JoyCon 均为 `115200 baud`；USART1
  WHT101 为 `9600 baud`。

提高遥测速率前应在对应板卡的 CubeMX 中改为 `921600` 或 `1 Mbps`，并同步修改
上位机配置。

参考带宽：

- JustFloat，16 通道，50 Hz：约 3.4 KB/s。
- FireWater，16 通道，3 位小数，50 Hz：通常约 15 KB/s 以上。

### 3.7 调度建议

- ControlTask 只生成或更新遥测快照。
- MonitorTask 以 20..50 Hz 调用 `Dev_Telemetry_Base_Send()`。
- 不要在 ControlTask 中格式化文本或等待 DMA。
- 不要为了遥测调用 `Dev_Encoder_Base_GetDelta()`，该接口会消费控制周期所需的增量。
- 串口忙时接受整帧丢弃，通过 `telemetry_status_t` 暴露丢帧和错误计数。

### 3.8 F103 速通任务一 USART1

`Boards/f103c8t6-fast-channel-task-1/` 使用 USART1：

- TX DMA1 Channel 4，115200 8N1，32 通道 JustFloat，50 Hz。
- RX 单字节中断，进入 256 字节环形缓冲。
- 板级 Link 是物理 UART 的唯一回调所有者。
- `justfloat_uart_t` 使用 Link 提供的虚拟 `uart_base_t`，保持共享回调契约不变。
- RX 支持 `SET <参数> <值>` 和 `RESET EKF`，TX 不输出 ASCII 应答。

32 通道每帧 132 字节，50 Hz 时约 6600 B/s，占 115200 8N1 线路容量约 57%。
命令结果显示在 JustFloat 第 31 通道。

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
- FireWater / JustFloat UART DMA 协议设备
- Debug、Release 和无 RTT 构建
- F103 板上 RTT 控制块发现、启动日志、并发运行日志与主机端接收
- F407 四轮目标/实测轮速、整车命令、yaw 和 gyro-Z 的 16 通道 JustFloat 快照
- F407 JoyCon 蓝牙命令接收、超时制动和控制心跳急停

尚未完成：

- 在真实蓝牙模块上验证 115200 全双工链路和主机端 JustFloat 解析
- 验证 16 通道 JustFloat 在 50 Hz 下的实际丢帧率
- 在 J-Link 工具链中实际验证主机端接收

# STM32F103C8T6 速通赛道任务一

本板级工程面向 `26电控组速通赛道.md` 的任务一，提供 MPU6050 采集、上电自动
六面标定、Flash 校准记录、七状态 EKF 接口、FreeRTOS 周期任务和 USART1
JustFloat 实时波形输出。EKF 的核心预测与校正暂留空，由使用者实现。

## 工程结构

| 路径 | 内容 |
| --- | --- |
| `f103c8t6-fast-channel-task-1.ioc` | CubeMX 配置 |
| `Core/` | CubeMX 生成代码与 USER CODE 板级接入 |
| `App/` | 板级配置、IMU 标定会话、UART 命令和遥测 |
| `RTOS/` | ImuEkfTask 与 VofaTask |
| `cmake/` | ARM GCC、HAL、FreeRTOS 和生成源文件列表 |
| `Drivers/` | STM32F1 HAL 与 CMSIS |
| `Middlewares/` | FreeRTOS CM3 与 CMSIS-RTOS2 |
| `STM32F103xx_FLASH.ld` | 62 KB 代码区 + 2 KB 校准记录区 |

共享的 Platform F1、DeviceDrivers、Filter 和 System 直接从仓库根目录接入，
本工程不复制共享层。硬件连接见 [hardware-map.md](hardware-map.md)。

App 内部按职责拆分为：

- `app_fast_channel`：板级装配和任务入口。
- `app_imu_session`：IMU 管线编排和一致快照。
- `app_calibration_flow`：初始姿态、陀螺零偏和六面标定状态机。
- `app_calibration_record`：Flash 校准记录和活动配置。
- `app_attitude_reference`：直接姿态和陀螺积分参考。
- `app_status_indicator`：蜂鸣器和 LED 提示。
- `app_fast_channel_telemetry`：32 通道遥测映射。

## 运行流程

1. 上电后读取 Flash 中序号最新且 CRC 正确的校准记录；没有记录时使用零偏 0、
   加速度比例 1。
2. MPU6050 进入 100 Hz 采样。若首次姿态为 `-Z` 且连续稳定 3 秒，则跳过标定，
   直接进入主程序。
3. 否则采集静止陀螺零偏，然后按任意顺序识别六个面。每个新面需要稳定 1 秒并
   采集 100 点，寻找新面的窗口为 30 秒。
4. 每个面成功后立即追加 Flash 记录。前五面写入成功时短鸣两次；第六面不双鸣。
5. 标定完成或任一面超时后进入主程序，并使用最近一次 Flash 验证成功的校准值。
6. 进入主程序时有源蜂鸣器长鸣一次。EKF 更新开始，USART1 以 50 Hz 输出
   32 通道 JustFloat。

Flash 写入失败时不双鸣，继续寻找下一面；最终仍进入主程序并使用上一版有效记录。

## 蜂鸣器与 LED

- 有源蜂鸣器为三引脚 3.3 V 模块，`I/O` 接 PA0，低电平触发。
- 短鸣默认 80 ms，两次间隔 120 ms。
- 主程序开始长鸣默认 600 ms。
- PC13 LED 低电平点亮，标定时快速闪烁，主程序低速闪烁。
- PA0 为 WKUP 引脚，建议模块输入端保留 3.3 V 上拉，避免复位期间误鸣。

## EKF 边界

`Filter/Src/filter_ekf.c` 中的 `filter_ekf_coreupdate()` 是使用者实现位置。
当前壳层完成以下工作：

- 七状态对象、协方差静态内存和初始化/复位
- `deg/s` 到 `rad/s` 的框架边界转换
- 姿态弧度到框架 `deg` 的发布转换
- 实际采样时间差计算与夹紧
- 默认参数、在线参数校验和帧边界原子应用
- 单位四元数、零姿态和 `PLACEHOLDER` 状态发布；占位姿态不置有效位

核心未实现前，遥测中的 EKF 姿态不是有效估计，`valid` 为 false，不得用于控制。

## 在线调参

USART1 RX 接收 ASCII 命令，TX 只发送 JustFloat 二进制帧，不插入文本应答。
支持：

```text
SET Q_ANGLE 0.0001
SET Q_BIAS 0.000001
SET R_ACCEL 0.1
SET P0_ANGLE 1.0
SET P0_BIAS 0.01
SET ACCEL_MIN 7.8
SET ACCEL_MAX 11.8
SET GYRO_LIMIT 17.45
SET BIAS_LIMIT 1.0
SET DT_MIN 0.001
SET DT_MAX 0.05
RESET EKF
```

参数掉电恢复 `App/Inc/app_fast_channel_config.h` 中的宏默认值。命令结果通过
JustFloat 第 31 通道和 RTT 日志查看。

## 遥测 schema

| 通道 | 内容 | 单位 |
| --- | --- | --- |
| 0–2 | 原始加速度 XYZ | m/s² |
| 3–5 | 标定后加速度 XYZ | m/s² |
| 6–8 | 原始陀螺 XYZ | rad/s |
| 9–11 | 标定后陀螺 XYZ | rad/s |
| 12–13 | 加速度直接解算 roll、pitch | rad |
| 14–16 | 陀螺积分 roll、pitch、yaw | rad |
| 17–19 | EKF roll、pitch、yaw | rad |
| 20–23 | EKF 四元数 w、x、y、z | 无 |
| 24–26 | EKF 陀螺零偏 XYZ | rad/s |
| 27 | 实际融合周期 | s |
| 28 | 标定状态码 | 枚举 |
| 29 | 六面进度掩码 | 位掩码 |
| 30 | EKF 状态，`100` 表示占位 | 枚举 |
| 31 | 最近命令状态 | 枚举 |

32 通道、50 Hz、115200 8N1 每帧 132 字节，线路占用约 57%。

## 构建

依赖 CMake 3.22+、Ninja 和 `arm-none-eabi-gcc`：

```powershell
cd Boards/f103c8t6-fast-channel-task-1
cmake --preset Debug
cmake --build --preset Debug
```

Release：

```powershell
cmake --preset Release
cmake --build --preset Release
```

主要产物：

```text
build/Debug/f103c8t6-fast-channel-task-1.elf
build/Release/f103c8t6-fast-channel-task-1.elf
```

CubeMX 6.18.1 重新生成时会报告 FreeRTOS/SysTick 时基警告，并附带若干只读
可选参数提示；生成后的 USART1 TX DMA、I2C1 和系统时钟代码已由构建验证。

## 验证状态

- Debug 和 Release 固件完成编译与链接。
- 新增 Filter 与 Flash 记录原生契约测试通过。
- F1/F4 共享层 `-Werror` 编译检查通过。
实板已验证：

- ST-Link 通过 SWD 识别 Cortex-M3，目标电压约 3.22 V。
- Debug ELF 烧录、校验和复位成功。
- 72 MHz 启动、RTT、ImuEkfTask 与 VofaTask 正常运行。
- COM4 115200 持续输出 32 通道 JustFloat，实际 `dt` 约 0.010 s。
- `SET R_ACCEL 0.15` 和 `RESET EKF` 均返回命令接受状态。
- 陀螺零偏在约 1 s 内完成；`+Z` 面自动识别、采样并写入 Flash。
- 复位后 `flash=1`，证明校准记录跨复位保持。
- 不继续翻面时进入主程序状态。

仍需人工完成：

- 六面完整翻转、初始 `-Z` 三秒跳过和恢复姿态安装方向。
- 有源蜂鸣器当前未接入，实际音量和双鸣/长鸣时序未听测。
- 实芯片报告 128 KiB Flash，而 C8T6 目标按 64 KiB 保守链接；
  校准记录固定在 `0x0800F800`，不依赖芯片实际容量。

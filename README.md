# layered-embedded-c

面向 STM32F407 和 STM32F103 的分层嵌入式 C 工程：用静态设备模型、base + 具体实现、
注入式控制律与运动策略，把 HAL、设备驱动、控制、运动解算和上层任务分开。

## 当前状态

已完成并通过工具链构建的是**基础设施层**：

- **Platform**：GPIO / PWM / Encoder / USART / EXTI / CRC / RNG / IWDG / WWDG 的 HAL-free 接口，F4 实现已完整接入；F103 目标当前接入 GPIO / EXTI / CRC / IWDG / WWDG，PWM / Encoder / USART 按板级实例继续接线。两层都遵循编译期实例开关、CubeMX 初始化边界、UART 错误快照与 IT/DMA 缓冲区契约。
- **DeviceDrivers**：LED（GPIO / PWM）、电机（TB6612 + 闭环 base）、编码器（TIM + delta / resync）、USART（base + port）、IMU（WHT101 UART）、遥测（base + FireWater / JustFloat USART）。
- **Controller**：轮级 SISO 控制契约、线性控制律、compose。
- **Policy**：独立 MIMO 运动策略层，当前实现 omni 运动学。
- **Chassis**：底盘生命周期基类 + omni 底盘。
- **Filter**：样本、base、低通、透传、chain、distributor。
- **System**：数学、检查、限幅、符号、延时、临界区、Fault 诊断、看门狗、日志 base 与 SEGGER RTT。
- **RTOS**：F407 目标已创建四个任务骨架，控制任务和串口任务体待实现；F103 当前保留 CubeMX 默认任务。
- **App**：预留空文件，业务逻辑待实现。
- **Boards**：`boards/f103c8t6/` 提供 Cortex-M3 / 72 MHz / CMSIS-RTOS2 / RTT 日志构建目标，硬件映射见 [F103 板级说明](boards/f103c8t6/README.md)。

尚未完成或暂缓：

- `controller_pid_t`（PID）
- App / RTOS 接线
- Board 层（四轮编号、引脚拓扑、STBY 分组、统一初始化、故障切电机）
- 参数标定
- Filter 单元测试、统一单调时间源、Kalman / MPU6050
- Platform 外设扩展（I2C / SPI / ADC / DAC / CAN / RTC / Flash）
- MCU 运行时与安全（剩余：复位原因、PVD/BOR、MPU、SWO/ITM、参数存储）

## 构建

根目录 F4 目标依赖：

- `arm-none-eabi-gcc`（Cortex-M4F，硬浮点）
- CMake ≥ 3.22
- Ninja

`arm-none-eabi-` 必须在 `PATH` 中，或在 CLion 工具链里配置。
工程默认启用 `-Wall -Wextra`；第三方 HAL/FreeRTOS 目标单独关闭其不拥有的
`unused-parameter` 告警。

```bash
cmake --preset Debug
cmake --build --preset Debug
```

产物位于 `build/Debug/`，包括 `layered-embedded-c.elf` 和 `layered-embedded-c.map`。

Release 构建：

```bash
cmake --preset Release
cmake --build --preset Release
```

F103 使用板级目录中的独立 presets：

```powershell
cd boards/f103c8t6
cmake --preset Debug
cmake --build --preset Debug
```

F103 产物位于 `boards/f103c8t6/build/Debug/`，包括 `f103c8t6.elf` 和
`f103c8t6.map`。完整命令和当前接入状态见
[F103 板级说明](boards/f103c8t6/README.md)。

## 烧录 / 调试

工程使用 ST-Link + OpenOCD 即可，例如：

```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "program build/Debug/layered-embedded-c.elf verify reset exit"
```

实际接口配置按你的调试器与板卡调整。

## 文档

- [架构说明](doc/architecture.md)
- [单位与量纲](doc/units.md)
- [编码约定](doc/conventions.md)
- [Platform 契约](doc/platform.md)
- [F103 硬件与接口规范](boards/f103c8t6/hardware-map.md)
- [日志与遥测](doc/telemetry.md)
- [TODO](doc/todo.md)
- Doxygen：`doxygen doc/doxygen/Doxyfile`，或 `cmake --build build/Debug --target docs`

## 目录

| 目录 | 职责 |
| --- | --- |
| `Platform/` | HAL 封装，枚举身份 + 静态映射；F4 / F1 两套实现 |
| `DeviceDrivers/` | 设备抽象 base + 具体驱动 |
| `Controller/` | 轮级 SISO 控制律 |
| `Policy/` | 底盘级 MIMO 运动策略 |
| `Filter/` | 固定容量多通道滤波 |
| `System/` | 数学、检查、延时 |
| `RTOS/` | FreeRTOS 任务骨架 |
| `App/` | 业务逻辑（待实现） |
| `Core/` | CubeMX 生成代码 |
| `Drivers/`、`Middlewares/` | ST HAL / CMSIS、FreeRTOS |
| `doc/` | 文档 |
| `boards/` | 各 MCU 板级的 CubeMX、HAL、启动文件和链接脚本 |

## 设计要点

- **静态设备模型**：设备在编译期确定，上层只接触 base 接口。
- **base 首成员**：具体类型通过首成员嵌入 base，回调里强转回具体类型。
- **注入式算法**：电机注入 `controller_base_t`，底盘注入对应类型的 policy base。
- **无动态内存**：所有对象静态定义。
- **单位显式**：对外 SI 单位，设备层 duty，编码器 counts。
- **assert 管编程错误，状态枚举管运行时错误**。

## 许可

见 `LICENSE`。

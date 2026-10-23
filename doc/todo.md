# TODO

## 当前里程碑
**状态**：核心分层、F4/F1 Platform 全部支持外设、F103 基础目标、设备驱动、
控制/策略、Filter、System 基础服务、RTT 日志和两种 UART 遥测协议均已实现并通过构建。
- [x] F4 与 F103 的 Debug、Release、无 RTT 三种配置构建通过
- [x] 全局 `-Wall -Wextra` 无警告，且 `-Werror` 独立构建通过
- [x] SEGGER RTT 日志 base、门面、后端和启动初始化
- [x] FireWater / JustFloat 遥测设备与 UART DMA 发送
- [ ] Board 层与 App / RTOS 运行链路的完整接线
- [ ] 轮速 PID、参数标定和板上验证
- [ ] 遥测通道 schema、周期发送与 USART2 高速配置

## 命名与接口一致性
**目标**：公共类型、聚合标签、函数动作、对象句柄和并行模块词序使用同一套规则，
由 `doc/conventions.md` 冻结。
- [x] 公共 typedef 统一为小写蛇形 `_t`
- [x] 聚合标签统一为 `_s` / `_e` / `_u`，清除匿名公共聚合
- [x] 公共函数最后一段统一为驼峰动作，IT/DMA 等后缀不再拆成下划线
- [x] 对象句柄统一为 `<role>_h`，同类实例使用数组下标
- [x] 项目自有 UART 符号统一，CubeMX/HAL 的 USART 符号保留原名
- [x] Policy、RTOS、System 数学 API 和文件名统一词序
- [x] F4/F103 Debug、Release、`-Werror` 与全实例检查通过

## Platform · 统一契约重构
**目标**：F1/F4 只允许在实例数量、位宽、能力集合和 HAL 后端上不同；公共参数
校验、状态机、错误快照、回调所有权和异步生命周期保持一致。平行外设按
transfer / converter / timer / registry 分组，同组接口采用同一设计模式。

**规则冻结**
- [x] 新增 `doc/platform-contract.md`，冻结状态码、错误位、参数校验、timeout、
  回调、abort、缓冲区和跨系列能力规则
- [x] `platform_status_t` 增加参数错误和硬件不支持状态
- [x] 定义跨外设通用错误位，模块专用 HAL 错误使用独立高位
- [x] 明确公共参数错误不在 Debug/Release 间采用不同行为
- [x] 明确 `assert` 只用于内部不变量，不用于可返回错误的公共输入
- [ ] 所有错误快照统一为“操作开始时清零，记录最近一次公共操作，读取不清零”

**公共基础设施**
- [x] 提取公共错误快照辅助函数
- [ ] 提取公共实例校验和状态转换辅助函数
- [x] 回调注册统一返回 `platform_status_t`
- [x] 每个物理实例只允许注册一次；重复注册统一返回 `PLATFORM_STATUS_BUSY`
- [x] EXTI 取消特殊覆盖语义，与其他外设统一回调所有权
- [x] 增加原生 fake-backend watchdog/RTC/Flash 契约测试，覆盖能力、状态和错误差异
- [ ] 扩展 `cmake/platform-check`，验证两系列公共 API 签名和行为可编译

**第一批确定性修复**
- [x] IWDG 仅在 HAL 启动成功后设置 started 状态
- [x] WWDG 根据实际 PCLK1 返回可用最大超时，修正 F103 的 49 ms 声明
- [x] RTC 的非法实例、空指针和参数错误在 F1/F4 采用相同返回与错误位
- [x] `Platform_SPI_GetDataWidth` 改为 `status + out`，不再用 8-bit 表示失败
- [x] CAN 参数错误、状态错误和 HAL 错误统一写入 `GetLastError`
- [x] 消除同一公共错误条件同时 `assert` 和返回错误的混合实现

**分组重构**
- [x] watchdog 组：先完成公共状态机和双平台行为一致
- [x] watchdog 试点：公共逻辑位于 `Platform/Src/common/`，F1/F4 仅保留 backend
- [x] service 组：RTC、Flash 统一参数校验、错误快照和复位/擦除语义
- [x] transfer 组：UART、I2C、SPI、CAN 统一 typed event、回调所有权、
  参数错误和错误快照
- [x] transfer 组已完成 UART/I2C/SPI/CAN 的 common/backend 拆分
- [ ] transfer abort 能力差异仍需在能力查询和板上恢复流程中验证
- [x] converter 组：ADC、DAC 统一启动、停止、DMA、缓冲区和能力错误
- [x] timer 组：PWM、Encoder 统一 start/stop、计数器/比较值和非法实例处理
- [x] registry 组：GPIO、EXTI 公共逻辑统一；GPIO 保持明确的无状态快速路径
- [x] transfer 组已统一使用 typed event 结构
- [x] converter 组已统一使用 typed event 结构

**F1/F4 去重**
- [x] Public headers 只保留一份，能力差异通过配置、查询或 `PLATFORM_STATUS_UNSUPPORTED`
  表达，不改变普通函数签名
- [x] 公共参数校验、状态机、错误映射和回调分发迁入 `Platform/Src/common/`
- [x] F1/F4 目录只保留 handle 映射、HAL 调用、时钟和硬件能力差异
- [x] 已完成 watchdog、RTC、Flash、UART、I2C、SPI、CAN、ADC、DAC、
  PWM、Encoder、GPIO、EXTI、CRC、RNG 的 common/backend 拆分
- [ ] CMake 拆出 `platform_common` 与 `platform_f1/f4_backend` 目标
- [x] 每个分组成员完成后同时构建 F4/F103 Debug、Release 与 `-Werror`

**验收**
- [x] 同一输入通过同一组 fake-backend 契约测试验证状态和通用错误位
- [x] 同类外设的事件、abort、回调和错误快照语义一致
- [x] 公共输入在 Debug/Release 下不会因 `assert` 开关改变行为
- [x] Platform 公共头不泄漏 HAL 类型，公共实现不重复维护两份业务逻辑
- [x] App/RTOS 接线前完成 Platform 契约冻结和双平台编译/主机侧验收

## Platform
**架构**：Platform 层用枚举身份和静态映射函数把 HAL 类型封在实现文件内部，上层只接触 `Platform_*` 类型；公共状态、校验和策略逐步迁入 `Platform/Src/common/`，f1/f4 只保留 HAL backend。
- [x] 保留两个 `platform_tim.c`，作为 TIM 句柄/通道映射与基础定时器操作的落点
- [x] 将 HAL 状态映射从公共头移到 Platform 私有的 `platform_hal_internal.h`
- [x] 公共头文件只保留不依赖 HAL 类型的枚举、普通数据结构、回调和函数声明
- [x] 新增 `platform_config.h`，按编译期开关暴露当前可用实例
- [x] PWM 与 Encoder 拆分独立实例类型，PWM 通道不再接受 `ALL`
- [x] 新增显式 `Platform_PWM_StartAll/StopAll`，消除 `ALL` 写入 CCR4 的风险
- [x] Encoder 去掉冗余 channel 参数，固定使用两输入编码器模式
- [x] UART 发送接口改为 const 缓冲区，并增加 TX/RX abort 与错误快照 API
- [x] 文档补充 CubeMX 所有权、回调所有权和 DMA 缓冲区生命周期契约
- [x] 历史策略已由“Platform 统一契约重构”替代；公共参数错误统一返回状态
- [x] `Platform_UART_*` 各入口校验指针、长度和 UART 枚举
- [x] 修正 `Platform_UART_ClearBuffer`：改为中止接收并清除 UART 错误/状态标志
- [x] 修正 EXTI 回调传递引脚索引而非 GPIO Pin 的问题，并校验单 bit、范围和空 Pin
- [x] 修正 f1 UART callback 数组只有 3 项、公开 UART 枚举却有 6 项的问题
- [x] 为 `Platform_PWM_SetDuty` 增加 `duty <= max_duty` 限制并用更宽中间量避免乘法溢出
- [x] 为 `Platform_PWM_SetCompare` 增加 AUTORELOAD 夹紧
- [x] 保留 f1 平台
- [x] 为 STM32F103C8T6 建立独立 CMake 目标、F1 HAL、CM3 FreeRTOS、启动文件和链接脚本
- [ ] 完成 STM32F103C8T6 板上时钟、RTT、FreeRTOS、CRC、IWDG、WWDG 验证

## Platform · 外设扩展
**架构**：每个外设按“接口 / F4 实现 / F1 实现”拆分。`Platform/Inc/platform_xxx.h` 只定义枚举身份、状态码和 API，不包含 HAL 类型；`Platform/Src/stm32f4/platform_xxx.c` 负责 F4 HAL 映射与回调；F1 实现按需补，未实现时构建不选入。CubeMX 继续负责外设初始化，HAL 源文件单独注册。

**接口**
- [x] `platform_crc.h`：硬件 CRC 重置、32 位字累积与计算
- [x] `platform_rng.h`：硬件随机数读取与错误快照
- [x] `platform_iwdg.h`：按毫秒启动、刷新与复位标志
- [x] `platform_wwdg.h`：窗口 watchdog 启动、刷新与复位标志
- [x] `system_watchdog` 改为通过 Platform IWDG 启动、刷新和读取复位标志
- [x] `platform_i2c.h`：7 位地址主机阻塞/IT/DMA、内存传输、abort 与错误快照
- [x] `platform_spi.h`：主机 8/16 位传输、阻塞/IT/DMA 路径与 abort
- [x] `platform_adc.h`：轮询、IT、DMA 与单通道阻塞读取
- [x] `platform_dac.h`：F4 DAC CH1–2 输出、DMA 与 underrun 错误
- [x] `platform_can.h`：经典 CAN、32 位过滤器、收发超时与中断事件
- [x] `platform_rtc.h`：日期时间、每日闹钟；F4 额外支持唤醒定时器
- [x] `platform_flash.h`：读取、4 字节写入与跨系列擦除范围原语

**F4 实现**
- [x] `stm32f4/platform_crc.c`
- [x] `stm32f4/platform_rng.c`
- [x] `stm32f4/platform_iwdg.c`
- [x] `stm32f4/platform_wwdg.c`
- [ ] 板上验证 CRC 已知向量和连续累积结果
- [ ] 板上验证 RNG 启动、超时和错误状态
- [ ] 板上验证 IWDG 目标超时与复位标志
- [ ] 板上验证 WWDG 刷新窗口和复位标志
- [x] `stm32f4/platform_i2c.c`
- [x] `stm32f4/platform_spi.c`
- [x] `stm32f4/platform_adc.c`
- [x] `stm32f4/platform_dac.c`
- [x] `stm32f4/platform_can.c`
- [x] `stm32f4/platform_rtc.c`
- [x] `stm32f4/platform_flash.c`
- [x] `stm32f4/platform_uart.c` 补齐 USART3 / UART4 / UART5 / USART6 映射

**F1 与构建**
- [x] F103 目标接入共享 App / Controller / Policy / Filter / DeviceDrivers base / RTOS
- [x] F1 GPIO、EXTI、CRC、IWDG、WWDG 实现
- [x] F1 USART1–3、PWM TIM1、Encoder TIM2–4、I2C1–2、SPI1–2、ADC1–2、CAN1、RTC、Flash 实现
- [x] F103 目标补入 I2C、SPI、ADC、CAN、RTC、UART、TIM HAL 头文件与源文件
- [x] F4/F1 独立 Platform 全实例 Debug、Release 构建通过 `-Wall -Wextra`
- [ ] F103 CubeMX 配置实际 UART/TIM/I2C/SPI/ADC/CAN/RTC 实例、DMA 和 NVIC
- [ ] 每个实际启用的 F103 实例同步 CubeMX handle、初始化和板级 CMake 开关
- [ ] 板上验证新增 I2C / SPI / ADC / DAC / CAN / RTC / Flash 的电气与时序行为
- [ ] 评估 I2S / USB / SDIO / ETH / DCMI / FSMC，按项目需要启用

## DeviceDrivers · 编码器
**架构**：编码器对外只提供"自上次读取以来的增量"和重同步；基线、计数器回绕、首次读取对齐都在编码器内部按自身原理维护。`motor_base` 只调用 `get_delta`，不持有计数基线。
- [x] `encoder_base` 新增 `get_delta`，返回自上次读取以来的有符号增量
- [x] `encoder_base` 新增重同步动词，仅重对速度基线并保留累计位置
- [x] 将基线维护、计数器回绕处理、首次读取对齐收进编码器实现内部
- [x] `encoder_tim` 按上述接口补齐
- [x] 明确最大控制周期与计数速度：增加 `max_counts_per_period` 与溢出标志
- [x] 处理 `position += delta` 的长期溢出风险：`position` 改为 `int64_t`
- [x] `Dev_Encoder_TIM_Init` 返回启动状态，替代启动失败时直接 `assert(0)`
- [x] 修复 `Resync` 会改动累计位置的问题，改为只重对基线

## DeviceDrivers · 电机
**架构**：具体驱动配芯片并绑虚表，并在具体电机初始化时为内嵌 `motor_base` 注入 `law/enc`。`motor_base` 是闭环电机：目标速度接口存物理单位目标，`update` 按"读增量 → 控制律 → 限幅 → 执行器"运行；`reset` 复位控制律并重同步编码器；`enable` 内部调用 `reset`。具体驱动只认占空比。
**当前状态**：编码器反馈与控制律注入链路已完成；现有线性控制律忽略 `measure`，
实际执行仍是开环映射，真实轮速闭环等待 PID。
- [x] `motor_base` 新增 `controller_base_t *law` 与 `encoder_base_t *enc` 注入
- [x] `Dev_Motor_TB6612_Init` 接收 `controller_base_t *controller` 与 `encoder_base_t *encoder`，并传给 `Dev_Motor_Base_Init`
- [x] `motor_base` 新增 `update`：读取编码器增量、调用控制律、限幅、下发执行器
- [x] `motor_base` 新增 `reset`：复位控制律并重同步编码器基线
- [x] `motor_base` 的 `enable` 内部调用 `reset`
- [x] `motor_base` 的目标速度接口接收物理单位（m/s），`update` 将编码器增量换算为当前速度
- [x] `motor_base` 的 `max_speed` 更名为 `max_duty`
- [x] `motor_ops_t` 的 `set_speed` 更名为 `set_power`，参数语义为占空比
- [x] `motor_tb6612` 同步更新为 `set_power`
- [x] `motor_tb6612` 对 `power` 做有限值检查和 `[-max_duty, +max_duty]` 限幅
- [x] `motor_tb6612` 在方向切换前先撤销 PWM，再更新 IN1/IN2
- [x] `motor_tb6612` 的 `brake` 使用满占空比形成短制动
- [x] `Dev_Motor_TB6612_Init` 返回 `platform_status_t` 并处理 PWM 启动失败
- [x] 增加 `Dev_Motor_Base_Brake` 公共入口，使已经实现的 TB6612 短制动能力可达
- [x] 具体驱动保持 base 首成员 + 静态 ops 表
- [ ] 用 PID 替换线性开环映射，完成真实轮速闭环

## DeviceDrivers · 控制器
**目标架构**：`controller_base_t` 是轮级 SISO 控制接口，明确 target、measure、dt、输出单位/范围和错误状态；Policy 负责 MIMO 运动解算，Controller 只负责单轮闭环。所有具体实现内嵌 base 并绑定静态 ops 表。
- [x] 新增 `controller_base_t`，ops 含 `update(target, measure, dt, out)` 与 `reset`
- [x] 重构 `controller_base_t`：加入控制周期 dt、输出指针和错误/饱和状态，明确 input/output 单位
- [ ] 新增 `controller_pid_t`，基于新 Controller 契约实现，不复用即将废弃的 `PID_TypeDef`
- [x] 新增线性控制律，`duty = k * target + b`，`k = 1, b = 0` 时为目标透传
- [x] 实现 `controller_compose_t`，参考 Filter Base/Chain 的组合方式
- [x] 所有具体控制器采用 base 首成员 + 静态 ops 表

## Filter
**架构**：滤波器使用固定容量 `filter_sample_t` 承载多路输入输出，包含 `value[16]`、`valid_mask` 和 `time_ms`。`filter_base_t` 统一提供 `update(input, output)` 与 `reset`，调用前把完整输入复制到输出；具体滤波器覆盖或增加自己产生的通道。具体类型采用 base 首成员 + 静态 ops 表。`filter_chain_t` 负责串联，`filter_distributor_t` 负责多路分支与通道映射，整个 Filter 层不使用动态内存。
- [x] 新增 `filter_sample_t` 固定 16 通道样本、有效位掩码和统一时间戳
- [x] 新增 `filter_base_t`，ops 含 `update(input, output)` 与 `reset`
- [x] `Filter_Base_Update` 执行全量输入到输出复制，并检查输入输出别名、通道掩码范围和时间戳不变
- [x] 新增 `filter_lowpass_t`，内嵌 base 并采用静态 ops 表
- [x] 低通按通道保存上次输出、上次时间和时间常数，支持多通道 `valid_mask`
- [x] 低通首样本直接初始化，后续按 `dt / (time_constant + dt)` 计算一阶低通
- [x] 低通支持 reset、逐通道时间常数配置，并拒绝未初始化或非有限输入
- [x] 低通使用 `system_math.h` 的 `SYS_ISFINITE` 和 `SYS_CLAMP`
- [x] 新增 `filter_passthrough_t`，使用 base 全量复制语义直接透传原始通道
- [x] 新增 `filter_chain_t`，固定最多 8 级，内嵌 base 并采用静态 ops 表
- [x] Chain 使用两个临时样本交替传递，支持级联 reset、重复 stage 和自引用检查
- [x] 新增 `filter_distributor_t`，固定最多 8 条路由，每条路由最多 4 输入、4 输出
- [x] Distributor 支持全局到局部通道映射、部分有效输入转发和局部输出到全局通道合并
- [x] Distributor 校验重复滤波器、重复通道、非法索引和跨路由输出冲突
- [x] 保留 Filter Base 的输入预复制语义，Distributor 允许覆盖预复制通道，修复同通道路由断言冲突
- [x] 将 Filter base、Lowpass、Chain、Distributor 加入 `CMakeLists.txt` 和 include 路径
- [x] 为 `FILTER_CHANNEL_CAPACITY` 增加编译期上限检查，避免掩码移位超过 32 位
- [ ] 为 Filter Base、Lowpass、Chain、Distributor 增加可执行的单元测试和组合测试
- [ ] 新增 `filter_kalman_t` 具体实现，基于样本有效位支持预测与校正
- [x] 定义统一 IMU 九通道枚举，覆盖姿态、gyro XYZ 与 accel XYZ
- [x] MPU6050 原始六轴提交给已注册滤波器
- [ ] 为三轴传感器配置 Kalman/低通 Chain 或 Distributor 组合，并验证输出不冲突
- [x] 确认 WHT101 模块内部完成卡尔曼滤波、校准和姿态解算，外部注册 passthrough
- [ ] 为 Filter 的其他使用者提供统一单调时间源，支持可变采样周期
- [x] WHT101 显式注册 passthrough，统一走 IMU 滤波管线

## Policy
**架构**：Policy 是独立的 MIMO 运动解算层，与 Controller 平级。按底盘种类拆分接口，每种底盘只接受对应类型的 Policy；具体 Policy 内嵌 base 并绑定静态 ops 表。
- [x] 新增 `policy_omni_base_t`，ops 含 `solve(v, dir, yaw, omega, out[4])`
- [x] 新增 `policy_omni_kinematics_t`，内嵌 base，持有安装角、轮距、方向符号

## DeviceDrivers · 底盘
**架构**：`chassis_base_t` 只承载生命周期（update/stop/estop）；具体底盘内嵌 base，持有自己的执行器、IMU、Policy 和命令。底盘负责解算与下发目标，闭环仍在各电机内部。
- [x] 新增 `chassis_base_t`
- [x] 新增 `chassis_omni_t`，持有 `motor_base_t *wheel[4]`、`imu_base_t *imu`、`policy_omni_base_t *policy`
- [x] `chassis_omni_t` 提供 `set_motion(v, dir, omega)`
- [x] `chassis_omni_t` 提供 `update`：读取 yaw、调用 policy 解算、向四个电机下发目标
- [x] `chassis_omni_t` 根据 yaw 有效位和时间戳拒绝过期数据并锁存制动
- [x] `Dev_Motor_Base_Brake` 进入保持制动状态，恢复控制必须显式重新 Enable
- [x] `chassis_omni_t` 提供 `stop`：目标清零 + Brake
- [x] `chassis_omni_t` 提供 `estop`：向四个电机传播 Disable

## DeviceDrivers · 串口与 IMU
**架构**：`imu_base_t` 负责原始帧合并、已注册滤波器的调度、滤波结果发布、逐通道时间戳和一致快照。WHT101 通过 UART 采集并提交 yaw/gyro-z；MPU6050 通过 Platform I2C 采集六轴、完成陀螺零偏标定和轴映射后提交 accel/gyro。具体驱动不内置软件滤波，WHT101 显式注册 passthrough，MPU6050 可注册 lowpass、chain 或后续 Kalman。
**状态**：统一管线、WHT101 迁移、MPU6050 I2C 驱动和主机侧测试已完成；
App/RTOS 仍需把 IMU 解析和采样接入任务。
- [x] `uart_base` 补齐 `Dev_UART_ReceiveIT`、`Dev_UART_TransmitIT`、`Dev_UART_ReceiveDMA`、`Dev_UART_TransmitDMA`、`Dev_UART_RegisterCallback` 的转发实现
- [x] 重构 `uart_base` 使用独立的 `dev_uart_status_t`，不向设备层返回 Platform 类型
- [x] 新增 `uart_port_t` 适配 `Platform_UART_*`，并在 Platform 回调与 `uart_base` 回调之间桥接
- [x] `uart_pingpong` 已归档并删除，不再重写或恢复
- [x] 删除空的 `uart_controller`；协议明确后再新建具体驱动
- [x] `imu_base_t` 增加 accel XYZ、完整样本、raw/filtered 快照和统一滤波调度
- [x] 具体 IMU 通过 valid_mask 表示不支持通道，不再依赖空 getter 断言
- [x] 将 WHT101 置零和输出速率配置改为由 `process(now_ms)` 推进的非阻塞状态机
- [x] 为各 IMU 通道提供独立更新时间戳，明确不同异步帧的新鲜度判断
- [x] `imu_base_t` 增加 `start`、`process` ops 及对应转发入口
- [x] WHT101 仅依赖 `uart_base_t`，不再包含或引用 Platform UART 类型
- [x] WHT101 使用单字节中断接收、256 字节环形队列、溢出统计和自动重新挂起
- [x] 建立 USART1 中断 -> 环形队列 -> `process()` -> `FeedByte` 的输入路径
- [x] WHT101 提交模块内部处理后的 yaw 和 gyro-z，并通过 passthrough 发布
- [x] 增加 UART 错误回调和 ORE/FE/噪声错误后的解析重同步与接收重挂
- [x] 队列溢出和 UART 错误通过任务侧重同步标志复位解析状态，避免 ISR 修改解析器
- [x] 通过 base 暴露启动状态、有效通道、样本数和累计错误数
- [x] `process(now_ms)` 引入统一时间参数，状态暴露 `has_timestamp` 和 `last_update_ms`
- [x] 非阻塞配置请求通过 `is_busy` 和 `last_request_failed` 暴露执行结果
- [x] 上层根据时间戳实现数据过期判断和安全停机
- [ ] 高速输出时评估 F4 循环 DMA 或 DMA double-buffer 接收路径
- [ ] UART 驱动稳定后，另行实现独立的 HWT101 I2C 驱动
- [x] 新增 `mpu6050_i2c_t`，支持 WHO_AM_I、复位、DLPF、量程和采样率配置回读
- [x] MPU6050 使用 14 字节突发读取并输出 m/s²、deg/s 和温度
- [x] MPU6050 支持静止陀螺零偏标定、加速度模长检查和三轴映射
- [x] MPU6050 与 F103/F407 Platform I2C API 共用同一实现
- [x] 新增 IMU 管线与 MPU6050 假 I2C 主机测试
- [ ] 在真实 F103/F407 上启用 I2C 实例并完成板上采样验证
- [ ] 实现低通/互补或 Kalman 姿态滤波器并输出 yaw/roll/pitch

## DeviceDrivers · LED
**架构**：LED base 保留调光能力；GPIO 实现只负责开关，调光由独立 PWM LED 具体实现承担。
- [x] `Dev_LED_GPIO_Init` 显式写入 inactive level，避免软件状态为 OFF 时硬件仍是未定义电平
- [x] 新增独立 PWM LED 具体实现，承载 `set_brightness`

## DeviceDrivers · 遥测
**架构**：`telemetry_base_t` 承载固定多通道样本和非阻塞整帧发送契约；
`firewater_uart_t` 与 `justfloat_uart_t` 是具体协议设备，通过 `uart_base_t`
DMA 输出。协议设备拥有独立 TX 缓冲，DMA 忙时整帧丢弃。UART 回调槽由当前
协议设备独占。
- [x] 新增 `telemetry_base_t`、样本、状态与 base/ops
- [x] 新增 `firewater_uart_t` 文本协议实现
- [x] 新增 `justfloat_uart_t` 小端二进制协议实现
- [x] 对有效掩码、非有限值、帧容量和 DMA 状态做检查
- [x] 记录发送、丢弃和错误计数
- [x] 加入 CMake 与 Doxygen 输入
- [ ] 在 `rtos_shared` 中静态定义具体遥测设备并选择协议
- [ ] 从电机、IMU、底盘生成固定通道 schema
- [ ] 在 MonitorTask 中以 20..50 Hz 发送
- [ ] 将 USART2 配置为 921600 / 1 Mbps 并做板上验证

## DeviceDrivers · 电源与模拟
**架构**：电池电压、电流等模拟量通过 Platform ADC 采样，经 Filter 处理后提供给上层；模拟采样不在电机或底盘驱动里硬编码。
- [ ] 新增电池电压采样驱动
- [ ] 新增电流采样驱动（硬件支持时）
- [ ] 采样值接入 Filter 低通与标定

## Algorithm · 已废弃
**状态**：整个 Algorithm 层将被 Policy 和 Controller 完全取代，不再扩展或修复旧实现。
- [x] 确定废弃旧 omni 和旧 `algorithm_pid_HongxiWong`
- [x] Controller/Policy 完成后，从 CMake、RTOS 头文件和所有 include 中移除 Algorithm
- [x] 删除或归档 `Algorithm/` 目录

## RTOS
**架构**：静态对象按索引接线，第 i 个电机配第 i 个编码器与控制律，四个电机挂在同一个底盘下。一个固定周期任务驱动整条回路：底盘解算并下发，再逐轮运行闭环。Monitor 通过心跳与堵转标志监控。
**状态**：暂停旧接线，等待上层重构后一次性接入。
- [ ] `rtos_shared.c` 定义电机、编码器、控制律、底盘、IMU、policy 的静态对象
- [ ] `App_Init` 按索引完成电机、编码器、控制律、底盘、policy 的接线
- [ ] 确定四轮编号，并在电机数组、policy 安装角数组、底盘 wheel 数组中保持一致
- [ ] 上层接线按硬件组成对管理 STBY（`m0/m1`、`m2/m3`），禁止单电机依赖 STBY 停机
- [ ] `rtos_control.c` 实现 `RTOS_Control_StartTask`，固定周期调用底盘 update 与四个电机 update
- [ ] `rtos_control.c` 维护 `g_ctrl_heartbeat`
- [ ] `rtos_uart.c` 实现 `RTOS_UART_StartTask`，周期调用 `Dev_IMU_Base_Process(now_ms)`
- [ ] `rtos_monitor.c` 补齐控制任务死亡与电机堵转时的停机动作
- [ ] 替换 `Core/Src/freertos.c` 中 `RTOS_Control_StartTask`、`RTOS_UART_StartTask` 的 `__weak` 占位
- [ ] 将应用任务的 `osThreadNew` 动态创建改为静态 TCB/栈分配
- [ ] 为 Idle/Timer Task 提供静态内存并关闭 FreeRTOS 动态分配
- [ ] 开启并实现 `configCHECK_FOR_STACK_OVERFLOW`
- [ ] 开启并实现 `configUSE_MALLOC_FAILED_HOOK`
- [ ] 评估 `configGENERATE_RUN_TIME_STATS` 与任务运行时间统计
- [ ] 使用 `osThreadGetStackSpace` 做关键任务栈水位监控
- [ ] 按需引入队列/信号量/事件组/任务通知
- [ ] 评估 idle hook 与低功耗联动

## App
**架构**：业务逻辑只通过底盘入口下发运动命令，不直接接触电机。
**状态**：计划随上层重构重写，当前不补旧调用链。
- [ ] 业务逻辑通过 `chassis_base_t` 的 `set_motion` 下发运动命令
- [ ] 建立遥控或串口命令到 `set_motion` 的链路

## System
**架构**：时基统一——RTOS 下毫秒延时使用内核延时，裸机下使用 DWT；
微秒级延时统一使用 DWT。
- [x] 在构建配置中定义 `USE_FREERTOS`
- [x] `Sys_DelayMs` 在 RTOS 下改用 `osDelay` 或 `vTaskDelay`，并包含对应头文件
- [x] 在 System 内用条件编译（`STM32F4xx` / `STM32F1xx`）选择 MCU 头文件与延时实现，保留 DWT 细节
- [x] 在启动流程中调用 `Sys_Delay_Init()`，确保 DWT 微秒延时可用
- [x] 修复 `SYS_ABS` 对 `INT_MIN`/`LONG_MIN`/`LLONG_MIN` 的未定义行为，默认使用饱和版本

## System · 运行时与并发
**架构**：System 负责 CPU/运行时服务，不承载具体外设策略；上层通过它获得临界区、故障诊断、复位原因、看门狗策略和低功耗能力。
- [x] 新增 `system_critical`：任务/ISR 临界区与原子访问封装，RTOS 与裸机两套实现
- [x] 新增 `system_fault`：HardFault/MemManage/BusFault/UsageFault 诊断，保存 CFSR/HFSR/MMFAR/BFAR 与 MSP/PSP
- [ ] 为 fault handler 增加 R0–R3/PC/LR/xPSR 栈帧捕获（naked wrapper）
- [ ] 新增 `system_reset`：复位原因读取、记录与启动分流
- [x] 新增 `system_watchdog`：任务 check-in + 统一喂狗策略，禁止驱动直接喂狗
- [x] 新增 `system_log_base`、`system_log` 与 SEGGER RTT 后端
- [x] RTT 使用非阻塞 skip 模式并统计丢弃字节与竞争次数
- [x] `main()` 在调度器启动前安全初始化 RTT 日志
- [ ] 在 Fault 强钩子中输出固定的紧急二进制快照
- [ ] 补充 OpenOCD RTT 启动脚本或 J-Link RTT 调试配置
- [ ] 在真实板卡和主机端验证 RTT 连接、并发日志与缓冲丢弃行为
- [ ] 新增 `system_power`：PVD/BOR 状态与低功耗模式入口
- [ ] 评估 MPU：栈/堆/关键数据区保护
- [ ] 评估 SWO/ITM 与 DBGMCU freeze，补齐调试日志与暂停策略
- [ ] 新增参数存储层：Flash 参数区 + CRC 校验 + 默认值回退

## Core / Board
**架构**：Core 保持 CubeMX 生成内容，板级对象、引脚拓扑、STBY 分组和统一初始化入口应移入 Board 层。
- [x] `Core/Src/main.c` 不再直接依赖 `RTOS/Inc/rtos_shared.h`
- [ ] 建立统一的 `Board_Init()` / `System_Init()` 初始化入口
- [ ] USART2 与命令接收共用时启用 NVIC 并补充 `USART2_IRQHandler`
- [ ] 将 USART2 波特率从 9600 提升到遥测所需速率
- [ ] `Error_Handler` 在停机前通过 Board 接口切断电机输出
- [ ] 在 Board 层集中定义四轮编号、PWM/方向/STBY、编码器映射和成对 STBY 关系
- [ ] 定义 Board NVIC 优先级表（`BOARD_IRQ_PRIO_*`），在 CubeMX MSP 中引用
- [ ] 明确 STBY 硬件下拉与复位安全状态，确保看门狗/复位时电机不会保持使能
- [ ] 启动时读取并记录复位原因（IWDG/WWDG/软件/上电/欠压）
- [ ] 记录 TIM7 HAL 时基与 FreeRTOS SysTick 的关系和优先级
- [ ] 统一 `.ioc` 中 25 MHz HSE 配置与当前 `main.c` 使用 HSI PLL 的差异
- [ ] 为使用的 EXTI 引脚建立板级映射

## 构建
**说明**：CubeMX 生成的构建骨架与手写模块统一在顶层 `CMakeLists.txt` 注册，新增模块随源文件加入编译。
- [ ] 按 Platform、DeviceDrivers、Controller、Policy、Board、RTOS、App 拆分目标，用构建系统约束依赖方向
- [x] 将新增的 policy、chassis 源文件加入 `CMakeLists.txt`
- [x] 同步 `motor_base` 新接口到 `motor_tb6612`
- [ ] 更新 App/RTOS 调用点并完成运行链路接线（工程链接已通过）
- [x] 从 CMake 中移除 Algorithm、空的 controller/uart/platform_tim 文件和不再使用的旧模块
- [x] 用 `target_include_directories` 替代大量 `../../` 相对 include
- [x] 新增 `SYS_LOG_USE_RTT` CMake 开关并验证关闭后无 SEGGER 依赖
- [x] RTT、日志和遥测源码加入顶层构建
- [x] 全局启用 `-Wall -Wextra`，项目自有代码与第三方目标均无警告
- [x] 使用独立构建验证 `-Werror` 可通过
- [ ] 增加 `-Wconversion` 等更严格告警并逐步收敛

## 标定与参数
**约定**：传动常数（每圈计数、轮径）归属电机侧，几何常数（安装角、轮距、方向符号）归属 policy；控制周期常数与任务真实周期保持一致。
- [ ] 标定四轮安装角与方向符号
- [ ] 标定编码器每圈计数与轮径，得到 `counts_per_meter`
- [ ] 使 `base` 的控制周期常数与控制任务真实周期一致
- [ ] 整定轮速 PID 参数

## 清理
**说明**：源码与注释统一使用 UTF-8。
- [x] 补充 `motor_tb6612` 的关键契约注释：STBY 组共享、`power` 范围、方向切换和初始化失败状态
- [x] 添加 `.editorconfig` 固定 UTF-8

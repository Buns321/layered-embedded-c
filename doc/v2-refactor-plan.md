# V2 架构精简重构详案（路线 A · 根目录单工程）

> 状态：`已确认待执行`
> 基线提交：`992b235`（执行时于 `main` 打归档标签 `v1-layered-archive`）
> 核心方向：聚焦 **STM32 全系列**（当前实装 **STM32F407VET6**，不保留 **F103**），纯 **C11**，根目录单工程布局；**砍除 18,522 行 `Platform/` 枚举包装层与命令式装配胶水，保留 5 处高价值领域多态与静态安全底线**，用 `BSP/` + 三大静态服务 + `Car_Omni` 一键门面实现真正的“快速开始 + User 层极简”。

---

## 一、背景、目标与冻结约束

### 1.1 问题根因（基于 `992b235` 基线核查）

1. **底层税吞噬业务价值**：`Platform/`（77 文件 / 18,522 行，占共享框架 53.9%）仅将 CubeMX 全局句柄包装为枚举身份，不仅 F1/F4 13 个同名后端中有 11 个相似度高达 **96.5%–99.6%**，而且上层仍强依赖 CubeMX `.ioc` 配置与 `STM32F4xx` / Cortex-M 寄存器。
2. **过度碎片化导致上手极重**：新增/改造 F407 小车需同步维护 CubeMX `.ioc`、`Core/Src/main.c`、`Core/Board/`（326 行）、224 行板级 `CMakeLists.txt`、13 文件 `App/`（1,402 行）和 7 文件 `RTOS/`（265 行），单电机初始化 `Dev_Motor_TB6612_Init()` 多达 **16 个位置参数**。
3. **1:1 空转抽象与过重注释摩擦**：`uart_base`/`uart_port`、`encoder_base`、`joycon_base`、`policy_omni_base`、`app_car_motion` 均为单一实现配虚表/转发壳；全仓共享层与板级代码注释占比达 **45.7%**，掩盖核心逻辑。

### 1.2 冻结约束（必须严格遵守）

- **语言与编译标准**：ISO **C11**（`-std=c11 -Wall -Wextra -Werror` 零警告，ARM 与宿主机 CTest 均须遵守，严禁依赖 GNU statement-expression 等非标准扩展）。
- **静态分配铁律**：生产代码与链接产物 **0 处 `malloc` / `calloc` / `realloc` / `free`**；FreeRTOS 维持 `configSUPPORT_STATIC_ALLOCATION=1` 与 `configSUPPORT_DYNAMIC_ALLOCATION=0`（ELF 中严禁出现 `ucHeap`、`pvPortMalloc`、`vPortFree`）。
- **单向分层隔离**：`User/app_*.c` 与 `Framework/Control/` **严禁包含任何 STM32 HAL 头文件、`BSP/` 头文件或硬件引脚/句柄**；所有外设句柄隔离在 `BSP/` 与 `User/board_cfg.c`。
- **5 处保留的 C 多态边界**（首成员嵌入 `base` + 静态只读 `ops` 虚表）：
  1. `controller_base_t`（PID / Linear / Compose）
  2. `motor_base_t`（TB6612，保留后续接 CAN 电机扩展槽）
  3. `imu_base_t`（WHT101 UART / MPU6050 I2C）
  4. `telemetry_base_t`（JustFloat / FireWater）
  5. `chassis_base_t`（Omni，保留后续接差速/麦轮扩展槽）
- **显式物理单位**：继续遵循 [单位与量纲](units.md) 的 SI 单位后缀（`_mps`、`_radps`、`_rad`、`_deg`、`_dps`、`_mps2`、`_ms`、`_s`、`_m`、`_counts`、`_duty` 等）。
- **注释规范瘦身**：公共头文件（`*/Inc/*.h`）API 保留 1–3 行简明中文 `@brief` 及关键单位/范围/并发约束；私有 `static` 函数与实现文件不再强制编写多段 Doxygen `@pre/@post/@note/@warning` 模板，使有效代码注释占比收敛至 **15%–25%**。

### 1.3 量化验收红线

| 验收指标 | v1 基线（`992b235` 实测） | v2 重构硬性红线 |
| --- | --- | --- |
| 底层硬件抽象体量（`Platform/` → `BSP/`） | 77 文件 / **18,522 行** | ≤ 18 文件 / **≤ 2,000 行** |
| F407 用户侧手写代码（`User/` + 根 `CMakeLists.txt`） | 24 文件 / **2,217 行**（跨 5 区域） | **≤ 320 行**（`User/{board_cfg,app_car,rtos_tasks}.{h,c}` + `CMakeLists.txt`） |
| 单电机初始化参数数量 | **16 个位置参数** | **1 个 `const motor_tb6612_cfg_t *`** |
| 主机单元测试规模 | 62 文件 / 9,466 行（含 14 个假 HAL 测试） | ~14 文件 / **≤ 2,800 行**（纯算法/协议/服务/集成行为测试） |
| F407 Release ELF 体积（Flash / RAM） | 63,520 B / 20,408 B | **≤ 63,520 B Flash，≤ 20,408 B RAM** |
| 动态内存与栈安全门禁 | 0 动态符号，未开栈溢出检测 | 0 动态符号 + 开启 `configCHECK_FOR_STACK_OVERFLOW=2` |

### 1.4 基线审计发现的 3 处 F407 真实缺陷（重构期一并修复）

1. **`USART2` 缺失中断向量，导致 JustFloat DMA 发送首帧后永久卡死**：
   - 现状证据：`Boards/f407vet6-omni-car/Core/Src/usart.c` 为 `USART2` 启用了 `hdma_usart2_tx`（`DMA1_Stream6`），但未启用 `USART2_IRQn`；`Core/Src/stm32f4xx_it.c` 缺失 `USART2_IRQHandler()`。STM32F4 HAL 的 `HAL_UART_Transmit_DMA` 在 DMA 传输完毕后必须经 `USART2` `TC` 中断才能把 `huart2.gState` 复位为 `HAL_UART_STATE_READY` 并触发 `HAL_UART_TxCpltCallback`。
   - 修复：在 `Core/Src/usart.c` 启用 `USART2_IRQn`（优先级 `5, 0`），并在 `Core/Src/stm32f4xx_it.c` 添加 `USART2_IRQHandler()` 调用 `HAL_UART_IRQHandler(&huart2)`，同步更新 `f407vet6.ioc` 与 `doc/hardware-map.md`（原板级 `hardware-map.md` 迁至 `doc/` 或根目录文档）。
2. **全向运动学 `yaw` 恒等抵消与 `wheel_sign` 仅作用于自转项**：
   - 现状证据：`DeviceDrivers/Src/chassis_omni.c` 将 `atan2f(vy, vx) + yaw_rad` 传给 `Policy/Src/policy_omni_kinematics.c`，后者再算 `cosf(dir - (wheel_theta[i] + yaw))`，导致 `yaw` 在车体系下恒等抵消却白算三角函数并强绑 IMU；同时 `wheel_sign[i]` 只乘了自转项 `rotate` 而未乘平动项 `trans`，导致单轮接线反转无法靠 `wheel_sign=-1` 矫正。
   - 修复：将 `kinematics_omni` 显式拆为 `CHASSIS_FRAME_BODY`（零三角函数、不依赖 IMU `yaw`）与 `CHASSIS_FRAME_WORLD`（无头模式），且 `wheel_sign[i]` 作用于整轮输出 `(trans + rotate) * wheel_sign[i]`。
3. **FreeRTOS 未开启运行期栈溢出检查**：
   - 现状证据：`Core/Inc/FreeRTOSConfig.h` 未定义 `configCHECK_FOR_STACK_OVERFLOW`，`cmsis_os2.c` 中 `vApplicationStackOverflowHook` 仅为空弱函数。
   - 修复：在 `FreeRTOSConfig.h` 的 `USER CODE Defines` 区定义 `#define configCHECK_FOR_STACK_OVERFLOW 2`，并在 `Framework/Core/Src/rtos_task.c` 强实现 `vApplicationStackOverflowHook` 触发紧急安全断电钩子并停机。

---

## 二、最终根目录结构与依赖隔离矩阵

### 2.1 根目录单工程目录树

```text
layered-embedded-c/
├── AGENTS.md                        # 精简版工程规则与反过度抽象铁律（≤80 行）
├── README.md                        # 精简版总览与 5 分钟快速开始指南
├── CMakeLists.txt                   # ≤50 行：按 BUILD_HOST_TESTS 自动分流固件构建与主机测试
├── CMakePresets.json                # 统一三预设：Debug / Release / Test
├── f407vet6.ioc                     # CubeMX 工程（从 Boards/f407vet6-omni-car/ 迁至根目录）
├── startup_stm32f407xx.s
├── STM32F407xx_FLASH.ld
├── .clang-format                    # 保留
├── .clang-tidy                      # HeaderFilterRegex 更新为 ^(BSP|Framework|User)/.*\.(h|c)$
├── Core/                            # CubeMX 生成区（仅动 main.c/usart.c/stm32f4xx_it.c/FreeRTOSConfig.h/freertos.c）
├── Drivers/                         # CubeMX STM32F4 HAL 与 CMSIS
├── Middlewares/                     # 100% 归 CubeMX 独占（FreeRTOS）
├── BSP/                             # Layer 1：极薄 STM32 HAL 服务（≤18 文件，≤2,000 行）
│   ├── Inc/
│   │   ├── bsp_def.h                # bsp_status_t 统一错误码
│   │   ├── bsp_mcu.h                # 唯一芯片头选择器（STM32F4xx/G4xx/H7xx + BSP_HOST_STUB）
│   │   ├── bsp_dwt.h                # DWT 周期计数与微秒延时
│   │   ├── bsp_gpio.h               # bsp_gpio_t 描述符 + 读写翻转
│   │   ├── bsp_pwm.h                # bsp_pwm_t 描述符 + 64 位防溢出占空比
│   │   ├── bsp_encoder.h            # bsp_encoder_t（自动 16/32 位解码、极性反转、防回绕溢出）
│   │   ├── bsp_uart.h               # bsp_uart_t（IT/DMA_IDLE 双模式、环形缓冲、重挂与回调分发）
│   │   ├── bsp_i2c.h                # 轻量主机 I2C 读写与总线恢复（按 HAL_I2C_MODULE_ENABLED 启用）
│   │   └── bsp_flash.h              # 内部 Flash 读写擦除原语（按 HAL_FLASH_MODULE_ENABLED 启用）
│   └── Src/
│       ├── bsp_dwt.c
│       ├── bsp_gpio.c
│       ├── bsp_pwm.c
│       ├── bsp_encoder.c
│       ├── bsp_uart.c
│       ├── bsp_i2c.c
│       └── bsp_flash_f4.c           # 系列差异仅落在 Flash 擦除后端
├── Framework/                       # Layer 2：可复用核心、算法、驱动与小车门面
│   ├── Core/
│   │   ├── Inc/
│   │   │   ├── sys_math.h           # 合并原 abs/clamp/minmax/sign/check/math_def
│   │   │   ├── sys_angle.h
│   │   │   ├── sys_time_window.h    # 无符号 32 位回绕时间窗口
│   │   │   ├── sys_byte_ring.h      # SPSC 无锁环形字节缓冲
│   │   │   ├── sys_text.h
│   │   │   ├── sys_critical.h       # 任务/ISR 临界区
│   │   │   ├── sys_snapshot.h       # 临界区结构体快照宏
│   │   │   ├── sys_fault.h          # Cortex-M Fault 与栈溢出安全钩子
│   │   │   ├── sys_log_base.h
│   │   │   ├── sys_log.h
│   │   │   ├── sys_log_rtt.h
│   │   │   ├── sys_record_store.h   # 双页 CRC 记录存储（改为依赖 bsp_flash）
│   │   │   ├── sys_daemon.h         # [新增] 静态离线看门狗服务
│   │   │   ├── sys_topic.h          # [新增] 静态 C11 类型安全 Pub-Sub 通道宏
│   │   │   └── rtos_task.h          # [新增] 静态周期任务宏 + 耗时/栈水位监控 + 栈溢出钩子
│   │   └── Src/
│   │       ├── sys_angle.c
│   │       ├── sys_byte_ring.c
│   │       ├── sys_text.c
│   │       ├── sys_critical.c
│   │       ├── sys_fault.c
│   │       ├── sys_log_base.c
│   │       ├── sys_log.c
│   │       ├── sys_log_rtt.c
│   │       ├── sys_record_store.c
│   │       ├── sys_daemon.c
│   │       └── rtos_task.c
│   ├── Control/                     # 100% HAL-Free 纯算法层
│   │   ├── Inc/
│   │   │   ├── controller_base.h    # [多态边界 1] SISO 轮级控制律基类
│   │   │   ├── controller_pid.h     # 改为 ctrl_pid_cfg_t 结构体初始化
│   │   │   ├── controller_linear.h  # 改为 ctrl_linear_cfg_t 结构体初始化
│   │   │   ├── controller_compose.h
│   │   │   ├── filter_sample.h
│   │   │   ├── filter_base.h
│   │   │   ├── filter_passthrough.h
│   │   │   ├── filter_lowpass.h
│   │   │   ├── filter_chain.h
│   │   │   ├── filter_imu_cal.h
│   │   │   └── kinematics_omni.h    # 扁平化全向运动学（修正坐标系与 wheel_sign）
│   │   └── Src/
│   │       └── (对应 10 个 .c 文件)
│   ├── Drivers/
│   │   ├── Inc/
│   │   │   ├── motor_base.h         # [多态边界 2] 闭环电机基类
│   │   │   ├── motor_tb6612.h       # 16 参数收敛为 1 个 motor_tb6612_cfg_t
│   │   │   ├── imu_base.h           # [多态边界 3] 9 通道 IMU 基类与滤波管线
│   │   │   ├── imu_wht101.h         # 直连 bsp_uart_t + 内置可选上电归零状态机
│   │   │   ├── imu_mpu6050.h        # 直连 bsp_i2c（按需编入）
│   │   │   ├── rc_joycon.h          # 扁平化 JoyCon 串口解析（去掉 1:1 joycon_base）
│   │   │   ├── telemetry_base.h     # [多态边界 4] 统一遥测基类
│   │   │   ├── telem_justfloat.h    # 直连 bsp_uart_t DMA 发送
│   │   │   ├── telem_firewater.h    # 直连 bsp_uart_t DMA 发送
│   │   │   └── led.h                # 轻量 GPIO/PWM LED（合并原 led_base/led_gpio/led_pwm）
│   │   └── Src/
│   │       └── (对应 .c 文件 + 内部 telem_uart_tx.h/.c)
│   ├── Chassis/
│   │   ├── Inc/
│   │   │   ├── chassis_base.h       # [多态边界 5] 底盘生命周期基类
│   │   │   ├── chassis_omni.h       # 四轮全向底盘实现
│   │   │   └── car_omni.h           # [新增] 一键装配四轮全向小车门面
│   │   └── Src/
│   │       ├── chassis_base.c
│   │       ├── chassis_omni.c
│   │       └── car_omni.c
│   └── ThirdParty/
│       └── SEGGER_RTT/              # 从原根 Middlewares/Third_Party/SEGGER_RTT/ 迁入
├── User/                            # Layer 3：用户全部手写业务区（≤280 行）
│   ├── board_cfg.h / board_cfg.c    # 唯一硬件配置表（映射 CubeMX 句柄与参数）
│   ├── app_car.h   / app_car.c      # 业务入口与控制流（0 个 HAL/BSP 头文件）
│   └── rtos_tasks.h / rtos_tasks.c  # 集中声明静态周期任务与启动入口
├── cmake/
│   ├── gcc-arm-none-eabi.cmake      # 从原 F407 板级 cmake/ 迁入
│   ├── stm32cubemx/CMakeLists.txt   # 从原 F407 板级 cmake/stm32cubemx/ 迁入
│   ├── car_framework.cmake          # 统一框架子图 + car_configure_board() 宏
│   └── verify_static_elf.cmake      # ELF 零动态堆符号检查
├── tests/                           # 主机 CTest 单元测试（≤2,800 行）
│   ├── CMakeLists.txt
│   ├── stubs/
│   │   ├── bsp_host_types.h
│   │   ├── bsp_stub.c
│   │   └── rtos_stub.h
│   └── test_*.c                     # 14 个高价值单测文件
├── tools/
│   ├── check.py                     # 精简门禁（quick / pr / full，含行数预算检查）
│   ├── doc_map.json                 # 仅保留 4 条核心映射规则
│   └── rtt/                         # 保留 OpenOCD RTT 接收脚本
└── doc/
    ├── README.md                    # 文档索引
    ├── v2-refactor-plan.md          # 本文档
    ├── architecture.md              # v2 三层架构说明（≤150 行）
    ├── quickstart-new-board.md      # 新硬件接线与上手指南
    ├── hardware-map.md              # F407 引脚/时钟/DMA/NVIC 规范（由原 F407 文档迁入更新）
    ├── conventions.md               # 精简编码与注释约定
    └── units.md                     # 保留单位后缀契约（更新去除 Platform 相关旧行）
```

### 2.2 既存资产完整移除清单

| 原路径 | 处置 | 说明 |
| --- | --- | --- |
| `Platform/`（77 文件 / 18,522 行） | **全部删除** | 替换为 `BSP/`（≤2,000 行） |
| `DeviceDrivers/`、`Controller/`、`Policy/`、`Filter/`、`System/` | **抽离迁移后删除原目录** | 核心代码迁入 `Framework/`；1:1 空转发壳与未用文件直接丢弃 |
| `Filter/Src/filter_ekf.c`、`filter_distributor.c`及对应头文件 | **删除** | EKF 为未实现占位壳，Distributor 无使用方 |
| `DeviceDrivers/Base/Inc/{uart,encoder,joycon,led}_base.h` 及实现、`uart_port`、`uart_rx_stream` | **扁平化吸收后删除** | 无多实现需求，由 `BSP/` 和具体驱动直接吸收 |
| `Boards/f103c8t6-platform-test/`、`Boards/f103c8t6-fast-channel-task-1/` | **全部删除** | 已于 `v1-layered-archive` 标签留档，v2 不保留 F103 |
| `Boards/f407vet6-omni-car/` | **迁移至根目录后删除空壳** | `Core/`、`Drivers/`、`Middlewares/`、`f407vet6.ioc`、`*.ld`、`*.s` 迁入根；`Core/Board/`、`App/`、`RTOS/`、`verify_rtos_symbols.cmake` 废弃删除 |
| `cmake/{framework,platform-check,platform-test,check_layer_dependencies.cmake}` | **全部删除** | 由 `cmake/car_framework.cmake` 与 `tests/` 替代 |
| `doc/{platform,platform-contract,framework-contract,todo,imu,telemetry}.md`、`doc/examples/`、`doc/doxygen/` | **清理精简** | 删除与 v1 绑定且冗余的契约文档及示例（精简汇总进 `architecture.md` 与 `hardware-map.md`） |

### 2.3 CMake 目标有向无环依赖与 Include 边界矩阵

| CMake 目标 | 允许直接依赖的目标 / 头文件路径 | 严禁包含（由 CMake PRIVATE 隔离 + `tools/check.py` 扫描） |
| --- | --- | --- |
| **`car_bsp`** | `stm32cubemx`（或主机 `bsp_host_stub`）、`Framework/Core/Inc/sys_byte_ring.h`、`sys_critical.h` | `Framework/Control`、`Drivers`、`Chassis`、`User` |
| **`car_core`** | `BSP/Inc`（仅 `sys_critical`/`sys_fault`/`sys_log`/`sys_record_store`/`rtos_task`）、`SEGGER_RTT`（可选） | `Control`、`Drivers`、`Chassis`、`User` |
| **`car_control`** | 仅 `Framework/Core/Inc` 纯数学/角度头、`<math.h>` | **严禁 `BSP/`、HAL、FreeRTOS** |
| **`car_drivers`** | `car_bsp`、`car_core`、`car_control` | `Chassis`、`User`、`Core/Inc/main.h` |
| **`car_chassis`** | `car_core`、`car_control`、`car_drivers` | `User`、`Core/Inc/main.h` |
| **`board_user_app`**（`User/app_car.c`） | `Framework/Chassis/Inc/car_omni.h`、`Framework/Core/Inc`（`sys_topic`/`sys_daemon`/`sys_log`/`sys_math`） | **严禁 `stm32*hal*.h`、`BSP/`、`usart.h`/`tim.h`、`FreeRTOS.h`** |
| **`board_user_cfg`**（`User/board_cfg.c`） | `Core/Inc`（`tim.h`/`usart.h`/`gpio.h`）、`car_chassis`、`car_bsp` | 无 |
| **`board_user_rtos`**（`User/rtos_tasks.c`） | `rtos_task.h`、`User/app_car.h`、`cmsis_os2.h` | `BSP/`、`Drivers`、HAL |

---

## 三、Layer 1：`BSP/` 完整技术规范

### 3.1 `bsp_def.h` 与 `bsp_mcu.h`

```c
/* BSP/Inc/bsp_def.h */
#ifndef LAYERED_CAR_BSP_DEF_H
#define LAYERED_CAR_BSP_DEF_H

typedef enum bsp_status_e {
  BSP_OK = 0U,
  BSP_ERR_INVALID_ARG,
  BSP_ERR_BUSY,
  BSP_ERR_TIMEOUT,
  BSP_ERR_NO_MEM,
  BSP_ERR_HAL,
  BSP_ERR_UNSUPPORTED,
} bsp_status_t;

#endif /* LAYERED_CAR_BSP_DEF_H */
```

```c
/* BSP/Inc/bsp_mcu.h */
#ifndef LAYERED_CAR_BSP_MCU_H
#define LAYERED_CAR_BSP_MCU_H

#if defined(BSP_HOST_STUB)
  #include "bsp_host_types.h"
#elif defined(STM32F4xx)
  #include "stm32f4xx_hal.h"
#elif defined(STM32G4xx)
  #include "stm32g4xx_hal.h"
#elif defined(STM32H7xx)
  #include "stm32h7xx_hal.h"
#else
  #error "Define STM32F4xx, STM32G4xx, STM32H7xx, or BSP_HOST_STUB"
#endif

#endif /* LAYERED_CAR_BSP_MCU_H */
```

- **HAL 状态映射（私有内联于 `BSP/Src/bsp_hal_internal.h`）**：统一转换 `HAL_OK`/`HAL_ERROR`/`HAL_BUSY`/`HAL_TIMEOUT` 为 `bsp_status_t`。
- **模块自动裁剪**：`bsp_i2c.c` 包裹 `#if defined(HAL_I2C_MODULE_ENABLED)`；`bsp_flash_f4.c` 包裹 `#if defined(HAL_FLASH_MODULE_ENABLED) && defined(STM32F4xx)`。`.ioc` 未开启对应外设时自动编译为空单元，无需任何 `PLATFORM_BUILD_*` 宏。

### 3.2 `bsp_dwt.h`、`bsp_gpio.h`、`bsp_pwm.h`

```c
/* BSP/Inc/bsp_dwt.h */
void     BSP_DWT_Init(void);             /* 使能 CoreDebug->DEMCR TRCENA 与 DWT->CYCCNT */
uint32_t BSP_DWT_GetCycles(void);        /* 读取 DWT->CYCCNT */
uint32_t BSP_DWT_CyclesToUs(uint32_t cycles);
void     BSP_DWT_DelayUs(uint32_t us);   /* 忙等微秒延时，只用于短时序 */
void     BSP_DelayMs(uint32_t ms);       /* USE_FREERTOS 时调用 osDelay，裸机分片调 DelayUs */
```

```c
/* BSP/Inc/bsp_gpio.h */
typedef struct bsp_gpio_s {
  GPIO_TypeDef *port;
  uint16_t      pin;
} bsp_gpio_t;

bool BSP_GPIO_IsValid(bsp_gpio_t gpio); /* port != NULL 且 pin 为单 bit ((pin & (pin - 1U)) == 0U && pin != 0U) */
void BSP_GPIO_Write(bsp_gpio_t gpio, bool high);
bool BSP_GPIO_Read(bsp_gpio_t gpio);
void BSP_GPIO_Toggle(bsp_gpio_t gpio);
```

```c
/* BSP/Inc/bsp_pwm.h */
typedef struct bsp_pwm_s {
  TIM_HandleTypeDef *htim;
  uint32_t           channel;      /* TIM_CHANNEL_1..4 */
  uint32_t           max_duty;     /* 占空比量程，> 0；为 0 时默认按 ARR + 1 解释 */
} bsp_pwm_t;

bsp_status_t BSP_PWM_Start(const bsp_pwm_t *pwm);
bsp_status_t BSP_PWM_Stop(const bsp_pwm_t *pwm);
bsp_status_t BSP_PWM_SetDuty(const bsp_pwm_t *pwm, uint32_t duty);
```

- `BSP_PWM_SetDuty` 计算不变量：校验 `htim`、通道与 `arr = __HAL_TIM_GET_AUTORELOAD(htim)`；令 `effective_max = (pwm->max_duty > 0U) ? pwm->max_duty : (arr + 1U)`；若 `duty > effective_max` 则夹紧到 `effective_max`；使用 64 位中间量 `uint32_t ccr = (uint32_t)(((uint64_t)arr * clamped_duty) / effective_max)`，再夹紧到 `arr` 后写入 `__HAL_TIM_SET_COMPARE`。

### 3.3 `bsp_encoder.h`（合并 `Platform_Encoder` + `encoder_tim`）

```c
typedef struct bsp_encoder_cfg_s {
  TIM_HandleTypeDef *htim;
  uint32_t           max_counts_per_period; /* 0 = 不限幅；非 0 时夹紧增量并置 overflow */
  bool               invert;                /* true 时对采样增量取反 */
} bsp_encoder_cfg_t;

typedef struct bsp_encoder_s {
  TIM_HandleTypeDef *htim;
  uint32_t           last_count;
  int64_t            position_counts;
  uint32_t           max_counts_per_period;
  uint8_t            bit_width;             /* 16 或 32 */
  bool               invert;
  bool               has_base;
  bool               overflow;
} bsp_encoder_t;

bsp_status_t BSP_Encoder_Init(bsp_encoder_t *enc, const bsp_encoder_cfg_t *cfg);
int32_t      BSP_Encoder_GetDelta(bsp_encoder_t *enc);
int64_t      BSP_Encoder_GetPosition(const bsp_encoder_t *enc);
void         BSP_Encoder_Resync(bsp_encoder_t *enc);     /* 仅重对速度基线，保留 position_counts */
void         BSP_Encoder_ResetZero(bsp_encoder_t *enc);  /* 清零硬件计数器与 position_counts */
bool         BSP_Encoder_HasOverflow(const bsp_encoder_t *enc);
void         BSP_Encoder_ClearOverflow(bsp_encoder_t *enc);
```

- **实现不变量**：
  - `BSP_Encoder_Init` 检查 `htim != NULL`：若 `htim->Init.Period == 0xFFFFFFFFU` 且 `IS_TIM_32B_COUNTER_INSTANCE(htim->Instance)` 设为 `32`，若 `htim->Init.Period == 0xFFFFU` 设为 `16`，否则返回 `BSP_ERR_INVALID_ARG`；随后调用 `HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL)`。
  - `GetDelta` 首次调用建立 `last_count` 基线并返回 `0`；后续按无符号差值安全转换补码，应用 `invert` 与 `max_counts_per_period` 夹紧后累加到 `position_counts`。

### 3.4 `bsp_uart.h`（6 层合一、双 RX 模式、零 `malloc`）

```c
#ifndef BSP_UART_MAX_INSTANCES
#define BSP_UART_MAX_INSTANCES 6U
#endif

typedef enum bsp_uart_rx_mode_e {
  BSP_UART_RX_NONE = 0U,      /* 仅发送（如 JustFloat） */
  BSP_UART_RX_IT_BYTE,        /* 单字节中断入队（无 RX DMA 时首选，如 USART3） */
  BSP_UART_RX_DMA_IDLE,       /* ReceiveToIdle DMA 块入队（高频流首选） */
} bsp_uart_rx_mode_t;

typedef void (*bsp_uart_tx_done_cb_t)(void *ctx);

typedef struct bsp_uart_cfg_s {
  UART_HandleTypeDef   *huart;
  bsp_uart_rx_mode_t    rx_mode;
  uint8_t              *rx_ring_buf;     /* rx_mode != NONE 时必填，容量须为 >= 2 的 2 的幂 */
  uint16_t              rx_ring_size;
  uint8_t              *rx_dma_buf;      /* 仅 RX_DMA_IDLE 时必填，>= 2 */
  uint16_t              rx_dma_size;
  bsp_uart_tx_done_cb_t on_tx_done;      /* 可选，TX 完成/中止 ISR 回调 */
  void                 *tx_ctx;
} bsp_uart_cfg_t;

typedef struct bsp_uart_s {
  UART_HandleTypeDef   *huart;
  bsp_uart_rx_mode_t    rx_mode;
  sys_byte_ring_t       rx_ring;
  uint8_t               rx_byte;
  uint8_t              *rx_dma_buf;
  uint16_t              rx_dma_size;
  uint16_t              rx_dma_last_pos;
  bsp_uart_tx_done_cb_t on_tx_done;
  void                 *tx_ctx;
  volatile bool         tx_busy;
  volatile bool         rearm_pending;
  volatile bool         resync_requested;
  volatile uint32_t     rx_bytes;
  volatile uint32_t     rx_dropped_bytes;
  volatile uint32_t     rx_error_count;
  volatile uint32_t     tx_error_count;
  bool                  registered;
} bsp_uart_t;

bsp_status_t BSP_UART_Init(bsp_uart_t *uart, const bsp_uart_cfg_t *cfg);
bsp_status_t BSP_UART_StartRx(bsp_uart_t *uart);
void         BSP_UART_EnsureRxArmed(bsp_uart_t *uart);
bool         BSP_UART_PopByte(bsp_uart_t *uart, uint8_t *out_byte);
bool         BSP_UART_TakeResync(bsp_uart_t *uart);
bsp_status_t BSP_UART_SendBlocking(bsp_uart_t *uart, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
bsp_status_t BSP_UART_SendDMA(bsp_uart_t *uart, const uint8_t *data, uint16_t len);
bool         BSP_UART_IsTxBusy(const bsp_uart_t *uart);
void         BSP_UART_ResetSlots(void); /* 仅主机单测调用，清零 s_uart_slots */
```

- **实现不变量**：
  - `BSP_UART_Init` 扫描 `s_uart_slots[BSP_UART_MAX_INSTANCES]`：同 `huart` 重复注册返回 `BSP_ERR_BUSY`，槽满返回 `BSP_ERR_NO_MEM`，禁止 `malloc` 与 `while(1)`。
  - `BSP_UART_IsTxBusy` 使用 `(uart->huart->gState & HAL_UART_STATE_BUSY_TX) != 0U`（按位与 `&`）。
  - `HAL_UARTEx_RxEventCallback(huart, Size)` 兼容 `DMA_CIRCULAR` 与 `DMA_NORMAL`：增量计算 `rx_dma_last_pos` → `Size`（处理回绕两段入队）；仅当 `huart->hdmarx->Init.Mode != DMA_CIRCULAR` 时清空 `rx_dma_last_pos` 并重新调用 `HAL_UARTEx_ReceiveToIdle_DMA`，关闭半传输中断 `__HAL_DMA_DISABLE_IT(huart->hdmarx, DMA_IT_HT)`。
  - `HAL_UART_ErrorCallback` 显式清除 `ORE`/`FE`/`NE`/`PE` 硬件标志，置 `resync_requested = true` 并尝试重挂接收，失败时置 `rearm_pending = true` 由任务侧 `BSP_UART_EnsureRxArmed` 重试；同时清零 `tx_busy` 并递增 `tx_error_count`（若当前有 TX 故障）。

### 3.5 `bsp_i2c.h` 与 `bsp_flash.h`

```c
/* BSP/Inc/bsp_i2c.h */
typedef struct bsp_i2c_s {
  I2C_HandleTypeDef *hi2c;
  uint8_t            addr_7bit;   /* 0x00..0x7F，内部自动左移 1 位 */
  uint32_t           timeout_ms;  /* > 0 */
} bsp_i2c_t;

bsp_status_t BSP_I2C_Probe(const bsp_i2c_t *dev, uint32_t trials);
bsp_status_t BSP_I2C_MemWrite(const bsp_i2c_t *dev, uint16_t reg, uint16_t reg_size, const uint8_t *data, uint16_t len);
bsp_status_t BSP_I2C_MemRead(const bsp_i2c_t *dev, uint16_t reg, uint16_t reg_size, uint8_t *data, uint16_t len);
```

```c
/* BSP/Inc/bsp_flash.h */
uint32_t     BSP_Flash_GetEraseUnitSize(uint32_t addr); /* F407 按地址返回 16 KiB / 64 KiB / 128 KiB 扇区大小，非法地址返回 0 */
bsp_status_t BSP_Flash_Read(uint32_t addr, void *out, uint32_t size_bytes);
bsp_status_t BSP_Flash_Write(uint32_t addr, const void *data, uint32_t size_bytes); /* 4 字节对齐校验 + 临界区保护 */
bsp_status_t BSP_Flash_Erase(uint32_t start_addr, uint32_t size_bytes);            /* 对齐到扇区边界后按扇区擦除 */
```

---

## 四、Layer 2A：`Framework/Core/` 与三大静态服务规范

### 4.1 原 `System/` 迁移与合并

- `system_abs.h`、`system_clamp.h`、`system_minmax.h`、`system_sign.h`、`system_check.h`、`system_math_def.h`、`system_math.h` **合并为单一 `Framework/Core/Inc/sys_math.h`**。
- 保留 `sys_angle`、`sys_time_window`、`sys_byte_ring`、`sys_text`、`sys_critical`、`sys_snapshot`、`sys_fault`、`sys_log_base`、`sys_log`、`sys_log_rtt`、`sys_record_store`。
- 将原 `System/` 中 5 处重复 `#if defined(STM32F4xx) / STM32F1xx` 分支全部消除：延时改调用 `BSP_DWT_*`/`BSP_DelayMs`；`sys_critical.c` 与 `sys_fault.c` 统一经 `bsp_mcu.h` 或 CMSIS 核心头接入；`sys_log.c` 默认时间戳使用 `HAL_GetTick()`（并继续保留 `SYS_LOG_EXTERNAL_TIME_SOURCE` 宏供主机测试覆盖）；`sys_record_store` 依赖 `bsp_flash.h`。

### 4.2 `sys_daemon.h/.c`（静态离线看门狗服务）

```c
#ifndef SYS_DAEMON_MAX_NODES
#define SYS_DAEMON_MAX_NODES 16U
#endif

typedef enum sys_daemon_state_e {
  SYS_DAEMON_INIT_GRACE = 0U, /* 上电宽限期 */
  SYS_DAEMON_ONLINE,
  SYS_DAEMON_OFFLINE,
} sys_daemon_state_t;

typedef void (*sys_daemon_cb_t)(void *ctx, uint32_t now_ms);

typedef struct sys_daemon_cfg_s {
  const char     *name;
  uint32_t        timeout_ms;     /* 必填 > 0 */
  uint32_t        init_grace_ms;  /* 0 时默认等于 timeout_ms */
  sys_daemon_cb_t on_offline;     /* 进入 OFFLINE 边沿触发一次 */
  sys_daemon_cb_t on_recover;     /* OFFLINE -> ONLINE 边沿触发一次 */
  void           *ctx;
} sys_daemon_cfg_t;

typedef struct sys_daemon_s {
  sys_daemon_cfg_t            cfg;
  volatile uint32_t           last_feed_ms;
  volatile uint32_t           feed_seq;
  uint32_t                    registered_ms;
  uint32_t                    seen_feed_seq;
  uint32_t                    offline_count;
  volatile sys_daemon_state_t state;
  bool                        registered;
} sys_daemon_t;

void               Sys_Daemon_ResetAll(void);
bool               Sys_Daemon_Register(sys_daemon_t *node, const sys_daemon_cfg_t *cfg, uint32_t now_ms);
void               Sys_Daemon_Feed(sys_daemon_t *node, uint32_t now_ms);
void               Sys_Daemon_FeedFromISR(sys_daemon_t *node, uint32_t now_ms);
void               Sys_Daemon_Poll(uint32_t now_ms);
bool               Sys_Daemon_IsOnline(const sys_daemon_t *node);
sys_daemon_state_t Sys_Daemon_GetState(const sys_daemon_t *node);
```

- **时序与并发不变量**：
  - `Feed`/`FeedFromISR` 仅在临界区内成对更新 `(last_feed_ms, feed_seq++)`，不在喂狗路径执行回调。
  - `Sys_Daemon_Poll(now_ms)` 在临界区内原子采样 `(last_feed_ms, feed_seq)`；通过 `Sys_Time_IsExpiredMs` 处理 32 位回绕；正确执行 `INIT_GRACE` 宽限判断；边沿回调在临界区外同步执行，避免长耗时回调关闭中断。

### 4.3 `sys_topic.h`（严格 ISO C11 静态 Pub-Sub 通道）

为保证在 `-std=c11 -pedantic -Wall -Wextra -Werror` 下 100% 合法且无 GNU `({...})` 扩展，采用头文件宏生成内联强类型函数：

```c
typedef struct sys_topic_meta_s {
  uint32_t time_ms;
  uint32_t sequence;
  bool     valid;
} sys_topic_meta_t;

#define SYS_TOPIC_DECLARE(name, payload_t)                                      \
  typedef struct name##_topic_s {                                               \
    payload_t        payload;                                                   \
    sys_topic_meta_t meta;                                                      \
  } name##_topic_t;                                                             \
  extern name##_topic_t g_topic_##name;                                         \
  static inline void Topic_##name##_Publish(const payload_t *p, uint32_t now_ms) { \
    Sys_Critical_Enter();                                                       \
    g_topic_##name.payload = *p;                                                \
    g_topic_##name.meta.time_ms = now_ms;                                       \
    g_topic_##name.meta.sequence++;                                             \
    g_topic_##name.meta.valid = true;                                           \
    Sys_Critical_Exit();                                                        \
  }                                                                             \
  static inline bool Topic_##name##_Read(payload_t *out, sys_topic_meta_t *meta) { \
    sys_topic_meta_t snap_meta;                                                 \
    Sys_Critical_Enter();                                                       \
    *out = g_topic_##name.payload;                                              \
    snap_meta = g_topic_##name.meta;                                            \
    Sys_Critical_Exit();                                                        \
    if (meta != NULL) { *meta = snap_meta; }                                    \
    return snap_meta.valid;                                                     \
  }

#define SYS_TOPIC_DEFINE(name) name##_topic_t g_topic_##name = {0}
```

### 4.4 `rtos_task.h/.c`（集中静态任务 + 监控 + 栈溢出安全钩子）

```c
typedef void (*rtos_periodic_fn_t)(uint32_t now_ms);
typedef void (*rtos_emergency_halt_fn_t)(void);

typedef struct rtos_task_stats_s {
  uint32_t last_exec_us;
  uint32_t max_exec_us;
  uint32_t overrun_count;
  uint32_t stack_free_bytes;
} rtos_task_stats_t;

typedef struct rtos_periodic_task_s {
  const char        *name;
  rtos_periodic_fn_t step_fn;
  uint32_t           period_ms;
  osPriority_t       priority;
  StaticTask_t       tcb;
  uint32_t          *stack_buf;
  uint32_t           stack_bytes;
  osThreadId_t       handle;
  rtos_task_stats_t  stats;
} rtos_periodic_task_t;

/* 声明 8 字节对齐静态栈（uint64_t 数组）和任务控制块 */
#define RTOS_TASK_PERIODIC(var_name, period_ms_val, prio_val, stack_words, fn) \
  static uint64_t var_name##_stack_[((stack_words) + 1U) / 2U];                \
  static rtos_periodic_task_t var_name = {                                     \
      .name = #var_name,                                                       \
      .step_fn = (fn),                                                         \
      .period_ms = (period_ms_val),                                            \
      .priority = (prio_val),                                                  \
      .stack_buf = (uint32_t *)var_name##_stack_,                              \
      .stack_bytes = (uint32_t)sizeof(var_name##_stack_),                      \
  }

bool RTOS_Task_StartPeriodic(rtos_periodic_task_t *task);
void RTOS_Task_GetStats(const rtos_periodic_task_t *task, rtos_task_stats_t *out);
void RTOS_SetEmergencyHaltHook(rtos_emergency_halt_fn_t hook);
```

- **内部运行循环**：调用 `osKernelGetTickFreq()` 转换毫秒时间戳 `now_ms`；使用 `osDelayUntil(next_wake_tick)` 保持无漂移周期并在超时后从当前 tick 重新对齐（不补跑历史帧）；用 `BSP_DWT_GetCycles()` 测量 `step_fn` 耗时并更新 `stats`；每 1000 ms 调用一次 `osThreadGetStackSpace(osThreadGetId())`。
- **强实现 `vApplicationStackOverflowHook`**：先调用已注册的 `rtos_emergency_halt_fn_t`（切断电机 PWM/STBY），再写 `SYS_LOG_ERROR` 或置停机标志并进入关中断死循环。

---

## 五、Layer 2B：`Framework/Control/` 完整规范（100% HAL-Free）

### 5.1 控制器（保留多态边界 1）

- 保留 `controller_base.h/.c`、`controller_compose.h/.c`。
- 将 `Ctrl_PID_Init` 和 `Ctrl_Linear_Init` 的多参数列表改为配置结构体：

```c
typedef struct ctrl_pid_cfg_s {
  float kp;
  float ki;
  float kd;
  float max_target;          /* 0 = 不限制目标 */
  float min_target;          /* 当 min_target == 0 && max_target > 0 时默认取 -max_target */
  float max_out;             /* 必填 > 0 */
  float min_out;             /* 当 min_out == 0 时默认取 -max_out */
  float max_integral;        /* 当为 0 时默认等于 max_out */
  float derivative_tau_s;    /* 0 = 关闭微分低通 */
} ctrl_pid_cfg_t;

bool Ctrl_PID_Init(controller_pid_t *pid, const char *name, const ctrl_pid_cfg_t *cfg);
```

### 5.2 滤波器裁剪

- 保留：`filter_sample.h`、`filter_base.{h,c}`、`filter_passthrough.{h,c}`、`filter_lowpass.{h,c}`、`filter_chain.{h,c}`、`filter_imu_cal.{h,c}`。
- 移除：`filter_ekf`（未实现占位壳）、`filter_distributor`（无调用方）。

### 5.3 修正版 `kinematics_omni.h/.c`

```c
#define KINEMATICS_OMNI_WHEELS 4U

typedef enum chassis_frame_e {
  CHASSIS_FRAME_BODY = 0U,   /* 车体系：x 车头，y 左侧，不需要 IMU yaw */
  CHASSIS_FRAME_WORLD,       /* 世界系（无头模式）：按 -yaw_rad 旋至车体系后解算 */
} chassis_frame_t;

typedef struct kinematics_omni_cfg_s {
  float wheel_distance_m;                         /* > 0 */
  float wheel_theta_rad[KINEMATICS_OMNI_WHEELS];  /* 四轮正转方向角 (rad) */
  float wheel_sign[KINEMATICS_OMNI_WHEELS];       /* +1.0f / -1.0f，0 默认 +1.0f */
} kinematics_omni_cfg_t;

typedef struct kinematics_omni_s {
  float wheel_distance_m;
  float cos_theta[KINEMATICS_OMNI_WHEELS];
  float sin_theta[KINEMATICS_OMNI_WHEELS];
  float wheel_sign[KINEMATICS_OMNI_WHEELS];
} kinematics_omni_t;

bool Kinematics_Omni_Init(kinematics_omni_t *kin, const kinematics_omni_cfg_t *cfg);
void Kinematics_Omni_Solve(
    const kinematics_omni_t *kin,
    float vx_mps, float vy_mps, float wz_radps,
    chassis_frame_t frame, float yaw_rad,
    float out_wheel_mps[KINEMATICS_OMNI_WHEELS]);
```

- 解算公式：
  - 当 `frame == CHASSIS_FRAME_WORLD`：
    $$v_{x,\text{body}} = v_x \cos\psi + v_y \sin\psi,\quad v_{y,\text{body}} = -v_x \sin\psi + v_y \cos\psi$$
  - 逐轮计算：
    $$v_i = \left(v_{x,\text{body}}\cos\theta_i + v_{y,\text{body}}\sin\theta_i + \omega_z \cdot R\right) \cdot s_i$$
  - 任意输入非有限（`!SYS_ISFINITE`）时，四轮输出清零。

---

## 六、Layer 2C/2D：`Framework/Drivers/` 与 `Framework/Chassis/` 完整规范

### 6.1 器件驱动配置化收敛（5 处多态对齐）

1. **Motor（多态边界 2）**：
   - `motor_base_t` 持有 `controller_base_t *controller_h` 与 `bsp_encoder_t *encoder_h`（无外部编码器的总线电机可置 `NULL` 并通过 `ops` 返回测速）。
   - `motor_tb6612` 保留方向切换前先写 0 占空比与 `AIN1=AIN2=HIGH` 短路制动逻辑，16 参数收敛为：
     ```c
     typedef struct motor_tb6612_cfg_s {
       const char        *name;
       bsp_pwm_t          pwm;
       bsp_gpio_t         in1;
       bsp_gpio_t         in2;
       bsp_gpio_t         stby;              /* 允许同组共享或 {NULL, 0} */
       bsp_encoder_t     *encoder;
       controller_base_t *controller;
       float              counts_per_meter;  /* > 0 */
       float              control_period_s;  /* > 0 */
     } motor_tb6612_cfg_t;

     bsp_status_t Dev_Motor_TB6612_Init(motor_tb6612_t *motor, const motor_tb6612_cfg_t *cfg);
     ```
2. **IMU（多态边界 3）**：
   - 保留 `imu_base_t`（9 通道快照 + `filter_base_t` 管线 + `ZeroYaw`）。
   - `imu_wht101` 直连 `bsp_uart_t`，移除 `uart_port`/`uart_base`/`uart_rx_stream` 中间层；并**内置可选上电自动归零状态机**（`RATE_CONFIG -> WAIT_STILL -> ZERO_CONFIG -> WAIT_ZERO_FRAME -> READY`，由 `cfg.auto_zero_on_boot` 控制）。
   - `imu_mpu6050` 直连 `bsp_i2c_t`（保留 14 字节突发采样、轴映射与静止零偏标定）。
3. **Telemetry（多态边界 4）**：
   - 保留 `telemetry_base_t`；`telem_justfloat` 与 `telem_firewater` 内部共用 `telem_uart_tx`（直连 `bsp_uart_t` 的 `BSP_UART_SendDMA` + `on_tx_done` 回调）。
4. **RC（`rc_joycon`）与 `led`**：
   - `rc_joycon_t` 内置 `bsp_uart_t`，去掉 1:1 `joycon_base`；`led.h/.c` 合并 GPIO 与 PWM 两种轻量 LED。

### 6.2 `car_omni.h/.c`（一键四轮全向车门面）

`car_omni_t` 静态内嵌四套 `bsp_encoder_t`、`controller_pid_t`、`motor_tb6612_t`，1 套 `kinematics_omni_t`、`chassis_omni_t`，可选的 `imu_wht101_t` + `filter_passthrough_t`、`rc_joycon_t`、`telem_justfloat_t`/`telem_firewater_t`，以及 3 个 `sys_daemon_t`（IMU/RC/控制心跳）：

```c
typedef struct car_omni_wheel_cfg_s {
  bsp_pwm_t         pwm;
  bsp_encoder_cfg_t encoder;
  bsp_gpio_t        in1;
  bsp_gpio_t        in2;
  bsp_gpio_t        stby;
  float             theta_rad;
  float             sign;               /* +1.0f / -1.0f，0 默认 +1.0f */
  float             counts_per_meter;
  ctrl_pid_cfg_t    pid;
} car_omni_wheel_cfg_t;

typedef struct car_omni_cfg_s {
  float                control_period_s;     /* 必填，如 0.01f */
  float                wheel_distance_m;     /* 必填，如 0.12f */
  float                max_linear_mps;       /* 默认 0.30f */
  float                max_angular_radps;    /* 默认 1.50f */
  chassis_frame_t      drive_frame;          /* CHASSIS_FRAME_BODY 或 WORLD */
  car_omni_wheel_cfg_t wheels[4];

  struct {
    UART_HandleTypeDef *huart;               /* NULL = 不启用 WHT101 */
    uint32_t            timeout_ms;          /* 默认 250 */
    bool                auto_zero_on_boot;   /* true 时上电自动 10Hz + 静止确认 + 置零 */
  } imu_wht101;

  struct {
    UART_HandleTypeDef *huart;               /* NULL = 不启用 JoyCon 遥控 */
    uint32_t            timeout_ms;          /* 默认 300 */
    float               deadband_norm;       /* 默认 0.03f */
  } rc_joycon;

  struct {
    UART_HandleTypeDef *huart;               /* NULL = 不启用遥测 */
    uint32_t            period_ms;           /* 默认 20 */
    bool                use_firewater;       /* false = JustFloat, true = FireWater */
  } telemetry;

  uint32_t control_heartbeat_timeout_ms;     /* 默认 100 */
} car_omni_cfg_t;

typedef enum car_state_e {
  CAR_STATE_BOOTING = 0U,
  CAR_STATE_READY,
  CAR_STATE_RUNNING,
  CAR_STATE_FAULT,
} car_state_t;

bsp_status_t    Car_Omni_Init(car_omni_t *car, const car_omni_cfg_t *cfg);
void            Car_Omni_ProcessInputs(car_omni_t *car, uint32_t now_ms);
void            Car_Omni_SetTwist(car_omni_t *car, const chassis_twist_t *twist, uint32_t now_ms);
void            Car_Omni_DriveFromRC(car_omni_t *car, uint32_t now_ms);
void            Car_Omni_StepControl(car_omni_t *car, uint32_t now_ms);
void            Car_Omni_StepService(car_omni_t *car, uint32_t now_ms);
void            Car_Omni_Stop(car_omni_t *car);
void            Car_Omni_EStop(car_omni_t *car, const char *reason);
car_state_t     Car_Omni_GetState(const car_omni_t *car);
chassis_base_t *Car_Omni_Chassis(car_omni_t *car);
imu_base_t     *Car_Omni_Imu(car_omni_t *car);
```

- **渐进调试与安全闭环**：
  - `huart == NULL` 时自动跳过对应外设初始化与监控节点注册，支持“只接四轮跑开/闭环 → 接遥控 → 接 IMU/遥测”分步调试。
  - `Car_Omni_Init` 自动向 `RTOS_SetEmergencyHaltHook` 注册紧急断电回调（在 Fault 或栈溢出时直接对四轮拉低 PWM + STBY）。

---

## 七、Layer 3：根目录 F407 工程与 `User/` 规范

### 7.1 CubeMX 生成区最小 4 处改动（迁至根目录 `Core/`）

1. **`Core/Src/usart.c` & `Core/Src/stm32f4xx_it.c`**：补齐 `USART2_IRQn` 使能与 `USART2_IRQHandler()` 转发到 `HAL_UART_IRQHandler(&huart2)`（同步更新根目录 `f407vet6.ioc`）。
2. **`Core/Inc/FreeRTOSConfig.h`**：在 `USER CODE Defines` 保持 `#define configSUPPORT_DYNAMIC_ALLOCATION 0` 并新增 `#define configCHECK_FOR_STACK_OVERFLOW 2`。
3. **`Core/Src/freertos.c`**：清理 CubeMX 生成的 3 个旧弱任务（`MonitorTask`/`ControlTask`/`UARTTask`），仅保留空 `MX_FREERTOS_Init()`；Idle 与 Timer 任务的静态内存由 `cmsis_os2.c` 已有弱实现提供。
4. **`Core/Src/main.c`**：`USER CODE 2` 仅保留：
   ```c
   App_Car_Boot();
   osKernelInitialize();
   MX_FREERTOS_Init();
   RTOS_StartAllTasks();
   osKernelStart();
   ```

### 7.2 `User/` 用户代码规范（含根 `CMakeLists.txt` 合计 ≤320 行）

- **`User/board_cfg.h` / `User/board_cfg.c`（~90 行）**：返回只读 `const car_omni_cfg_t *Board_GetCarConfig(void)`，映射 `htim1..5`、`huart1..3`、`PD0..7`、`PC13`/`PE5`、`theta_rad` 与四轮 PID 参数。
- **`User/app_car.h` / `User/app_car.c`（~90 行，0 个 HAL/BSP 头文件）**：
  - `App_Car_Boot()`：调用 `BSP_DWT_Init()`（由外部初始化辅助或封装进 `Board_InitSystem()`，保证 `app_car.c` 本身不直接包含 `bsp_dwt.h`）、RTT 初始化及 `Car_Omni_Init(&s_car, Board_GetCarConfig())`。注：`app_car.c` 中仅调用 `Car_Omni_*` 与 `Sys_Log_*`，启动时硬件级 `BSP_DWT_Init` 与 `Sys_Log_RTT_Init` 由 `User/board_cfg.c` 提供的 `Board_BootServices()` 执行，确保 `app_car.c` **100% 零 `BSP/` 依赖**。
  - `App_Car_StepInput(now_ms)` → `Car_Omni_ProcessInputs(&s_car, now_ms)`
  - `App_Car_StepControl(now_ms)` → `Car_Omni_DriveFromRC(&s_car, now_ms)` + `Car_Omni_StepControl(&s_car, now_ms)`
  - `App_Car_StepService(now_ms)` → `Car_Omni_StepService(&s_car, now_ms)`
- **`User/rtos_tasks.h` / `User/rtos_tasks.c`（~35 行）**：集中用 `RTOS_TASK_PERIODIC(...)` 声明 `ControlTask`（10 ms）、`InputTask`（2 ms）、`ServiceTask`（10 ms）与 `RTOS_StartAllTasks()`。

### 7.3 根目录 `CMakeLists.txt`（≤50 行）与统一 `CMakePresets.json`

```cmake
# 根目录 CMakeLists.txt
cmake_minimum_required(VERSION 3.22)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_HOST_TESTS "构建宿主机 CTest 单元测试" OFF)
option(SYS_LOG_USE_RTT  "启用 SEGGER RTT 日志后端" ON)

if(BUILD_HOST_TESTS)
  project(layered_car_tests C)
  enable_testing()
  add_subdirectory(tests)
  return()
endif()

project(f407vet6 C ASM)
add_executable(${PROJECT_NAME})
add_subdirectory(cmake/stm32cubemx)
include(cmake/car_framework.cmake)

car_configure_board(
  TARGET        ${PROJECT_NAME}
  MCU_FAMILY    STM32F4xx
  HAL_TARGET    stm32cubemx
  USE_FREERTOS  ON
  USE_RTT       ${SYS_LOG_USE_RTT}
  APP_SOURCES   User/app_car.c
  BOARD_SOURCES User/board_cfg.c
  RTOS_SOURCES  User/rtos_tasks.c
  USER_INCLUDES User
)
```

`CMakePresets.json` 统一提供 3 个预设：`Debug`（交叉编译 `-O0 -g3`）、`Release`（交叉编译 `-Os -g0`）、`Test`（宿主机编译 `-DBUILD_HOST_TESTS=ON`），日常构建命令统一为：

```bash
cmake --preset Debug   && cmake --build --preset Debug
cmake --preset Release && cmake --build --preset Release
cmake --preset Test    && cmake --build --preset Test && ctest --preset Test --output-on-failure
```

---

## 八、主机测试矩阵、门禁与工程配置重设计

### 8.1 主机测试矩阵（`tests/CMakeLists.txt`，全部 `-std=c11 -Wall -Wextra -Werror`）

| 测试可执行目标 | 覆盖范围与关键断言 |
| --- | --- |
| `test_sys_utils` | `sys_math`、`sys_angle`、`sys_time_window`（含 `0xFFFFFFFF` 回绕）、`sys_byte_ring`、`sys_text`、`sys_log` |
| `test_sys_daemon` | `INIT_GRACE` 宽限、`ONLINE`→`OFFLINE` 边沿回调、恢复边沿回调、回绕安全、槽位上限 |
| `test_sys_topic` | 编译期类型安全发布/读取、`sequence` 递增、无效初态、临界区快照一致性 |
| `test_sys_record_store` | 基于 `bsp_flash` 桩的双页追加、CRC32 校验、页满擦除切换、坏槽恢复 |
| `test_controllers` | `controller_pid`（抗积分饱和、微分低通、默认限幅、`NaN` 防护）、`controller_linear`、`controller_compose` |
| `test_filters` | `filter_base`、`filter_lowpass`、`filter_passthrough`、`filter_chain`、`filter_imu_cal` |
| `test_kinematics_omni` | `CHASSIS_FRAME_BODY` 与 `CHASSIS_FRAME_WORLD` 解算、单轮 `sign=-1` 完整反转、非有限输入清零 |
| `test_bsp_encoder` | 16/32 位回绕解码、`invert` 极性翻转、`max_counts_per_period` 夹紧与 `overflow`、`Resync` 保位置 |
| `test_motor_tb6612` | 方向切换前先撤 PWM=0、短路制动满占空比、闭环 `Update`、控制器错误清零 |
| `test_imu_wht101` | 11 字节帧同步与校验、断帧/溢出重同步、可选上电 10 Hz + 静止确认 + 硬件/软件归零状态机 |
| `test_imu_mpu6050` | `bsp_i2c` 桩驱动 WHO_AM_I、14 字节突发换算、轴映射与静止零偏标定 |
| `test_rc_joycon` | `[j,lx,ly,rx,ry]` 正常/边界/溢出/非法帧解析、Daemon 喂狗与掉线检测 |
| `test_telemetry` | `JustFloat` 小端尾帧、`FireWater` 文本编码、DMA 忙整帧丢弃统计 |
| `test_car_omni` | 端到端集成：`Car_Omni_Init`、可选禁用 IMU/RC 模式、摇杆死区与速度圆限幅、超时急停锁存 |

### 8.2 构建宏与门禁配置

- **`cmake/car_framework.cmake`**：创建 `car_bsp`、`car_core`、`car_control`、`car_drivers`、`car_chassis` 与独立 `board_user_app`、`board_user_cfg`、`board_user_rtos` 目标；为 `board_user_app` 严格隔离 `stm32cubemx` 与 `BSP/Inc` 头路径；自动对 `STM32_Drivers`/`FreeRTOS` 屏蔽 `-Wno-unused-parameter`；并在可执行目标链接后自动挂接 `cmake/verify_static_elf.cmake`（检查 `nm -u` 与 `--defined-only` 禁止 `malloc`/`calloc`/`realloc`/`free`/`ucHeap`/`pvPortMalloc`/`vPortFree`）。
- **`tools/check.py`**：
  - `quick`：`hygiene`（UTF-8/LF/行尾空白）+ `format`（`clang-format`）+ `static-alloc` + `layers`（检查 `Framework/Control/` 与 `User/app_*.c` 禁止包含 `stm32*`/`BSP/`/`FreeRTOS`）+ `budgets`（强检查 `BSP/` ≤2,000 行、`User/`+根 `CMakeLists.txt` ≤320 行）。
  - `pr`：`quick` + `host-test`（`cmake --preset Test` + `ctest`）+ `clang-tidy`。
  - `full`：`pr` + `board-build`（根目录 `Debug` 与 `Release` 构建 + ELF 零动态符号 + `arm-none-eabi-size` 不超 63,520 B Flash / 20,408 B RAM）。
- **`.clang-tidy`**：`HeaderFilterRegex: '^(BSP|Framework|User)/.*\.(h|c)$'`。
- **`tools/doc_map.json` & `AGENTS.md`**：精简为 4 条映射规则（`doc/conventions.md`、`doc/architecture.md`、`doc/units.md`、`doc/hardware-map.md`），写入“硬件闭环优先 / 二次复用才抽象 / 代码行数预算红线”三条反过度抽象铁律。

---

## 九、分阶段实施路线图（Phase 0 → Phase 6）

| 阶段 | 精确操作步骤 | 阶段验收命令与硬性通过判据 |
| --- | --- | --- |
| **Phase 0：留档与分支准备** | 1. 确认工作区干净；2. 在 `main` 上创建不可变归档标签 `git tag -a v1-layered-archive main -m "Archive v1 layered Platform architecture"`；3. 从 `main` 切工作分支 `refactor/v2-slim-car-framework`；4. 更新 `tools/doc_map.json` 防止新建目录触发无效阻断 | `git tag -l v1-layered-archive` 存在；当前位于工作分支 |
| **Phase 1：建设 `BSP/`、主机桩与 `Framework/Core/`** | 1. 新建 `BSP/Inc/*.h`（9 头文件）与 `BSP/Src/*.c`（7 源文件）；2. 新建 `tests/stubs/{bsp_host_types.h,bsp_stub.c,rtos_stub.h}`；3. 将 `System/` 迁入 `Framework/Core/`，消除 5 处 STM32 宏重复；4. 迁入 `SEGGER_RTT` 至 `Framework/ThirdParty/SEGGER_RTT/` | `BSP/` 总行数 ≤ 2,000；`test_sys_utils`、`test_bsp_encoder`、`test_sys_record_store` 全通过 |
| **Phase 2：三大静态服务** | 1. 实现 `Framework/Core/{Inc,Src}/sys_daemon.{h,c}`、`sys_topic.h`、`rtos_task.{h,c}`；2. 编写 `tests/test_sys_daemon.c` 与 `tests/test_sys_topic.c` | `ctest` 中 `test_sys_daemon` 与 `test_sys_topic` 全绿；`-std=c11 -pedantic -Wall -Wextra -Werror` 零警告 |
| **Phase 3：`Control/`、`Drivers/`、`Chassis/` 与 `Car_Omni`** | 1. 迁移并简化 `Framework/Control/`（删 `ekf`/`distributor`、扁平修正版 `kinematics_omni`）；2. 迁移核心驱动至 `Framework/Drivers/`，统一 `cfg` 初始化；3. 实现 `Framework/Chassis/car_omni.{h,c}`；4. 补全 14 个主机单测 | 宿主机 `ctest` 14 个测试全部 PASS；`clang-tidy` 0 告警 |
| **Phase 4：根目录迁移与 F407 重构** | 1. 将 `Boards/f407vet6-omni-car/` 中 CubeMX 资产（`Core/`、`Drivers/`、`Middlewares/`、`f407vet6.ioc`、`*.ld`、`*.s`、`cmake/{gcc-arm-none-eabi.cmake,stm32cubemx}`）迁至根目录；2. 修复 `USART2_IRQn`/`USART2_IRQHandler`、开启 `configCHECK_FOR_STACK_OVERFLOW=2`、清理 `freertos.c` 旧弱任务、精简 `main.c`；3. 编写 `User/{board_cfg,app_car,rtos_tasks}.{h,c}`；4. 编写 `cmake/car_framework.cmake`、`cmake/verify_static_elf.cmake`、根 `CMakeLists.txt` 与 `CMakePresets.json` | 根目录 `Debug` 与 `Release` 编译零警告通过；ELF 0 动态符号；Release 体积 ≤ 63,520 B Flash / ≤ 20,408 B RAM；`User/` + 根 `CMakeLists.txt` ≤ 320 行 |
| **Phase 5：拆除旧资产与门禁收口** | 1. 移除旧 `Platform/`、`DeviceDrivers/`、`Controller/`、`Policy/`、`Filter/`、`System/`、`Boards/`、`cmake/{framework,platform-check,platform-test,check_layer_dependencies.cmake}` 及废弃文档；2. 更新 `tools/check.py`、`.clang-tidy`、`AGENTS.md`、`README.md` 与 `doc/` 索引 | `python3 tools/check.py --profile full --scope all` 全部阶段 PASS |
| **Phase 6：F407 真板联调验证（人工配合）** | 1. 烧录 `build/Debug/f407vet6.elf`；2. 验证 RTT 日志、`USART2` JustFloat 50 Hz 连续出帧（验证 `USART2_IRQn` 修复生效）；3. 逐轮校验电机转向、`encoder.invert` 与 `counts_per_meter`；4. 整定 PID 并实测遥控断连/IMU 超时急停 | 实车四轮稳态跟踪正常、无丢帧、无栈溢出告警、遥控失联 ≤ 300 ms 安全停稳 |

---

## 十、Re-plan 触发条件（执行期红线）

实施期间如出现以下任一情况，**立即停止扩写并回退评审**：
1. `BSP/` 总行数突破 **2,000 行**或出现新的多芯片重复镜像文件。
2. `User/` 与根目录 `CMakeLists.txt` 总行数突破 **320 行**（说明门面抽象仍存在泄漏或样板回流）。
3. `f407vet6.elf` 出现任意动态内存符号，或 `Release` 体积反超当前基线（63,520 B Flash / 20,408 B RAM）。
4. `User/app_car.c` 被迫直接包含任何 STM32 HAL 头文件或 `BSP/` 头文件。

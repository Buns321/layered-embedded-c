# V2 三层架构说明

本工程采用根目录单工程布局（ISO C11），分为三层与 CubeMX 硬件基座，所有长期对象静态分配，零动态内存。

## 1. 分层与单向依赖矩阵

```text
Layer 3: User/ + Core/
  ├── User/board_cfg.{h,c}    # 映射 CubeMX 句柄、引脚与 PID/机械参数，提供 Board_BootServices/InitCar
  ├── User/app_car.{h,c}      # 纯业务入口（启用 CAR_OMNI_APP_VIEW=1，零 HAL/BSP/FreeRTOS 依赖）
  └── User/rtos_tasks.{h,c}   # 通过 RTOS_TASK_PERIODIC 集中声明周期任务

Layer 2: Framework/
  ├── Chassis/                # chassis_base, chassis_omni, car_omni（一键整车门面）
  ├── Drivers/                # motor_*, imu_*, rc_joycon, telem_*, led（直连 bsp_*）
  ├── Control/                # 100% HAL-Free：controller_*, filter_*, kinematics_omni
  └── Core/                   # sys_*, sys_daemon, sys_topic, rtos_task（封装临界区/延时/日志/Flash）

Layer 1: BSP/                 # 极薄 STM32 HAL 服务（≤18 文件 / ≤2,000 行）
  └── bsp_mcu, bsp_def, bsp_dwt, bsp_gpio, bsp_pwm, bsp_encoder, bsp_uart, bsp_i2c, bsp_flash

Base: Core/ + Drivers/ + Middlewares/   # STM32CubeMX 生成代码、F4 HAL 与 FreeRTOS
```

CMake（[cmake/car_framework.cmake](../cmake/car_framework.cmake)）与 [tools/check.py](../tools/check.py) 强制执行以下隔离规则：

| 目标 | 允许依赖 | 严禁包含 |
| --- | --- | --- |
| `car_bsp` | `stm32cubemx`（或宿主机 `bsp_stub`）、`sys_byte_ring`、`sys_critical` | `Control/`、`Drivers/`、`Chassis/`、`User/` |
| `car_core` | `BSP/Inc`（部分模块）、`car_rtt`（可选） | `Control/`、`Drivers/`、`Chassis/`、`User/` |
| `car_control` | `Framework/Core/Inc` 纯数学/角度/快照头、`<math.h>` | **`BSP/`、STM32 HAL、FreeRTOS、`Core/Inc/`** |
| `car_drivers` | `car_bsp`、`car_core`、`car_control` | `Chassis/`、`User/`、`Core/Inc/main.h` |
| `car_chassis` | `car_core`、`car_control`、`car_drivers` | `User/`、`Core/Inc/main.h` |
| `board_user_app` | `car_omni.h`（`CAR_OMNI_APP_VIEW=1`）、`board_cfg.h`、`sys_*` | **任何 HAL、`BSP/`、`FreeRTOS`、CubeMX 外设头** |
| `board_user_rtos` | `rtos_task.h`、`User/app_car.h`、`cmsis_os2.h` | `BSP/`、`Drivers/`、HAL |

`User/app_car.c` 借助 `CAR_OMNI_APP_VIEW=1` 只可见 `car_omni_t *` 不透明指针及控制接口，硬件级 `BSP_DWT_Init` 与 `Sys_Log_RTT_Init` 隔离在 `User/board_cfg.c` 的 `Board_BootServices()` 中完成。

## 2. 保留的 5 处 C 多态边界

均采用“具体结构体首成员嵌入 `*_base_t` + 绑定静态只读 `*_ops_t` 虚表”模式，上层通过基类指针操作，禁止值拷贝：

1. **`controller_base_t`**：`controller_pid`、`controller_linear`、`controller_compose`（轮级 SISO 控制律）。
2. **`motor_base_t`**：`motor_tb6612`（闭环执行器基类，保留后续 CAN 电机扩展槽）。
3. **`imu_base_t`**：`imu_wht101`、`imu_mpu6050`（9 通道快照 + `filter_base_t` 滤波管线 + `ZeroYaw`）。
4. **`telemetry_base_t`**：`telem_justfloat`、`telem_firewater`（共享内部 `telem_uart_tx`）。
5. **`chassis_base_t`**：`chassis_omni`（底盘运动与急停生命周期基类）。

其余无多实现模块（`bsp_uart`、`bsp_encoder`、`rc_joycon`、`led`、`kinematics_omni`）直接扁平实现，不设 1:1 虚表。

## 3. 三大静态核心服务（`Framework/Core/`）

- **`sys_daemon`**：固定槽位（`SYS_DAEMON_MAX_NODES=16`）离线监控；`Feed`/`FeedFromISR` 在临界区内原子更新时间戳与序号；`Sys_Daemon_Poll()` 处理 32 位回绕与 `INIT_GRACE` 宽限，并在临界区外执行边沿回调。
- **`sys_topic`**：通过 `SYS_TOPIC_DECLARE` / `SYS_TOPIC_DEFINE` 生成严格 ISO C11 类型安全 Pub-Sub 通道（无 GNU 扩展），由 `Sys_Critical_*` 保护结构体快照一致性。
- **`rtos_task`**：`RTOS_TASK_PERIODIC` 声明 8 字节对齐 `uint64_t` 静态栈与 `StaticTask_t`；内部基于 `osDelayUntil` 保持无漂移周期并用 `BSP_DWT_GetCycles()` 统计耗时与 `osThreadGetStackSpace()` 栈余量；强实现 `vApplicationStackOverflowHook()` 在栈溢出时触发 `rtos_emergency_halt_fn_t` 并关中断停机。

## 4. `Car_Omni` 一键门面与运行流

`car_omni_t` 静态聚合 4 套 `bsp_encoder_t`/`controller_pid_t`/`motor_tb6612_t`、`kinematics_omni_t`、`chassis_omni_t` 及可选 `imu_wht101_t`/`rc_joycon_t`/`telem_*` 与 3 个 `sys_daemon_t`（`rc`、`imu`、`control_hb`）。

- **启动顺序**（[Core/Src/main.c](../Core/Src/main.c)）：`MX_*_Init()` → `App_Car_Boot()`（含注册急停钩子）→ `osKernelInitialize()` → `MX_FREERTOS_Init()` → `RTOS_StartAllTasks()` → `osKernelStart()`。
- **状态机**：`CAR_STATE_BOOTING`（若开启 `auto_zero_on_boot` 等待 WHT101 归零）→ `CAR_STATE_READY` ↔ `CAR_STATE_RUNNING`；控制心跳超时、电机控制器错误或 IMU 启动故障则进入不可逆 `CAR_STATE_FAULT` 并切断 PWM/STBY；RC/IMU 运行期离线则触发可恢复的 `Car_Omni_Stop()`。
- **按需裁剪**：配置中 `huart == NULL` 时自动跳过对应外设与看门狗，无需任何外部脚本或手写胶水即可分步调试。

## 5. 静态内存与预算红线

- 生产代码与链接产物 **0 动态分配**：FreeRTOS `configSUPPORT_DYNAMIC_ALLOCATION=0`、禁用 `heap_4.c`；[cmake/verify_static_elf.cmake](../cmake/verify_static_elf.cmake) 链接后拦截 `malloc`/`calloc`/`realloc`/`free`/`ucHeap`/`pvPortMalloc`/`vPortFree` 并强校验 `vApplicationStackOverflowHook`。
- 体量红线由 `tools/check.py` `budgets` 阶段强制门禁：`BSP/` ≤ 18 文件 / 2,000 行，`User/` + 根 `CMakeLists.txt` ≤ 320 行，`tests/test_*.c` ≤ 2,800 行，`Release` ELF ≤ 63,520 B Flash / 20,408 B RAM。

# 编码约定

## 1. 命名规范

| 前缀 / 后缀 | 归属与用途 |
| --- | --- |
| `BSP_*` | `BSP/` 硬件服务 API（`BSP_GPIO_*`、`BSP_PWM_*`、`BSP_Encoder_*`、`BSP_UART_*`、`BSP_I2C_*`、`BSP_Flash_*`、`BSP_DWT_*`） |
| `Sys_*` | `Framework/Core/` 系统与基础服务 API（`Sys_Math_*`、`Sys_Daemon_*`、`Topic_*`、`Sys_Log_*`、`Sys_Critical_*`、`Sys_RecordStore_*`） |
| `RTOS_*` | `Framework/Core/` 与 `User/rtos_tasks` 任务调度与安全钩子 API |
| `Ctrl_*` / `Filter_*` / `Kinematics_*` | `Framework/Control/` 控制律、滤波器与运动学 API |
| `Dev_*` / `RC_*` | `Framework/Drivers/` 设备驱动与遥控协议 API |
| `Chassis_*` / `Car_*` | `Framework/Chassis/` 底盘基类与整车门面 API |
| `Board_*` / `App_*` | `User/board_cfg` 板级配置与 `User/app_car` 应用入口 API |
| `*_t` / `*_base_t` / `*_ops_t` | 公共 typedef（全小写蛇形）、多态基类、只读虚表 |

### 1.1 类型、标签与变量

- 聚合类型必须显式声明标签且与 `*_t` 词干一致：struct 用 `_s`、enum 用 `_e`、union 用 `_u`（例如 `typedef struct motor_base_s { ... } motor_base_t;`）。
- 公共头文件（`BSP/Inc/*.h`、`Framework/*/Inc/*.h`、`User/*.h`）**严禁**使用匿名 `typedef struct/enum/union { ... }`。
- 枚举常量与宏使用全大写蛇形（如 `BSP_OK`、`CAR_STATE_READY`）；字段、局部变量、静态函数使用全小写蛇形。
- 指向长期存活对象的指针使用 `<role>_h`（如 `motor_base_t *motor_h`、`car_omni_t *car_h`）；同类对象优先用数组下标而非数字后缀。
- 物理量与工程量标识符必须以 [单位与量纲](units.md) 登记的单位后缀结尾（如 `control_period_s`、`wheel_distance_m`、`delta_counts`）。

### 1.2 函数命名

- 公共函数采用 `Layer_Module_[Qualifier_]VerbObject`，末尾动作为不可拆分的驼峰短语（如 `BSP_UART_SendDMA`、`Car_Omni_StepControl`）。
- 文件内 `static` 私有函数全小写蛇形（如 `car_publish_telemetry`、`app_command_parsefloat`）。
- 多参数初始化统一收敛为单个 `const *_cfg_t *cfg` 结构体指针（如 `Dev_Motor_TB6612_Init`、`Ctrl_PID_Init`、`Car_Omni_Init`）。

## 2. 对象模型与 5 处多态

- 仅以下 5 处接口保留 C 多态：`controller_base_t`、`motor_base_t`、`imu_base_t`、`telemetry_base_t`、`chassis_base_t`。
- 具体类型的**第一个成员**必须是对应 `base`；`Init` 校验参数并绑定 `static const *_ops_t`，禁止复制 `base` 对象以防切片。
- 所有长期对象（设备、控制器、滤波器、底盘、`car_omni_t`、RTOS TCB 与栈）必须静态分配，严禁 `malloc`/`calloc`/`realloc`/`free`。

## 3. 错误处理与中断安全

- 公开接口参数校验优先返回错误码（`bsp_status_t`、`filter_status_t`、`controller_status_t` 或 `bool`），不静默忽略初始化失败。
- 对外部传感器、串口与速度命令输入必须执行有限值校验（`SYS_ISFINITE`、`SYS_IS_POS_FINITE`），非有限值清零或置错。
- ISR 中仅执行最小工作：入队、清标志、重新挂接接收与 `Sys_Daemon_FeedFromISR`；禁止阻塞调用与浮点密集解算。
- ISR 与任务共享状态须使用 `volatile` 并通过 `Sys_Critical_Enter/Exit` 或 `SYS_SNAPSHOT_*` 保证原子性。

## 4. ISO C11 风格与精简注释规范

- **ISO C11 严格兼容**：所有自有代码必须通过 `-std=c11 -Wall -Wextra -Werror`（主机测试附加 `-pedantic`），禁止 GNU statement-expression 等非标准扩展；使用 `_Static_assert`、`_Generic`、`<stdbool.h>` 与定宽整数类型。
- **格式规范**：2 空格缩进、UTF-8、LF 换行、遵循 [.clang-format](../.clang-format)。
- **精简注释规范**（有效代码注释占比收敛至 15%–25%）：
  - 文件头包含 `@file` 与 1 句简体中文 `@brief`（公共头可按模块保留 `@addtogroup`）。
  - 公共头文件 API 与核心结构体保留 **1–3 行简明中文 `@brief`** 及关键单位、范围、所有权或 ISR/任务上下文约束。
  - `.c` 实现文件与私有 `static` 函数不强制编写多段 `@param`/`@return`/`@pre`/`@post`/`@note` 模板；普通 `//` 或行内注释只解释非显而易见的硬件约束、状态机步骤或并发原因。

## 5. 分层与构建约束

- 由 [cmake/car_framework.cmake](../cmake/car_framework.cmake) 中 `car_configure_board()` 与 `tools/check.py` 强制执行隔离：
  - `Framework/Control/` 严禁包含 `BSP/`、STM32 HAL、FreeRTOS 或 `Core/Inc/`。
  - `User/app_*.c` 依靠 `CAR_OMNI_APP_VIEW=1` 严禁包含任何 HAL、`BSP/`、`FreeRTOS` 或 CubeMX 外设头。
  - `Drivers/`、`Middlewares/` 与 CubeMX 生成文件仅可在 `/* USER CODE BEGIN/END */` 区域按最小必要修改，不对其套用工程格式与 lint 规则。

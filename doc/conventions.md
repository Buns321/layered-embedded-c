# 编码约定

## 1. 命名

| 前缀 / 类型 | 用途 |
| --- | --- |
| `Platform_*` | Platform 层 API |
| `Dev_*` | 设备驱动 API |
| `Ctrl_*` | Controller API |
| `Policy_*` | Policy API |
| `Chassis_*` | Chassis API |
| `Filter_*` | Filter API |
| `Sys_*` | System 数学/延时 API |
| `Sys_Log_*` | System 日志 API |
| `RTOS_*` | RTOS 任务入口 |
| `*_t` | 公共 typedef，全小写蛇形并以 `_t` 结尾 |
| `*_base_t` | 抽象接口 |
| `*_ops_t` | 虚表 |

### 1.1 类型与标签

- 公共 typedef 使用全小写蛇形并带 `_t` 后缀，例如 `platform_uart_event_t`。
- 聚合类型必须使用标签：struct 标签以 `_s` 结尾，enum 标签以 `_e` 结尾，
  union 标签以 `_u` 结尾。
- 标签词干与 typedef 词干一致，例如：
  `typedef struct motor_base_s { ... } motor_base_t;`。
- 公共头文件不得使用匿名 struct、enum 或 union。
- 枚举值和宏使用全大写蛇形，例如 `PLATFORM_STATUS_OK`。
- 结构体字段、参数和局部变量使用全小写蛇形。

### 1.2 函数

- 公共函数使用 `Layer_Module_Qualifier_VerbObject` 形式，至少包含层、模块和动作。
- 最后一个下划线之后必须是不可再拆分的驼峰动作，例如
  `Platform_RTC_StartWakeUpTimer`、`Platform_UART_ReceiveIT`、
  `Platform_ADC_StartDMA`。
- 禁止 `Platform_UART_Receive_IT`、`StartMonitor_Task` 这类把动作或词法单元
  继续用下划线拆开的写法。
- 平行模块保持相同词序。Policy 统一使用 `Policy_Omni_*`；通用串口统一使用
  `UART`，项目自有 API 不再混用 `USART`。
- 通用动作遵循同一顺序：`Init`、`Start`、`Stop`、`Read*`、`Write*`、
  `Set*`、`Get*`、`RegisterCallback`、`Clear*`。
- CubeMX/HAL 生成的符号可保留原名，例如 `huart1`、`htim1`、
  `HAL_UART_Receive_IT`；项目包装层不沿用其中的命名风格。

### 1.3 句柄

- 指向长期存活软件对象的指针使用 `<role>_h`，类型和角色必须一致，例如
  `motor_base_t *motor_h`、`filter_base_t *filter_h`、`uart_base_t *uart_h`。
- 同类对象使用数组下标，不使用编号后缀，例如：

  ```c
  motor_base_t *motor_h[4];
  motor_h[i]
  ```

- 项目自有的 HAL handle 变量同样使用 `_h`，例如
  `UART_HandleTypeDef *uart_h = &huart1;`。
- 只有 CubeMX 生成的全局符号可保留 `huart1`、`htim1`、
  `hdma_usart1_rx` 等原名。
- 不使用 `this`、`self`、`obj` 作为公共接口或回调参数；按职责命名为
  `motor_h`、`filter_h` 等。
- `_h` 只用于对象句柄。配置、状态、缓冲区、ops 表和一次性输入不作为句柄。

## 2. 对象模型

- base 必须是具体类型的第一个成员。
- 具体 init 绑定静态 ops 表，再初始化自己的字段。
- 回调第一个参数使用角色句柄，需要具体字段时强转回具体类型。
- 上层只持有 base 视图，例如 `&motor.motor_base`。
- 不复制 base 对象；复制会切片。
- 不使用动态内存，所有对象静态定义。
- 日志与遥测都遵循 base/ops；具体 RTT、FireWater、JustFloat 对象的 base
  必须是首成员。

## 3. 初始化

- 所有 init 先校验指针、数量、枚举和关键常数。
- 能失败的具体 init 返回 `platform_status_t` 或对应状态。
- 需要两段式初始化时，具体 init 负责芯片配置与 ops 绑定，base init 负责通用状态和依赖注入。
- 初始化顺序见 `architecture.md`。

## 4. 错误处理

- `assert` 只用于内部不变量和不可恢复的代码缺陷。Platform 公共输入的参数错误
  统一返回 `PLATFORM_STATUS_INVALID_ARGUMENT`，不得依赖 `assert`，详见
  [Platform 公共契约](platform-contract.md)。
- 运行时错误用状态枚举或返回值：`platform_status_t`、`dev_uart_status_t`、
  `controller_status_t`。
- 对外部输入（传感器、串口、命令）做有限值检查；非有限值清零或返回 ERROR。
- 不静默忽略初始化失败；调用方必须检查。

## 5. 单位与时间

- 对外使用 SI 单位：m、s、rad、m/s、rad/s。
- 设备层 duty 使用整数或归一化浮点，必须同时给出量程。
- `dt` 单位秒；`time_ms` 单位毫秒。
- 每个公开接口的单位写在声明旁边或 Doxygen 注释里。

## 6. ISR 与任务

- ISR 只做最小工作：入队、清标志、重新挂接收。
- 解析、控制、协议处理放在任务里。
- ISR 与任务共享的变量使用 `volatile`，并明确单写者/单读者关系。
- ISR 中不调用阻塞 API，不做浮点密集解析。

## 7. C 语言风格

- C11；使用 `_Static_assert` 做编译期检查。
- 使用 `_Generic` 的数学宏，避免函数重载。
- 2 空格缩进；回调首参使用 `<role>_h`。
- 头文件使用 include guard，保留文件头注释。
- 源码与注释统一 UTF-8。

## 8. 注释与 Doxygen

- 公共头文件写 Doxygen 注释；C 源文件只在算法或状态机复杂时写块注释。
- 注释至少说明：单位、范围、所有权、调用上下文、失败行为。
- 使用 `@brief` / `@param` / `@retval` / `@note` / `@warning`。
- 用 `@defgroup` / `@ingroup` 给模块分组。
- 不写“显然”的注释；注释解释契约和原因。

## 9. 依赖规则

- 上层依赖 base 接口，不依赖具体驱动。
- HAL 类型只能出现在 `Platform/Src/`。
- Platform 公共头只放不依赖 HAL 类型的枚举、数据结构、回调和声明。
- Platform 不调用 `HAL_*_Init()`，外设结构配置归 CubeMX/Core；只有运行期
  不可逆启动的 IWDG/WWDG 例外。
- 每个功能使用自己的实例类型，不复用定时器身份表示 PWM 和 Encoder。
- 未启用实例通过 `platform_config.h` 排除在公共枚举之外。
- 回调注册只在初始化阶段进行，一个物理实例只有一个回调所有者。
- IT/DMA 源和目的缓冲区在完成或错误回调前必须保持有效。
- 使用 CMake 的 `target_include_directories`，不在源码里写大量 `../../`。
- `Drivers/`、`Middlewares/` 不手改；CubeMX 生成代码只改 USER CODE 区。

## 10. 构建要求

- 提交前至少完成受影响目标的 Debug 构建；修改共享 Platform 或 System 代码时，
  同时验证 F4 与 F103 目标。
- 不引入动态内存。
- 新增源文件同步加入 `CMakeLists.txt`。
- 新增公共头文件同步加入 Doxygen 的 `INPUT`。

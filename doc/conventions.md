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
- 私有函数指维护中的 `.c` 文件内具有 `static` 链接的函数。私有函数沿用对应
  公共函数的模块词干和动作结构，但整个标识符转为小写，不新增或移动下划线。
  例如 `App_Command_ParseFloat` 应写为 `app_command_parsefloat()`，
  `Filter_Ekf_CoreUpdate` 应写为 `filter_ekf_coreupdate()`。
- 公共头文件中通过 `static inline` 提供给使用者的函数属于公开 API，继续使用
  公共函数命名规则，不按文件内私有函数处理。
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

### 1.4 物理量标识符

- 表示时间、长度、角度、速度、角速度、加速度、频率、电压、电流、占空比、
  编码器计数等物理量或工程量的参数、字段、局部变量、全局变量以及常量
  （包括宏和枚举值），标识符必须以规范单位后缀结尾。
- 单位后缀是标识符末尾的规范单位 token 序列；语义主体放在单位后缀之前，
  例如 `target_speed_mps`、`control_period_s`、`wheel_theta_rad`、
  `timeout_ms`。
- 单位后缀必须从最后一个语义 token 之后的下划线开始，不得与语义词粘连；
  标识符也不能只由单位 token 构成。

  ```c
  float target_speed_mps;  // 合格
  float target_speed;      // 不合格：缺少单位后缀
  float target_speedmps;   // 不合格：单位没有独立分隔
  ```

- 函数名、类型名、状态名、对象或事件计数器、索引、位掩码和纯协议控制字段
  不因本规则改名；但函数参数、结构体字段和常量值仍须遵守单位后缀规则。
  编码器 `counts` 属于工程量，仍使用 `_counts` 后缀。
- 后缀必须与实际存储的量纲和倍率一致。不得用 `_m` 表示毫米，也不得在变量
  内部保存未声明的缩放单位。
- 单位后缀不能替代接口契约。公开接口仍须在声明或 Doxygen 注释中说明单位、
  范围、符号、所有权和调用上下文。
- `_h` 只表示对象句柄，不表示亨利；需要亨利时必须先在 `doc/units.md` 登记
  无冲突后缀。
- 规范后缀及其复合形式统一登记在 `doc/units.md`。新增单位、倍率或复合单位
  必须先登记后使用，不得临时自造缩写。
- 本规则适用于新增和修改的标识符。存量不合规标识符按模块单独迁移，不得把
  单位后缀迁移混入无关功能改动。

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
- 物理量标识符必须带规范单位后缀，完整规则和后缀表见第 1.4 节和
  [单位与量纲](units.md)。
- 设备层 duty 使用整数或归一化浮点，必须同时给出量程。
- 新增或修改的控制周期或时间步参数使用 `dt_s`，单位秒；存量 `dt` 在迁移前
  保持原状。毫秒时间使用 `time_ms`。
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
- 头文件使用 include guard；文件头遵守第 8.1 节的 Doxygen 模板。
- 源码与注释统一 UTF-8。

## 8. 注释与 Doxygen

### 8.1 文件头

- 项目维护的每个 `.c/.h` 文件都必须有 Doxygen 文件头，至少包含
  `@file`、中文 `@brief` 和所属分组。
- 文件头位于可选的创建信息之后、include 之前；`@file` 只写文件名，不写路径。
- 公共头文件用 `@addtogroup` 引入模块分组，并在文件末尾用 `/** @} */` 关闭：

  ```c
  /**
   * @file led_base.h
   * @brief LED 抽象接口：开关与调光。
   * @addtogroup drivers_base
   * @{
   */
  ```

- 实现文件和私有头文件使用 `@ingroup`，不重复打开公共分组：

  ```c
  /**
   * @file led_base.c
   * @brief LED 抽象接口实现。
   * @ingroup drivers_base
   */
  ```

- 新增分组必须先写入 `doc/doxygen/groups.dox`；新增文档或维护中的测试目录必须
  同步加入 `doc/doxygen/Doxyfile` 的 `INPUT`。

### 8.2 语言与内容

- 项目自有注释、文档和 Doxygen 描述统一使用简体中文。
- 类型名、函数名、参数名、宏、协议名、芯片外设名和通用缩写保留原文，例如
  `platform_status_t`、UART、DMA、I2C、MPU6050。
- 禁止保留整句英文说明；引用数据手册原文时，应在中文说明后附最小必要原文。
- 注释至少说明：单位、范围、所有权、调用上下文、失败行为和并发限制。
- 所有公共 API 与私有函数的 Doxygen 注释不得只包含 `@brief`。公共 API 的
  注释必须同时写在公共头文件的声明处和对应 `.c` 定义处（若存在定义），两处
  保持同步；私有函数的前向声明和定义处也都必须有注释，两处保持同步。
- 每个函数都必须按签名和实际契约写全说明：每个参数使用 `@param`；非 `void`
  返回使用 `@return`，有多个预期状态时使用 `@retval` 逐项说明；存在前置条件、
  后置状态、副作用、所有权、调用上下文、失败行为、单位、范围或并发限制时，
  必须使用 `@pre`、`@post`、`@note` 或 `@warning` 明确说明。
- 没有参数且返回 `void` 的函数也至少使用 `@post` 或 `@note` 说明可观察状态
  变化或调用限制。`@brief` 只概述职责，不能代替完整契约。
- 注释应说明契约和原因，不复述函数名；单位、范围、所有权、调用上下文和
  失败行为即使可由实现推断，也不得省略。
- 普通 `//` 注释只用于解释实现原因、状态机步骤或数据手册约束，不替代文件头、
  类型和公共 API 的 Doxygen 文档。
- 不写“显然”的注释；注释解释契约、原因和边界，不复述代码。

### 8.3 检查与排除

- 提交前至少重新生成一次 Doxygen；生成警告必须为零，并检查分组归属。
- 契约适用于 `App/`、`Controller/`、`DeviceDrivers/`、`Filter/`、`Policy/`、
  `Platform/`、`RTOS/`、`System/` 及项目维护的主机测试。
- `Drivers/`、`Middlewares/`、CubeMX 生成文件和第三方源码不套用本契约，也不得
  为统一格式而手工修改。

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

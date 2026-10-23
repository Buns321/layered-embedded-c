# 编码约定

## 1. 命名

| 前缀 / 类型 | 用途 |
| --- | --- |
| `Platform_*` | Platform 层 API |
| `Dev_*` | 设备驱动 API |
| `Ctrl_*` | Controller API |
| `Omni_Policy_*` / `Policy_*` | Policy API |
| `Chassis_*` | Chassis API |
| `Filter_*` | Filter API |
| `Sys_*` | System 数学/延时 API |
| `Sys_Log_*` | System 日志 API |
| `*_t` | 类型 |
| `*_base_t` | 抽象接口 |
| `*_ops_t` | 虚表 |

## 2. 对象模型

- base 必须是具体类型的第一个成员。
- 具体 init 绑定静态 ops 表，再初始化自己的字段。
- 回调第一个参数统一叫 `this`，需要具体字段时强转回具体类型。
- 上层只持有 base 视图，例如 `&motor.motor_base`。
- 不复制 base 对象；复制会切片。
- 不使用动态内存，所有对象静态定义。
- 日志与遥测都遵循 base/ops；具体 RTT、FireWater、JustFloat 对象的 base
  必须是首成员。

## 3. 初始化

- 所有 init 先校验指针、数量、枚举和关键常数。
- 能失败的具体 init 返回 `Platform_StatusTypeDef` 或对应状态。
- 需要两段式初始化时，具体 init 负责芯片配置与 ops 绑定，base init 负责通用状态和依赖注入。
- 初始化顺序见 `architecture.md`。

## 4. 错误处理

- `assert` 用于编程错误：空指针、非法枚举、非法数量、`dt <= 0`、base 首成员假设等。
- 运行时错误用状态枚举或返回值：`Platform_StatusTypeDef`、`Dev_USART_Status`、
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
- 2 空格缩进；`this` 作为回调首参。
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

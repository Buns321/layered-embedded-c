# Platform 公共契约

## 1. 目标

Platform 的公共行为必须跨 STM32F1/F4 保持一致。允许不同的内容仅包括：

- 编译期可用实例数量和实例编号。
- 定时器、计数器等硬件位宽。
- 外设能力集合，例如 DMA 请求或 RTC 唤醒定时器。
- HAL handle 映射、时钟和寄存器差异。

F1/F4 不得在参数校验、返回值、错误快照、回调所有权、异步状态机和 abort
语义上产生差异。公共 API 必须保持同一签名，不支持的能力通过
`Platform_UNSUPPORTED` 表达，不通过删除函数或改变返回类型表达。

## 2. 状态码

所有可能失败的控制、配置和传输操作返回 `Platform_StatusTypeDef`：

| 状态 | 含义 |
| --- | --- |
| `Platform_OK` | 操作成功，或异步操作已成功启动 |
| `Platform_ERROR` | 硬件或内部执行失败 |
| `Platform_BUSY` | 资源已被占用，或重复注册/重复启动 |
| `Platform_TIMEOUT` | 阻塞等待超时 |
| `Platform_INVALID_ARGUMENT` | 公共输入不满足接口契约 |
| `Platform_UNSUPPORTED` | 当前系列或实例不支持请求的能力 |

异步启动函数返回 `Platform_OK` 只表示硬件已接受请求，不表示传输完成。

## 3. 参数校验

公共输入错误统一返回 `Platform_INVALID_ARGUMENT`，不得同时依赖 `assert`。
以下输入属于公共输入错误：

- 空指针。
- 非法实例、通道、方向或事件编号。
- 长度为 0，或与数据宽度不匹配。
- 地址、范围、对齐或位宽非法。
- 回调重复注册。

`assert` 只允许用于内部不变量，例如编译期元数据不一致或不可达的 switch
分支。Debug 和 Release 对同一个公共输入必须产生相同的状态码和错误快照。

## 4. 错误快照

提供 `Platform_X_GetLastError()` 的模块统一遵循：

1. 每次公共操作开始时清除该实例的软件错误。
2. 参数错误、状态错误和不支持能力写入通用错误位。
3. HAL 或硬件错误映射后写入模块错误位。
4. 成功操作保持错误值为 `PLATFORM_COMMON_ERROR_NONE`。
5. `GetLastError()` 只读，不清除错误。
6. 非法实例调用 `GetLastError()` 返回
   `PLATFORM_COMMON_ERROR_PARAMETER`，不得解引用硬件 handle。

通用错误位定义在 `platform_def.h`，模块专用错误位不得覆盖这些位。

## 5. 回调

所有回调注册入口统一为：

- 返回 `Platform_StatusTypeDef`。
- 一个物理实例只能注册一次。
- 重复注册返回 `Platform_BUSY`。
- 空回调结构返回 `Platform_INVALID_ARGUMENT`。
- 回调结构在注册时复制，`context` 生命周期由调用方保证。
- 回调在中断上下文执行，不得阻塞、分配内存或执行长耗时解析。
- 同一模块族使用 typed event 结构，不为每个事件维护独立的函数指针槽。

如后续需要支持替换，必须提供语义明确的 `ReplaceCallbacks()`，不得由普通
注册函数静默覆盖。

## 6. 阻塞与 timeout

- timeout 单位统一为毫秒。
- 可无限等待的接口接受 `PLATFORM_TIMEOUT_INFINITE`。
- 阻塞接口只允许在任务上下文调用。
- 不得隐藏不可配置的 HAL 默认超时。
- timeout 到期返回 `Platform_TIMEOUT`，不得伪装成 `Platform_BUSY`。
- 总线忙且尚未等待超时返回 `Platform_BUSY`。

## 7. IT / DMA 生命周期

异步传输统一遵循以下生命周期：

```text
IDLE -> BUSY -> COMPLETION
             -> ERROR
             -> ABORTING -> ABORT_COMPLETE
```

- 启动成功后进入 `BUSY`。
- 完成、错误或 abort 回调是唯一结束当前操作的事件。
- 一次操作只能产生一个最终完成、错误或 abort 事件。
- 源/目的缓冲区从启动成功开始，到最终事件前必须保持有效。
- 回调后需要重新启动的操作由上层显式发起。
- 不支持异步 abort 的外设返回 `Platform_UNSUPPORTED`，不得静默忽略。
- 中止完成后必须清除硬件和软件错误状态，允许显式重新启动。

## 8. 查询接口

可能失败的查询使用：

```c
Platform_StatusTypeDef Platform_X_GetValue(
  Platform_X_Instance instance,
  Platform_X_Value *value
);
```

不得用某个合法值表示失败。例如数据宽度查询不能把非法实例映射为 8-bit。
无失败路径的纯查询可以返回值，但非法实例仍必须有明确、可测试的结果。

## 9. 无失败快速路径

GPIO 写、PWM 比较值写入等不返回 HAL 状态的快速路径可以保持 `void`，但必须
满足：

- 实例和通道在板级静态确定。
- 非法内部参数使用 `assert`，并在头文件明确为调用前置条件。
- 不把可发生的外部错误隐藏成 `void`。

如果接口需要处理运行期实例或外部配置，必须改为返回状态。

## 10. 模块分组

同类模块使用同一设计模式：

| 分组 | 模块 | 统一内容 |
| --- | --- | --- |
| transfer | UART、I2C、SPI、CAN | 阻塞/IT/DMA、事件、busy、abort、错误 |
| converter | ADC、DAC | start/stop、DMA、完成事件、缓冲区 |
| timer | PWM、Encoder | start/stop、计数/比较、位宽查询 |
| registry | GPIO、EXTI | 静态身份、直接操作、单所有者回调 |
| service | RTC、Flash、CRC、RNG、IWDG、WWDG | 参数校验、能力、错误、状态机 |

模块的具体事件内容可以不同，但状态、校验、回调注册、错误和生命周期必须
遵循本文规则。

## 11. 实现分层

公共契约代码放在：

```text
Platform/Src/common/
```

F1/F4 目录只保留：

- HAL handle 映射。
- HAL 调用和状态转换。
- 时钟、位宽、能力差异。
- HAL 回调到统一事件的分发。

公共代码不得包含 HAL 类型。

## 12. 验收

- 同一组契约测试分别以 F1/F4 配置编译并运行。
- 同一输入产生相同状态和通用错误位。
- Debug 与 Release 的参数错误行为一致。
- 同类模块的注册、完成、错误和 abort 模式一致。
- F1/F4 全实例编译检查、`-Werror` 和板上基础验证通过。

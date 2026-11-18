# F407 四轮小车应用示例

> 状态：示例
> 架构规则：[框架契约](../framework-contract.md)
> 板级工程：[F407 板级说明](../../Boards/f407vet6-omni-car/README.md)

## Core/Board

`Core/Board` 负责：

- 定义编码器、PID、TB6612、UART Port、IMU、遥测、JoyCon、Policy 和
  Chassis 对象。
- 保存 PWM、GPIO、编码器、STBY 和 UART 映射。
- 完成设备初始化、算法接线和依赖注入。
- 请求 IMU 输出速率并完成启动。
- 填写 `app_car_dependencies_t`。
- 保持全部板级对象生命周期覆盖固件运行期。

依赖示例：

```c
typedef struct app_car_dependencies_s {
  chassis_base_t *chassis_h;
  motor_base_t *motor_h[APP_CAR_MOTOR_CAPACITY];
  uint8_t motor_count;
  imu_base_t *imu_h;
  joycon_base_t *input_h;
  telemetry_base_t *telemetry_h;
} app_car_dependencies_t;
```

入口：

```c
platform_status_t Core_Board_Init(
  app_car_dependencies_t *dependencies
);
```

## App

App 内部按职责拆分：

- `app_car.c`：初始化、任务入口协调和周期发布调度。
- `app_car_command.c`：运动命令快照。
- `app_car_boot.c`：输入处理和启动归零状态机。
- `app_car_safety.c`：故障、停止、急停和电机使能。
- `app_car_control.c`：100 Hz 输入映射、底盘和电机闭环。
- `app_car_motion.c`：twist、底盘 update、电机 update、使能/停止/急停和轮速读取。
- `app_car_telemetry.c`：遥测样本和状态日志。

App 负责：

- JoyCon 输入超时判断。
- 摇杆到车体系 `vx/vy/wz` twist 的映射。
- 通过 `chassis_base_t` 提交运动命令并使用运行时 `motor_count`。
- Motion 服务内部负责底盘与电机的更新顺序，控制、安全和遥测不直接遍历电机。
- 启动、静止确认、IMU 归零和故障状态。
- 底盘与四轮闭环的应用控制步骤。
- 16 通道遥测和状态日志。

入口：

```c
bool App_Car_Init(
  const app_car_dependencies_t *dependencies
);
```

## RTOS

| 任务 | 周期 | App 入口 |
| --- | ---: | --- |
| ControlTask | 10 ms | `App_Car_ProcessControl(now_ms)` |
| UARTTask | 2 ms | `App_Car_ProcessInputs(now_ms)` |
| MonitorTask | 10 ms | `App_Car_ProcessMonitor(now_ms)` |

CubeMX 负责任务属性、静态 TCB、静态栈和任务创建；RTOS 层负责薄任务入口、
周期和 `now_ms` 转换。三个任务共享 `RTOS_Periodic_RunTask()`，通过静态配置
注入 App runtime 函数。

## 初始化

```c
app_car_dependencies_t dependencies;

if (Core_Board_Init(&dependencies) != PLATFORM_STATUS_OK) {
  Error_Handler();
}

if (!App_Car_Init(&dependencies)) {
  Error_Handler();
}

osKernelInitialize();
MX_FREERTOS_Init(); /* CubeMX 创建静态 TCB、静态栈和任务。 */
osKernelStart();
```

## 构建边界

- `layered_app` 只包含业务源码和 App 公共头。
- `layered_rtos` 只包含 CMSIS-RTOS2 任务源码和 App runtime 头。
- `layered_core_board` 包含具体设备、Platform 和算法装配。

## 验证状态

F407 Debug/Release、F4/F1 全层检查、主机契约测试和依赖扫描已通过。
真实板卡烧录和运行验证尚未执行。

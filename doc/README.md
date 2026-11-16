# 文档索引

- [框架契约](framework-contract.md)：生效中的架构主契约，定义 App/RTOS/Core
  职责边界、跨平台和构建约束。
- [架构说明](architecture.md)：通用框架分层、对象模型、数据流和扩展方式。
- [单位与量纲](units.md)：标识符单位后缀、各层单位、换算公式、常数归属、
  标定流程。
- [编码约定](conventions.md)：命名、base/ops、初始化、错误处理、ISR 边界。
- [Platform 契约](platform.md)：CubeMX 边界、实例可用性、回调、DMA 与错误。
- [Platform 公共契约](platform-contract.md)：跨 MCU backend 的状态、错误、
  回调、异步生命周期和扩展规则。
- [IMU 与 MPU6050](imu.md)：统一 IMU 管线、WHT101 与 MPU6050 驱动说明。
- [应用示例](examples/README.md)：具体产品的 Core/Board、App 和 RTOS 接线示例。
- [F103 板级工程](../Boards/f103c8t6-platform-test/README.md)：构建、接入状态和验证入口。
- [F103 硬件与接口规范](../Boards/f103c8t6-platform-test/hardware-map.md)：完整引脚、资源、
  电源、驱动契约与风险边界。
- [F103 速通赛道任务一](../Boards/f103c8t6-fast-channel-task-1/README.md)：
  MPU6050 自动标定、Flash 记录、EKF 壳层、JustFloat 与 USART1 命令。
- [F103 速通任务一硬件规范](../Boards/f103c8t6-fast-channel-task-1/hardware-map.md)：
  引脚、Flash 分区、标定门限和电气风险。
- [F407 板级工程](../Boards/f407vet6-omni-car/README.md)：构建、目录和验证入口。
- [F407 硬件与接口规范](../Boards/f407vet6-omni-car/hardware-map.md)：当前 CubeMX 引脚、
  时钟与外设分配。
- [日志与遥测](telemetry.md)：RTT 日志、FireWater、JustFloat、UART/DMA。
- [Platform 编译检查](../cmake/platform-check/README.md)：F4/F1 全实例编译验证。
- 主机测试说明：`cmake/platform-test/README.md`，公共契约与管线测试入口。
- [迁移路线](todo.md)：功能基线和架构迁移任务，不作为门禁。
- [Doxygen 主页](doxygen/mainpage.md)：API 文档入口。

生成 API 文档：

```bash
doxygen doc/doxygen/Doxyfile
```

输出在 `build/doxygen/html/`。

# 文档索引

- [架构说明](architecture.md)：分层、对象模型、数据流、任务模型、约束。
- [单位与量纲](units.md)：各层单位、换算公式、常数归属、标定流程。
- [编码约定](conventions.md)：命名、base/ops、初始化、错误处理、ISR 边界。
- [Platform 契约](platform.md)：CubeMX 边界、实例可用性、回调、DMA 与错误。
- [Platform 公共契约](platform-contract.md)：跨 F1/F4 状态、错误、回调和
  异步生命周期规则。
- [IMU 与 MPU6050](imu.md)：统一 IMU 管线、WHT101 与 MPU6050 驱动说明。
- [F103 板级工程](../Boards/f103c8t6-platform-test/README.md)：构建、接入状态和验证入口。
- [F103 硬件与接口规范](../Boards/f103c8t6-platform-test/hardware-map.md)：完整引脚、资源、
  电源、驱动契约与风险边界。
- [F407 板级工程](../Boards/f407vet6-omni-car/README.md)：构建、目录和验证入口。
- [F407 硬件与接口规范](../Boards/f407vet6-omni-car/hardware-map.md)：当前 CubeMX 引脚、
  时钟与外设分配。
- [日志与遥测](telemetry.md)：RTT 日志、FireWater、JustFloat、UART/DMA。
- [Platform 编译检查](../cmake/platform-check/README.md)：F4/F1 全实例编译验证。
- [主机测试说明](../cmake/platform-test/README.md)：公共契约与管线测试入口。
- [TODO](todo.md)：已完成与待办事项。
- [Doxygen 主页](doxygen/mainpage.md)：API 文档入口。

生成 API 文档：

```bash
doxygen doc/doxygen/Doxyfile
```

输出在 `build/doxygen/html/`。

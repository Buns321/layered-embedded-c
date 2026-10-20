# 文档索引

- [架构说明](architecture.md)：分层、对象模型、数据流、任务模型、约束。
- [单位与量纲](units.md)：各层单位、换算公式、常数归属、标定流程。
- [编码约定](conventions.md)：命名、base/ops、初始化、错误处理、ISR 边界。
- [板级配置](board.md)：当前 CubeMX 配置、定时器/串口/引脚、待定映射。
- [TODO](todo.md)：已完成与待办事项。
- [Doxygen 主页](doxygen/mainpage.md)：API 文档入口。

生成 API 文档：

```bash
doxygen doc/doxygen/Doxyfile
```

输出在 `build/doxygen/html/`。

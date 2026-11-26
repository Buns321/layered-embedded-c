# 文档索引

- [V2 三层架构说明](architecture.md)：`BSP/`、`Framework/`、`User/` 三层职责、依赖隔离矩阵、5 处多态与三大静态服务。
- [新硬件接线与上手指南](quickstart-new-board.md)：CubeMX 最小配置、分步调试流程、故障速查与跨 STM32 系列移植清单。
- [STM32F407VET6 硬件与接口规范](hardware-map.md)：当前 `.ioc` 时钟、GPIO、TIM、USART/DMA/NVIC、JustFloat 通道与 RTOS 任务规格。
- [编码约定](conventions.md)：ISO C11 规范、命名、结构体配置初始化、错误处理、精简注释与依赖边界。
- [物理单位与量纲](units.md)：规范单位后缀表、各层物理单位、换算公式与运动学符号约定。
- [V2 架构精简重构详案](v2-refactor-plan.md)：路线 A 技术规范与实施路线全案。
- [RTT 日志接收](../tools/rtt/README.md)：通过 ST-Link + OpenOCD 接收 `SEGGER RTT` 输出。

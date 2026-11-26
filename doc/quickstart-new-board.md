# 新硬件接线与上手指南

本文面向基于本框架调试当前 F407 全向小车或接入新硬件/新 STM32 系列板卡的开发者。

## 1. 上手与构建流程

1. 确认工具链已就位：`cmake`（≥ 3.22）、`ninja`、`arm-none-eabi-gcc`。
2. 先运行宿主机测试确认环境健康：
   ```bash
   cmake --preset Test && cmake --build --preset Test && ctest --preset Test --output-on-failure
   ```
3. 构建并烧录 F407 固件：
   ```bash
   cmake --preset Debug && cmake --build --preset Debug
   openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
     -c "program build/Debug/f407vet6.elf verify reset exit"
   ```

## 2. CubeMX 关键配置 Checklist

若使用 STM32CubeMX 修改 [f407vet6.ioc](../f407vet6.ioc) 并重新生成代码，必须核对以下 5 项：

1. **开启 UART 全局中断（DMA 必配）**：只要为某串口开启 TX DMA（如 `USART2` `DMA1_Stream6` 发送 JustFloat），必须在 CubeMX NVIC 中同时勾选对应 `USARTx_IRQn`（优先级 `5, 0`），并在 `stm32f4xx_it.c` 保留 `HAL_UART_IRQHandler(&huartx)`，否则 DMA 发送首帧后无法完成 `TC` 状态复位而永久卡死。
2. **FreeRTOS 静态分配与栈溢出检测**：在 `Core/Inc/FreeRTOSConfig.h` 的 `USER CODE Defines` 区保持：
   ```c
   #undef configSUPPORT_DYNAMIC_ALLOCATION
   #define configSUPPORT_DYNAMIC_ALLOCATION 0
   #undef configCHECK_FOR_STACK_OVERFLOW
   #define configCHECK_FOR_STACK_OVERFLOW 2
   ```
3. **移除动态堆源码**：核对 [cmake/stm32cubemx/CMakeLists.txt](../cmake/stm32cubemx/CMakeLists.txt) 未重新引入 `heap_4.c`。
4. **保留空闲 `MX_FREERTOS_Init()`**：任务均在 [User/rtos_tasks.c](../User/rtos_tasks.c) 中通过 `RTOS_TASK_PERIODIC` 集中定义，[Core/Src/freertos.c](../Core/Src/freertos.c) 不创建重复任务。
5. **保留 `Core/Src/main.c` USER CODE 入口**：仅保留 `App_Car_Boot()`、`RTOS_StartAllTasks()` 与头文件引入。

## 3. 分步上电调试流程（利用 `huart == NULL` 自动裁剪）

修改 [User/board_cfg.c](../User/board_cfg.c) 中的 `s_car_cfg` 即可无需改框架代码完成分阶段联调：

### 阶段一：仅四轮开/闭环验证（不接 IMU/RC/遥测）

1. 将 `drive_frame` 设为 `CHASSIS_FRAME_BODY`，把 `imu_wht101.huart`、`rc_joycon.huart`、`telemetry.huart` 暂时置为 `NULL`。
2. 车体架空，仅逻辑供电并烧录固件，确认 RTT 输出启动日志。
3. 再接入电机 `VM` 电源，临时在任务中调用 `Car_Omni_SetTwist()` 给低速前进/平移/自转指令，结合编码器方向与实测转速完成：
   - 校正各轮 `encoder.invert`（或互换编码器 A/B 相线）与电机正转方向（调整 `sign` 或互换电机线）。
   - 标定 `counts_per_meter` 与 `wheel_distance_m`。
   - 整定四轮 `pid`（`kp`/`ki`/`kd`/`max_integral`/`derivative_tau_s`）。

### 阶段二：接入 JoyCon 遥控

1. 恢复 `.rc_joycon.huart = &huart3`。
2. 验证摇杆零位死区（`deadband_norm = 0.03f`）静止制动、各方向映射及断开蓝牙后 `300 ms` 内自动 `Car_Omni_Stop()` 停稳。

### 阶段三：接入 WHT101 与 JustFloat 遥测

1. 恢复 `.imu_wht101.huart = &huart1`、`drive_frame = CHASSIS_FRAME_WORLD` 与 `.telemetry.huart = &huart2`。
2. 上电保持车体静止，等待 `BOOTING -> READY` 完成 10 Hz 配置与自动归零。
3. 上位机（VOFA+）以 `115200 8N1` 连接蓝牙，确认 JustFloat 16 通道以 `50 Hz` 连续稳定出帧，RTT 无 `telem_drop` 递增与栈溢出告警。

## 4. 常见接线与方向异常速查

| 现象 | 优先检查项 |
| --- | --- |
| JustFloat 发首帧后卡死 | 确认 `USART2_IRQn` 使能及 `USART2_IRQHandler` 存在 |
| 单个轮子反向 | 互换电机线 `AO1/AO2` 或调整 `wheels[i].sign = -1.0f` |
| 编码器速度符号反 | 互换编码器 A/B 相或设置 `wheels[i].encoder.invert = true` |
| 平移时自转偏航 | 核对轮位 A–D 顺序与 `theta_rad`（315°/225°/135°/45°）及机械对称性 |
| 四轮不动 | 检查 RTT 日志：确认 `STBY`（`PC13`/`PE5`）、IMU 归零状态、RC 超时与 `CAR_STATE_FAULT` |

## 5. 移植至其他 STM32 系列（如 G4 / H7）

1. 使用 CubeMX 生成新芯片的 `Core/`、`Drivers/`、`Middlewares/`、`.ioc`、`.ld`、`.s` 与 `cmake/stm32cubemx/`。
2. 在 [BSP/Inc/bsp_mcu.h](../BSP/Inc/bsp_mcu.h) 核对对应系列 HAL 头宏分支。
3. 如使用 `sys_record_store`，新增 `BSP/Src/bsp_flash_<family>.c` 实现 `bsp_flash.h` 接口，并在 `cmake/car_framework.cmake` 中按 `MCU_FAMILY` 选用；无 Flash 需求可直接复用已有 6 个系列无关的 `bsp_*.c`。
4. 更新根 `CMakeLists.txt` 中的 `MCU_FAMILY`、`User/board_cfg.c` 与 `cmake/gcc-arm-none-eabi.cmake` 的 `-mcpu`/`-mfpu` 参数。

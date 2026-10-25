# 分层编译检查

这些目标用 F4/F1 两套真实芯片头文件和编译宏检查全部共享层及对应板内 App/RTOS，
同时不改变任何板级的 CubeMX 引脚映射。私有句柄桩只提供 Platform 编译单元所需的
句柄，不代表固件初始化流程。

当前覆盖：

- Platform common 与 F1/F4 backend
- DeviceDrivers Base 与具体驱动
- Controller、Policy、Filter
- System、RTOS、App
- IMU base、MPU6050 和滤波组合

每层使用独立对象目标并启用 `-Wall -Wextra -Werror`。源文件按层递归收集，
`CONFIGURE_DEPENDS` 会在新增 `.c` 文件后自动重新配置，避免新增模块漏出编译检查。

## F4

```
cmake --preset f4-debug
cmake --build --preset f4-debug
cmake --preset f4-release
cmake --build --preset f4-release
```

F4 配置启用 I2C1-3、SPI1-3、ADC1-3、DAC 通道 1-2、CAN1-2、RTC、Flash、
USART1-6、PWM TIM1、编码器 TIM2-5、CRC、RNG、IWDG 和 WWDG。独立组合目标
还会验证编码器 TIM1 配合 PWM TIM2–4。

## F1

```
cmake --preset f1-debug
cmake --build --preset f1-debug
cmake --preset f1-release
cmake --build --preset f1-release
```

F103C8T6 配置只启用芯片实际支持的实例：USART1-3、PWM TIM1、编码器 TIM2-4、
I2C1-2、SPI1-2、ADC1-2、CAN1、RTC、Flash、CRC、IWDG 和 WWDG。独立组合
目标验证编码器 TIM1 配合 PWM TIM2–4。不会暴露 DAC、I2C3、CAN2、ADC3、
SPI3 或 USART4-6。

两套配置都使用 `-Wall -Wextra` 编译。F4 与 F1 使用独立工具链和构建目录，
避免 Cortex-M4 浮点参数泄漏到 Cortex-M3 构建。

构建还会在独立对象目标中编译 PWM TIM3、编码器 TIM1、PWM TIM2–4 编码器 TIM1、
仅 CAN1 和仅 CAN2 等替代分支。

## 原生测试

与 HAL 无关的校验辅助函数通过原生 C 测试验证：

```powershell
cmake -S cmake/platform-test -B build/platform-test -G Ninja `
  -DCMAKE_C_COMPILER='C:/Users/surface/AppData/Local/Programs/CLion 2026.2.3.1/bin/mingw/bin/gcc.exe'
cmake --build build/platform-test
ctest --test-dir build/platform-test --output-on-failure
```

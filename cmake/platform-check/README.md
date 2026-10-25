# Platform 编译检查

这些目标用于编译 Platform 实现，同时不改变任何板级的 CubeMX 引脚映射。私有
句柄桩只提供 Platform 编译单元所需的句柄，不代表固件初始化流程。

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

构建还会启用 `-Werror`，并在独立对象目标中编译 PWM TIM3、编码器 TIM1、
仅 CAN1 和仅 CAN2 等替代分支。IMU base 管线与 MPU6050 I2C 驱动会分别作为
F4 和 F1 的系列专用对象目标编译。

## 原生测试

与 HAL 无关的校验辅助函数通过原生 C 测试验证：

```powershell
cmake -S cmake/platform-test -B build/platform-test -G Ninja `
  -DCMAKE_C_COMPILER='C:/Users/surface/AppData/Local/Programs/CLion 2026.2.3.1/bin/mingw/bin/gcc.exe'
cmake --build build/platform-test
ctest --test-dir build/platform-test --output-on-failure
```

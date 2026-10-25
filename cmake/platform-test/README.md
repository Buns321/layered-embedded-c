# 原生测试

这些测试覆盖与 HAL 无关的校验逻辑、算术辅助函数、System 日志门面、统一 IMU 管线、
watchdog/RTC/Flash/UART/I2C/SPI/CAN/ADC/DAC/PWM/Encoder/EXTI/GPIO/CRC/RNG
公共状态机，并通过伪 Platform I2C 传输验证 MPU6050 驱动。F1 看门狗后端测试
还覆盖 HAL 预分频编码值为 0 的合法情况。

```powershell
cmake -S cmake/platform-test -B build/platform-test -G Ninja `
  -DCMAKE_C_COMPILER='C:/Users/surface/AppData/Local/Programs/CLion 2026.2.3.1/bin/mingw/bin/gcc.exe'
cmake --build build/platform-test
ctest --test-dir build/platform-test --output-on-failure
```

可以通过 `CMAKE_C_COMPILER` 改用任意支持 C11 的原生编译器。

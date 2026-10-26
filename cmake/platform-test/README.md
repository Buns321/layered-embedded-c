# 原生测试

这些测试覆盖与 HAL 无关的校验逻辑、算术辅助函数、System 日志门面、统一 IMU 管线、
watchdog/RTC/Flash/UART/I2C/SPI/CAN/ADC/DAC/PWM/Encoder/EXTI/GPIO/CRC/RNG
公共状态机，并通过伪 Platform I2C 传输验证 MPU6050 驱动。F1 看门狗后端测试
还覆盖 HAL 预分频编码值为 0 的合法情况。PID 契约测试覆盖比例、积分、微分测量、
低通、限幅、抗饱和和错误输入；JoyCon 测试覆盖 ASCII 包头、可选换行、数值边界、
非法帧、环形队列、错误重挂和线程安全快照。速通任务一新增六面识别与加速度
标定公式、ASCII 参数命令、IMU 六轴校正、七状态 EKF 壳层在线参数和双页 Flash
记录 CRC/回退契约测试。

```powershell
cmake -S cmake/platform-test -B build/platform-test -G Ninja `
  -DCMAKE_C_COMPILER='C:/Users/surface/AppData/Local/Programs/CLion 2026.2.3.1/bin/mingw/bin/gcc.exe'
cmake --build build/platform-test
ctest --test-dir build/platform-test --output-on-failure
```

可以通过 `CMAKE_C_COMPILER` 改用任意支持 C11 的原生编译器。

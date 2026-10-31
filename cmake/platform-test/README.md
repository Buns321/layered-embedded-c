# 原生测试

这些测试覆盖与 HAL 无关的校验逻辑、算术辅助函数、System 日志门面、统一 IMU 管线、
RTC/Flash/UART/I2C/SPI/CAN/ADC/DAC/PWM/Encoder/EXTI/GPIO/CRC/RNG 公共状态机，
并通过伪 Platform I2C 传输验证 MPU6050 驱动。PID 契约测试覆盖比例、积分、微分测量、
低通、限幅、抗饱和和错误输入；JoyCon 测试覆盖 ASCII 包头、可选换行、数值边界、
非法帧、环形队列、错误重挂和线程安全快照。速通任务一新增六面识别与加速度
标定公式、ASCII 参数命令、IMU 六轴校正、七状态 EKF 壳层在线参数和双页 Flash
记录 CRC/回退契约测试。共享 UART 文本行链路测试覆盖 RX 环形缓冲、自动重挂、
CRLF 组帧、行溢出和虚拟 UART TX 完成转发；快速通道遥测契约测试覆盖 32 通道映射、
占位 EKF 掩码和命令状态发布。`system_text` 契约测试覆盖 ASCII 忽略大小写比较、
固定容量空白切分、CR 处理、非法容量和严格有限 float 解析。
毫秒窗口、字节环和角度归一化测试覆盖 32 位时间回绕、SPSC 环形缓冲边界、
deg/rad 周期归一化及非有限输入。快速通道 App 测试还覆盖参考姿态、校准记录
持久化事件、初始倒置跳过和蜂鸣器/LED 时序。

```powershell
cmake -S cmake/platform-test -B build/platform-test -G Ninja `
  -DCMAKE_C_COMPILER='C:/Users/surface/AppData/Local/Programs/CLion 2026.2.3.1/bin/mingw/bin/gcc.exe'
cmake --build build/platform-test
ctest --test-dir build/platform-test --output-on-failure
```

可以通过 `CMAKE_C_COMPILER` 改用任意支持 C11 的原生编译器。

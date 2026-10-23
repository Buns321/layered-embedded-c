# 原生测试

这些测试覆盖与 HAL 无关的校验逻辑、算术辅助函数、统一 IMU 管线，并通过伪
Platform I2C 传输验证 MPU6050 驱动。

```powershell
cmake -S cmake/platform-test -B build/platform-test -G Ninja `
  -DCMAKE_C_COMPILER='C:/Users/surface/AppData/Local/Programs/CLion 2026.2.3.1/bin/mingw/bin/gcc.exe'
cmake --build build/platform-test
ctest --test-dir build/platform-test --output-on-failure
```

可以通过 `CMAKE_C_COMPILER` 改用任意支持 C11 的原生编译器。

# Native Tests

These tests exercise HAL-free validation, arithmetic helpers, the unified IMU
pipeline, and the MPU6050 driver through a fake Platform I2C transport.

```powershell
cmake -S cmake/platform-test -B build/platform-test -G Ninja `
  -DCMAKE_C_COMPILER='C:/Users/surface/AppData/Local/Programs/CLion 2026.2.3.1/bin/mingw/bin/gcc.exe'
cmake --build build/platform-test
ctest --test-dir build/platform-test --output-on-failure
```

Any native C11 compiler can be substituted with `CMAKE_C_COMPILER`.

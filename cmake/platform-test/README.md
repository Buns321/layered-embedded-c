# Platform Native Tests

These tests exercise HAL-free validation and arithmetic helpers used by the
Platform CAN, SPI, and RTC implementations.

```powershell
cmake -S cmake/platform-test -B build/platform-test -G Ninja `
  -DCMAKE_C_COMPILER='C:/Users/surface/AppData/Local/Programs/CLion 2026.2.3.1/bin/mingw/bin/gcc.exe'
cmake --build build/platform-test
ctest --test-dir build/platform-test --output-on-failure
```

Any native C11 compiler can be substituted with `CMAKE_C_COMPILER`.

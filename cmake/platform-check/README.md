# Platform Compile Checks

These targets compile Platform implementations without changing either board's
CubeMX pin mapping. Private handle stubs supply only the handles needed by the
Platform translation units; they are not firmware initialization.

## F4

```
cmake --preset f4-debug
cmake --build --preset f4-debug
cmake --preset f4-release
cmake --build --preset f4-release
```

The F4 profile enables I2C1-3, SPI1-3, ADC1-3, DAC channels 1-2, CAN1-2, RTC, Flash,
USART1-6, PWM TIM1, Encoder TIM2-5, CRC, RNG, IWDG, and WWDG.

## F1

```
cmake --preset f1-debug
cmake --build --preset f1-debug
cmake --preset f1-release
cmake --build --preset f1-release
```

The F103C8T6 profile enables only silicon-supported instances: USART1-3,
PWM TIM1, Encoder TIM2-4, I2C1-2, SPI1-2, ADC1-2, CAN1, RTC, Flash, CRC,
IWDG, and WWDG. It does not expose DAC, I2C3, CAN2, ADC3, SPI3, or USART4-6.

Both profiles are compiled with `-Wall -Wextra`. F4 and F1 use separate
toolchains and build directories so Cortex-M4 floating-point flags never leak
into the Cortex-M3 build.

The build also enables `-Werror` and compiles the alternative PWM TIM3,
encoder TIM1, CAN1-only, and CAN2-only branches in separate object targets.
The IMU base pipeline and MPU6050 I2C driver are compiled as family-specific
object targets for both F4 and F1.

## Native tests

The HAL-free validation helpers are exercised by a native C test:

```powershell
cmake -S cmake/platform-test -B build/platform-test -G Ninja `
  -DCMAKE_C_COMPILER='C:/Users/surface/AppData/Local/Programs/CLion 2026.2.3.1/bin/mingw/bin/gcc.exe'
cmake --build build/platform-test
ctest --test-dir build/platform-test --output-on-failure
```

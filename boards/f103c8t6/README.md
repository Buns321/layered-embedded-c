# STM32F103C8T6 Board Target

Hardware and demo wiring:

- [F103C8T6 Demo hardware map](hardware-map.md)

## Hardware

- STM32F103C8T6, Cortex-M3, 64 KB FLASH, 20 KB SRAM.
- External high-speed oscillator, 72 MHz SYSCLK.
- SWD on PA13 / PA14.
- HAL timebase: SysTick, shared through the CubeMX CMSIS-RTOS integration.

## Software

- STM32CubeMX generated F1 HAL and CMSIS.
- FreeRTOS CM3 port with CMSIS-RTOS V2.
- Shared System, Platform F1, DeviceDrivers, Controller, Policy, Filter and RTOS
  sources from the repository root.
- SEGGER RTT logging is integrated manually.

## Build

```bash
cmake --preset Debug
cmake --build --preset Debug
```

Release:

```bash
cmake --preset Release
cmake --build --preset Release
```

Output:

```text
build/Debug/f103c8t6.elf
build/Debug/f103c8t6.map
```

## Current Peripheral Support

Enabled:

- GPIO / EXTI
- CRC
- IWDG / WWDG
- FreeRTOS CMSIS-RTOS V2
- SEGGER RTT log

Not configured in the current `.ioc`:

- USART1/2/3
- TIM PWM and encoder inputs
- I2C1/2
- SPI1/2
- ADC1/2
- CAN1
- RTC

`DAC` and hardware `RNG` are not available on STM32F103C8T6 and remain disabled.

## Validation Status

Build and link verification passed. Board-level validation of clock, RTT,
FreeRTOS scheduling, CRC, IWDG and WWDG is still required.

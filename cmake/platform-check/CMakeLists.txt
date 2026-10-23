cmake_minimum_required(VERSION 3.22)

project(layered_embedded_platform_check C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(PLATFORM_CHECK_FAMILY "" CACHE STRING "要编译的 Platform 系列：F4 或 F1")
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

set(PLATFORM_COMMON_INCLUDE_DIRS
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
)

if(PLATFORM_CHECK_FAMILY STREQUAL "F4")
  add_library(platform_f4_check OBJECT
      ${REPO_ROOT}/Platform/Src/common/platform_iwdg_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_exti_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_crc_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_gpio_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_adc_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_dac_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_flash_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_can_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_i2c_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_rtc_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_rng_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_encoder_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_pwm_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_spi_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_uart_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_wwdg_common.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_adc.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_can.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_crc.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_dac.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_encoder.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_exti.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_flash.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_gpio.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_i2c.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_iwdg.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_pwm.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_rng.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_rtc.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_spi.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_uart.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_wwdg.c
  )

  target_include_directories(platform_f4_check PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/handles/f4
      ${PLATFORM_COMMON_INCLUDE_DIRS}
      ${REPO_ROOT}/System/Inc
      ${REPO_ROOT}/Core/Inc
      ${REPO_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc
      ${REPO_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
      ${REPO_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
      ${REPO_ROOT}/Drivers/CMSIS/Include
  )

  target_compile_definitions(platform_f4_check PRIVATE
      USE_HAL_DRIVER
      STM32F407xx
      STM32F4xx
      PLATFORM_ENABLE_PWM_TIM1=1
      PLATFORM_ENABLE_ADC1=1
      PLATFORM_ENABLE_ADC2=1
      PLATFORM_ENABLE_ADC3=1
      PLATFORM_ENABLE_CAN1=1
      PLATFORM_ENABLE_CAN2=1
      PLATFORM_ENABLE_DAC1=1
      PLATFORM_ENABLE_ENCODER_TIM2=1
      PLATFORM_ENABLE_ENCODER_TIM3=1
      PLATFORM_ENABLE_ENCODER_TIM4=1
      PLATFORM_ENABLE_ENCODER_TIM5=1
      PLATFORM_ENABLE_UART1=1
      PLATFORM_ENABLE_UART2=1
      PLATFORM_ENABLE_UART3=1
      PLATFORM_ENABLE_UART4=1
      PLATFORM_ENABLE_UART5=1
      PLATFORM_ENABLE_UART6=1
      PLATFORM_ENABLE_I2C1=1
      PLATFORM_ENABLE_I2C2=1
      PLATFORM_ENABLE_I2C3=1
      PLATFORM_ENABLE_SPI1=1
      PLATFORM_ENABLE_SPI2=1
      PLATFORM_ENABLE_SPI3=1
      PLATFORM_ENABLE_CRC=1
      PLATFORM_ENABLE_FLASH=1
      PLATFORM_ENABLE_RNG=1
      PLATFORM_ENABLE_RTC=1
      PLATFORM_ENABLE_IWDG=1
      PLATFORM_ENABLE_WWDG=1
      HAL_I2C_MODULE_ENABLED=1
      HAL_ADC_MODULE_ENABLED=1
      HAL_CAN_MODULE_ENABLED=1
      HAL_DAC_MODULE_ENABLED=1
      HAL_RTC_MODULE_ENABLED=1
      HAL_SPI_MODULE_ENABLED=1
  )

  target_compile_options(platform_f4_check PRIVATE -Wall -Wextra -Werror)

  get_target_property(PLATFORM_F4_INCLUDE_DIRS
      platform_f4_check INCLUDE_DIRECTORIES)
  get_target_property(PLATFORM_F4_DEFINITIONS
      platform_f4_check COMPILE_DEFINITIONS)
  set(PLATFORM_F4_ALT_TIM_DEFINITIONS ${PLATFORM_F4_DEFINITIONS})
  list(REMOVE_ITEM PLATFORM_F4_ALT_TIM_DEFINITIONS
      PLATFORM_ENABLE_PWM_TIM1=1
      PLATFORM_ENABLE_ENCODER_TIM3=1
  )
  list(APPEND PLATFORM_F4_ALT_TIM_DEFINITIONS
      PLATFORM_ENABLE_PWM_TIM3=1
      PLATFORM_ENABLE_ENCODER_TIM1=1
  )

  add_library(platform_f4_alt_tim_check OBJECT
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_encoder.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_pwm.c
  )
  target_include_directories(platform_f4_alt_tim_check PRIVATE
      ${PLATFORM_F4_INCLUDE_DIRS}
  )
  target_compile_definitions(platform_f4_alt_tim_check PRIVATE
      ${PLATFORM_F4_ALT_TIM_DEFINITIONS}
  )
  target_compile_options(platform_f4_alt_tim_check PRIVATE
      -Wall -Wextra -Werror
  )

  set(PLATFORM_F4_CAN1_ONLY_DEFINITIONS ${PLATFORM_F4_DEFINITIONS})
  list(REMOVE_ITEM PLATFORM_F4_CAN1_ONLY_DEFINITIONS
      PLATFORM_ENABLE_CAN2=1
  )
  add_library(platform_f4_can1_only_check OBJECT
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_can.c
  )
  target_include_directories(platform_f4_can1_only_check PRIVATE
      ${PLATFORM_F4_INCLUDE_DIRS}
  )
  target_compile_definitions(platform_f4_can1_only_check PRIVATE
      ${PLATFORM_F4_CAN1_ONLY_DEFINITIONS}
  )
  target_compile_options(platform_f4_can1_only_check PRIVATE
      -Wall -Wextra -Werror
  )

  set(PLATFORM_F4_CAN2_ONLY_DEFINITIONS ${PLATFORM_F4_DEFINITIONS})
  list(REMOVE_ITEM PLATFORM_F4_CAN2_ONLY_DEFINITIONS
      PLATFORM_ENABLE_CAN1=1
  )
  add_library(platform_f4_can2_only_check OBJECT
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_can.c
  )
  target_include_directories(platform_f4_can2_only_check PRIVATE
      ${PLATFORM_F4_INCLUDE_DIRS}
  )
  target_compile_definitions(platform_f4_can2_only_check PRIVATE
      ${PLATFORM_F4_CAN2_ONLY_DEFINITIONS}
  )
  target_compile_options(platform_f4_can2_only_check PRIVATE
      -Wall -Wextra -Werror
  )

  add_library(mpu6050_f4_check OBJECT
      ${REPO_ROOT}/DeviceDrivers/Base/Src/imu_base.c
      ${REPO_ROOT}/DeviceDrivers/Src/mpu6050_i2c.c
      ${REPO_ROOT}/Filter/Base/Src/filter_base.c
      ${REPO_ROOT}/Filter/Src/filter_passthrough.c
  )
  target_include_directories(mpu6050_f4_check PRIVATE
      ${REPO_ROOT}/DeviceDrivers/Base/Inc
      ${REPO_ROOT}/DeviceDrivers/Inc
      ${REPO_ROOT}/Filter/Base/Inc
      ${REPO_ROOT}/Filter/Inc
      ${PLATFORM_COMMON_INCLUDE_DIRS}
      ${REPO_ROOT}/System/Inc
  )
  target_compile_definitions(mpu6050_f4_check PRIVATE
      PLATFORM_ENABLE_I2C1=1
  )
  target_compile_options(mpu6050_f4_check PRIVATE
      -Wall -Wextra -Werror
  )
elseif(PLATFORM_CHECK_FAMILY STREQUAL "F1")
  add_library(platform_f1_check OBJECT
      ${REPO_ROOT}/Platform/Src/common/platform_iwdg_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_exti_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_crc_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_gpio_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_adc_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_flash_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_can_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_i2c_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_rtc_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_encoder_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_pwm_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_spi_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_uart_common.c
      ${REPO_ROOT}/Platform/Src/common/platform_wwdg_common.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_adc.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_can.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_crc.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_encoder.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_exti.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_flash.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_gpio.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_i2c.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_iwdg.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_pwm.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_rtc.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_spi.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_uart.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_wwdg.c
  )

  target_include_directories(platform_f1_check PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/handles/f1
      ${PLATFORM_COMMON_INCLUDE_DIRS}
      ${REPO_ROOT}/System/Inc
      ${REPO_ROOT}/boards/f103c8t6/Core/Inc
      ${REPO_ROOT}/boards/f103c8t6/Drivers/STM32F1xx_HAL_Driver/Inc
      ${REPO_ROOT}/boards/f103c8t6/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
      ${REPO_ROOT}/boards/f103c8t6/Drivers/CMSIS/Device/ST/STM32F1xx/Include
      ${REPO_ROOT}/boards/f103c8t6/Drivers/CMSIS/Include
  )

  target_compile_definitions(platform_f1_check PRIVATE
      USE_HAL_DRIVER
      STM32F103xB
      STM32F1xx
      PLATFORM_ENABLE_GPIO_D=1
      PLATFORM_ENABLE_GPIO_E=0
      PLATFORM_ENABLE_PWM_TIM1=1
      PLATFORM_ENABLE_CRC=1
      PLATFORM_ENABLE_ADC1=1
      PLATFORM_ENABLE_ADC2=1
      PLATFORM_ENABLE_CAN1=1
      PLATFORM_ENABLE_ENCODER_TIM2=1
      PLATFORM_ENABLE_ENCODER_TIM3=1
      PLATFORM_ENABLE_ENCODER_TIM4=1
      PLATFORM_ENABLE_FLASH=1
      PLATFORM_ENABLE_I2C1=1
      PLATFORM_ENABLE_I2C2=1
      PLATFORM_ENABLE_IWDG=1
      PLATFORM_ENABLE_RTC=1
      PLATFORM_ENABLE_SPI1=1
      PLATFORM_ENABLE_SPI2=1
      PLATFORM_ENABLE_UART1=1
      PLATFORM_ENABLE_UART2=1
      PLATFORM_ENABLE_UART3=1
      PLATFORM_ENABLE_WWDG=1
      HAL_I2C_MODULE_ENABLED=1
      HAL_ADC_MODULE_ENABLED=1
      HAL_CAN_MODULE_ENABLED=1
      HAL_RTC_MODULE_ENABLED=1
      HAL_SPI_MODULE_ENABLED=1
      HAL_TIM_MODULE_ENABLED=1
      HAL_UART_MODULE_ENABLED=1
  )

  target_compile_options(platform_f1_check PRIVATE -Wall -Wextra -Werror)

  get_target_property(PLATFORM_F1_INCLUDE_DIRS
      platform_f1_check INCLUDE_DIRECTORIES)
  get_target_property(PLATFORM_F1_DEFINITIONS
      platform_f1_check COMPILE_DEFINITIONS)
  set(PLATFORM_F1_ALT_TIM_DEFINITIONS ${PLATFORM_F1_DEFINITIONS})
  list(REMOVE_ITEM PLATFORM_F1_ALT_TIM_DEFINITIONS
      PLATFORM_ENABLE_PWM_TIM1=1
      PLATFORM_ENABLE_ENCODER_TIM3=1
  )
  list(APPEND PLATFORM_F1_ALT_TIM_DEFINITIONS
      PLATFORM_ENABLE_PWM_TIM3=1
      PLATFORM_ENABLE_ENCODER_TIM1=1
  )

  add_library(platform_f1_alt_tim_check OBJECT
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_encoder.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_pwm.c
  )
  target_include_directories(platform_f1_alt_tim_check PRIVATE
      ${PLATFORM_F1_INCLUDE_DIRS}
  )
  target_compile_definitions(platform_f1_alt_tim_check PRIVATE
      ${PLATFORM_F1_ALT_TIM_DEFINITIONS}
  )
  target_compile_options(platform_f1_alt_tim_check PRIVATE
      -Wall -Wextra -Werror
  )

  add_library(mpu6050_f1_check OBJECT
      ${REPO_ROOT}/DeviceDrivers/Base/Src/imu_base.c
      ${REPO_ROOT}/DeviceDrivers/Src/mpu6050_i2c.c
      ${REPO_ROOT}/Filter/Base/Src/filter_base.c
      ${REPO_ROOT}/Filter/Src/filter_passthrough.c
  )
  target_include_directories(mpu6050_f1_check PRIVATE
      ${REPO_ROOT}/DeviceDrivers/Base/Inc
      ${REPO_ROOT}/DeviceDrivers/Inc
      ${REPO_ROOT}/Filter/Base/Inc
      ${REPO_ROOT}/Filter/Inc
      ${PLATFORM_COMMON_INCLUDE_DIRS}
      ${REPO_ROOT}/System/Inc
  )
  target_compile_definitions(mpu6050_f1_check PRIVATE
      PLATFORM_ENABLE_I2C1=1
  )
  target_compile_options(mpu6050_f1_check PRIVATE
      -Wall -Wextra -Werror
  )
else()
  message(FATAL_ERROR "请将 PLATFORM_CHECK_FAMILY 设置为 F4 或 F1")
endif()

cmake_minimum_required(VERSION 3.22)

project(layered_embedded_layer_check C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(PLATFORM_CHECK_FAMILY "" CACHE STRING "要编译的目标系列：F4 或 F1")
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

if(PLATFORM_CHECK_FAMILY STREQUAL "F4")
  set(BOARD_ROOT ${REPO_ROOT}/Boards/f407vet6-omni-car)
elseif(PLATFORM_CHECK_FAMILY STREQUAL "F1")
  set(BOARD_ROOT ${REPO_ROOT}/Boards/f103c8t6-platform-test)
else()
  message(FATAL_ERROR "PLATFORM_CHECK_FAMILY must be F4 or F1")
endif()

set(PLATFORM_COMMON_INCLUDE_DIRS
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
)

file(GLOB_RECURSE APP_CHECK_SOURCES CONFIGURE_DEPENDS
    ${BOARD_ROOT}/App/*.c
)
file(GLOB_RECURSE CONTROLLER_CHECK_SOURCES CONFIGURE_DEPENDS
    ${REPO_ROOT}/Controller/*.c
)
file(GLOB_RECURSE DEVICE_BASE_CHECK_SOURCES CONFIGURE_DEPENDS
    ${REPO_ROOT}/DeviceDrivers/Base/Src/*.c
)
file(GLOB_RECURSE DEVICE_DRIVER_CHECK_SOURCES CONFIGURE_DEPENDS
    ${REPO_ROOT}/DeviceDrivers/Src/*.c
)
file(GLOB_RECURSE FILTER_CHECK_SOURCES CONFIGURE_DEPENDS
    ${REPO_ROOT}/Filter/*.c
)
file(GLOB_RECURSE POLICY_CHECK_SOURCES CONFIGURE_DEPENDS
    ${REPO_ROOT}/Policy/*.c
)
file(GLOB_RECURSE RTOS_CHECK_SOURCES CONFIGURE_DEPENDS
    ${BOARD_ROOT}/RTOS/Src/*.c
)
file(GLOB_RECURSE SYSTEM_CHECK_SOURCES CONFIGURE_DEPENDS
    ${REPO_ROOT}/System/Src/*.c
)

if(PLATFORM_CHECK_FAMILY STREQUAL "F4")
  add_library(platform_f4_check OBJECT
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
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_adc.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_can.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_crc.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_dac.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_encoder.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_exti.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_flash.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_gpio.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_i2c.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_pwm.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_rng.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_rtc.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_spi.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_uart.c
  )

  target_include_directories(platform_f4_check PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/handles/f4
      ${PLATFORM_COMMON_INCLUDE_DIRS}
      ${REPO_ROOT}/System/Inc
      ${BOARD_ROOT}/Core/Inc
      ${BOARD_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc
      ${BOARD_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
      ${BOARD_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
      ${BOARD_ROOT}/Drivers/CMSIS/Include
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
  set(LAYER_CHECK_FAMILY f4)
  set(LAYER_CHECK_INCLUDE_DIRS
      ${PLATFORM_F4_INCLUDE_DIRS}
      ${BOARD_ROOT}/App/Inc
      ${REPO_ROOT}/Controller/Base/Inc
      ${REPO_ROOT}/Controller/Inc
      ${REPO_ROOT}/DeviceDrivers/Base/Inc
      ${REPO_ROOT}/DeviceDrivers/Inc
      ${REPO_ROOT}/Filter/Base/Inc
      ${REPO_ROOT}/Filter/Inc
      ${REPO_ROOT}/Policy/Base/Inc
      ${REPO_ROOT}/Policy/Inc
      ${BOARD_ROOT}/RTOS/Inc
      ${REPO_ROOT}/System/Inc
      ${BOARD_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/include
      ${BOARD_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
      ${BOARD_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
      ${REPO_ROOT}/Middlewares/Third_Party/SEGGER_RTT/Config
      ${REPO_ROOT}/Middlewares/Third_Party/SEGGER_RTT/RTT
  )
  set(LAYER_CHECK_DEFINITIONS
      ${PLATFORM_F4_DEFINITIONS}
      USE_FREERTOS
      SYS_LOG_USE_RTT=1
  )
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

  set(PLATFORM_F4_BOARD_TIM_DEFINITIONS ${PLATFORM_F4_DEFINITIONS})
  list(REMOVE_ITEM PLATFORM_F4_BOARD_TIM_DEFINITIONS
      PLATFORM_ENABLE_PWM_TIM1=1
      PLATFORM_ENABLE_ENCODER_TIM2=1
      PLATFORM_ENABLE_ENCODER_TIM3=1
      PLATFORM_ENABLE_ENCODER_TIM4=1
      PLATFORM_ENABLE_ENCODER_TIM5=1
  )
  list(APPEND PLATFORM_F4_BOARD_TIM_DEFINITIONS
      PLATFORM_ENABLE_PWM_TIM2=1
      PLATFORM_ENABLE_PWM_TIM3=1
      PLATFORM_ENABLE_PWM_TIM4=1
      PLATFORM_ENABLE_ENCODER_TIM1=1
  )

  add_library(platform_f4_board_tim_check OBJECT
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_encoder.c
      ${REPO_ROOT}/Platform/Src/stm32f4/platform_pwm.c
  )
  target_include_directories(platform_f4_board_tim_check PRIVATE
      ${PLATFORM_F4_INCLUDE_DIRS}
  )
  target_compile_definitions(platform_f4_board_tim_check PRIVATE
      ${PLATFORM_F4_BOARD_TIM_DEFINITIONS}
  )
  target_compile_options(platform_f4_board_tim_check PRIVATE
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
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_adc.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_can.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_crc.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_encoder.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_exti.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_flash.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_gpio.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_i2c.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_pwm.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_rtc.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_spi.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_uart.c
  )

  target_include_directories(platform_f1_check PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/handles/f1
      ${PLATFORM_COMMON_INCLUDE_DIRS}
      ${REPO_ROOT}/System/Inc
      ${BOARD_ROOT}/Core/Inc
      ${BOARD_ROOT}/Drivers/STM32F1xx_HAL_Driver/Inc
      ${BOARD_ROOT}/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
      ${BOARD_ROOT}/Drivers/CMSIS/Device/ST/STM32F1xx/Include
      ${BOARD_ROOT}/Drivers/CMSIS/Include
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
      PLATFORM_ENABLE_RTC=1
      PLATFORM_ENABLE_SPI1=1
      PLATFORM_ENABLE_SPI2=1
      PLATFORM_ENABLE_UART1=1
      PLATFORM_ENABLE_UART2=1
      PLATFORM_ENABLE_UART3=1
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
  set(LAYER_CHECK_FAMILY f1)
  set(LAYER_CHECK_INCLUDE_DIRS
      ${PLATFORM_F1_INCLUDE_DIRS}
      ${BOARD_ROOT}/App/Inc
      ${REPO_ROOT}/Controller/Base/Inc
      ${REPO_ROOT}/Controller/Inc
      ${REPO_ROOT}/DeviceDrivers/Base/Inc
      ${REPO_ROOT}/DeviceDrivers/Inc
      ${REPO_ROOT}/Filter/Base/Inc
      ${REPO_ROOT}/Filter/Inc
      ${REPO_ROOT}/Policy/Base/Inc
      ${REPO_ROOT}/Policy/Inc
      ${BOARD_ROOT}/RTOS/Inc
      ${REPO_ROOT}/System/Inc
      ${BOARD_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/include
      ${BOARD_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
      ${BOARD_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3
      ${REPO_ROOT}/Middlewares/Third_Party/SEGGER_RTT/Config
      ${REPO_ROOT}/Middlewares/Third_Party/SEGGER_RTT/RTT
  )
  set(LAYER_CHECK_DEFINITIONS
      ${PLATFORM_F1_DEFINITIONS}
      USE_FREERTOS
      SYS_LOG_USE_RTT=1
  )
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

  set(PLATFORM_F1_BOARD_TIM_DEFINITIONS ${PLATFORM_F1_DEFINITIONS})
  list(REMOVE_ITEM PLATFORM_F1_BOARD_TIM_DEFINITIONS
      PLATFORM_ENABLE_PWM_TIM1=1
      PLATFORM_ENABLE_ENCODER_TIM2=1
      PLATFORM_ENABLE_ENCODER_TIM3=1
      PLATFORM_ENABLE_ENCODER_TIM4=1
  )
  list(APPEND PLATFORM_F1_BOARD_TIM_DEFINITIONS
      PLATFORM_ENABLE_PWM_TIM2=1
      PLATFORM_ENABLE_PWM_TIM3=1
      PLATFORM_ENABLE_PWM_TIM4=1
      PLATFORM_ENABLE_ENCODER_TIM1=1
  )

  add_library(platform_f1_board_tim_check OBJECT
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_encoder.c
      ${REPO_ROOT}/Platform/Src/stm32f1/platform_pwm.c
  )
  target_include_directories(platform_f1_board_tim_check PRIVATE
      ${PLATFORM_F1_INCLUDE_DIRS}
  )
  target_compile_definitions(platform_f1_board_tim_check PRIVATE
      ${PLATFORM_F1_BOARD_TIM_DEFINITIONS}
  )
  target_compile_options(platform_f1_board_tim_check PRIVATE
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

add_library(layer_${LAYER_CHECK_FAMILY}_check_config INTERFACE)
target_include_directories(layer_${LAYER_CHECK_FAMILY}_check_config INTERFACE
    ${LAYER_CHECK_INCLUDE_DIRS}
)
target_compile_definitions(layer_${LAYER_CHECK_FAMILY}_check_config INTERFACE
    ${LAYER_CHECK_DEFINITIONS}
)

function(add_layer_check layer_name)
  if(ARGC LESS 2)
    message(FATAL_ERROR "编译检查层 ${layer_name} 没有源文件")
  endif()

  set(target_name "${layer_name}_${LAYER_CHECK_FAMILY}_check")
  add_library(${target_name} OBJECT ${ARGN})
  target_link_libraries(
      ${target_name}
      PRIVATE
      layer_${LAYER_CHECK_FAMILY}_check_config
  )
  target_compile_options(${target_name} PRIVATE
      -Wall
      -Wextra
      -Werror
  )
endfunction()

add_layer_check(device_base ${DEVICE_BASE_CHECK_SOURCES})
add_layer_check(device_drivers ${DEVICE_DRIVER_CHECK_SOURCES})
add_layer_check(controller ${CONTROLLER_CHECK_SOURCES})
add_layer_check(policy ${POLICY_CHECK_SOURCES})
add_layer_check(filter ${FILTER_CHECK_SOURCES})
add_layer_check(system ${SYSTEM_CHECK_SOURCES})
add_layer_check(rtos ${RTOS_CHECK_SOURCES})
add_layer_check(app ${APP_CHECK_SOURCES})

foreach(source_group
    APP_CHECK_SOURCES
    CONTROLLER_CHECK_SOURCES
    DEVICE_BASE_CHECK_SOURCES
    DEVICE_DRIVER_CHECK_SOURCES
    FILTER_CHECK_SOURCES
    POLICY_CHECK_SOURCES
    RTOS_CHECK_SOURCES
    SYSTEM_CHECK_SOURCES
)
  list(LENGTH ${source_group} source_count)
  message(STATUS "${source_group}: ${source_count}")
endforeach()

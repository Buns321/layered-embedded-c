cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME layered-embedded-c)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
        Platform/Inc/platform_config.h
        Platform/Inc/platform_crc.h
        Platform/Inc/platform_def.h
        Platform/Inc/platform_iwdg.h
        Platform/Inc/platform_rng.h
        Platform/Inc/platform_wwdg.h
        Platform/Src/platform_hal_internal.h
        Platform/Src/stm32f4/platform_gpio.c
        Platform/Src/stm32f4/platform_crc.c
        Platform/Src/stm32f4/platform_iwdg.c
        Platform/Src/stm32f4/platform_rng.c
        Platform/Src/stm32f4/platform_wwdg.c
        Platform/Inc/platform_gpio.h
        DeviceDrivers/Base/Src/led_base.c
        DeviceDrivers/Base/Inc/led_base.h
        DeviceDrivers/Src/led_gpio.c
        DeviceDrivers/Inc/led_gpio.h
        DeviceDrivers/Src/led_pwm.c
        DeviceDrivers/Inc/led_pwm.h
        App/Src/app_blink.c
        App/Inc/app_blink.h
        System/Inc/system_delay.h
        System/Inc/system_abs.h
        System/Inc/system_math_def.h
        System/Inc/system_minmax.h
        System/Inc/system_clamp.h
        System/Inc/system_sign.h
        System/Inc/system_check.h
        System/Inc/system_math.h
        Platform/Src/stm32f4/platform_pwm.c
        Platform/Inc/platform_pwm.h
        Platform/Inc/platform_tim.h
        DeviceDrivers/Src/motor_tb6612.c
        DeviceDrivers/Inc/motor_tb6612.h
        DeviceDrivers/Base/Src/motor_base.c
        DeviceDrivers/Base/Inc/motor_base.h
        Platform/Src/stm32f4/platform_encoder.c
        Platform/Inc/platform_encoder.h
        Platform/Src/stm32f4/platform_usart.c
        Platform/Inc/platform_usart.h
        App/Src/app_motor_demo.c
        App/Inc/app_motor_demo.h
        DeviceDrivers/Base/Src/usart_base.c
        DeviceDrivers/Base/Inc/usart_base.h
        DeviceDrivers/Src/usart_port.c
        DeviceDrivers/Inc/usart_port.h
        Platform/Src/stm32f4/platform_exti.c
        Platform/Inc/platform_exti.h
        DeviceDrivers/Base/Src/encoder_base.c
        DeviceDrivers/Base/Inc/encoder_base.h
        DeviceDrivers/Src/encoder_tim.c
        DeviceDrivers/Inc/encoder_tim.h
        DeviceDrivers/Base/Inc/chassis_base.h
        DeviceDrivers/Base/Src/chassis_base.c
        DeviceDrivers/Inc/chassis_omni.h
        DeviceDrivers/Src/chassis_omni.c
        RTOS/Src/rtos_shared.c
        RTOS/Inc/rtos_shared.h
        RTOS/Src/rtos_monitor.c
        RTOS/Inc/rtos_monitor.h
        RTOS/Src/rtos_control.c
        RTOS/Inc/rtos_control.h
        DeviceDrivers/Base/Src/imu_base.c
        DeviceDrivers/Base/Inc/imu_base.h
        DeviceDrivers/Src/imu_wht101_usart.c
        DeviceDrivers/Inc/imu_wht101_usart.h
        RTOS/Src/rtos_usart.c
        RTOS/Inc/rtos_usart.h
        Controller/Base/Inc/controller_base.h
        Controller/Base/Src/controller_base.c
        Controller/Inc/controller_linear.h
        Controller/Src/controller_linear.c
        Controller/Inc/controller_compose.h
        Controller/Src/controller_compose.c
        Policy/Base/Inc/omni_policy_base.h
        Policy/Base/Src/omni_policy_base.c
        Policy/Inc/policy_omni_kinematics.h
        Policy/Src/policy_omni_kinematics.c
        Filter/Base/Inc/filter_sample.h
        Filter/Base/Inc/filter_base.h
        Filter/Base/Src/filter_base.c
        Filter/Inc/filter_lowpass.h
        Filter/Src/filter_lowpass.c
        Filter/Inc/filter_chain.h
        Filter/Src/filter_chain.c
        Filter/Inc/filter_distributor.h
        Filter/Src/filter_distributor.c
        Filter/Inc/filter_passthrough.h
        Filter/Src/filter_passthrough.c
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Third-party sources may contain warnings outside this project's ownership.
if(TARGET STM32_Drivers)
    target_compile_options(STM32_Drivers PRIVATE -Wno-unused-parameter)
endif()
if(TARGET FreeRTOS)
    target_compile_options(FreeRTOS PRIVATE -Wno-unused-parameter)
endif()

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Controller/Base/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Controller/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/DeviceDrivers/Base/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/DeviceDrivers/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Filter/Base/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Filter/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Policy/Base/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Policy/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Platform/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Platform/Src
    ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/System/Inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    USE_FREERTOS
    STM32F4xx
    PLATFORM_ENABLE_PWM_TIM1=1
    PLATFORM_ENABLE_ENCODER_TIM2=1
    PLATFORM_ENABLE_ENCODER_TIM3=1
    PLATFORM_ENABLE_ENCODER_TIM4=1
    PLATFORM_ENABLE_ENCODER_TIM5=1
    PLATFORM_ENABLE_UART1=1
    PLATFORM_ENABLE_UART2=1
    PLATFORM_ENABLE_CRC=1
    PLATFORM_ENABLE_RNG=1
    PLATFORM_ENABLE_IWDG=1
    PLATFORM_ENABLE_WWDG=1
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# Optional Doxygen documentation target
find_package(Doxygen QUIET)
if(NOT DOXYGEN_FOUND)
    find_program(DOXYGEN_EXECUTABLE
        NAMES doxygen
        HINTS "$ENV{ProgramFiles}/doxygen/bin" "$ENV{ProgramFiles\(x86\)}/doxygen/bin"
    )
endif()
if(DOXYGEN_EXECUTABLE)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating documentation with Doxygen"
        VERBATIM
    )
endif()

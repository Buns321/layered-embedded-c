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
set(CMAKE_PROJECT_NAME f103c8t6)

# Optional debug services
option(SYS_LOG_USE_RTT "Enable the SEGGER RTT system log backend" ON)
option(PLATFORM_BUILD_I2C "Compile the Platform I2C implementation" OFF)
option(PLATFORM_BUILD_SPI "Compile the Platform SPI implementation" OFF)
option(PLATFORM_BUILD_ADC "Compile the Platform ADC implementation" OFF)
option(PLATFORM_BUILD_CAN "Compile the Platform CAN implementation" OFF)
option(PLATFORM_BUILD_RTC "Compile the Platform RTC implementation" OFF)
option(PLATFORM_BUILD_FLASH "Compile the Platform flash implementation" OFF)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

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
    ${REPO_ROOT}/App/Src/app_blink.c
    ${REPO_ROOT}/App/Src/app_motor_demo.c
    ${REPO_ROOT}/Controller/Base/Src/controller_base.c
    ${REPO_ROOT}/Controller/Src/controller_compose.c
    ${REPO_ROOT}/Controller/Src/controller_linear.c
    ${REPO_ROOT}/DeviceDrivers/Base/Src/chassis_base.c
    ${REPO_ROOT}/DeviceDrivers/Base/Src/encoder_base.c
    ${REPO_ROOT}/DeviceDrivers/Base/Src/imu_base.c
    ${REPO_ROOT}/DeviceDrivers/Base/Src/led_base.c
    ${REPO_ROOT}/DeviceDrivers/Base/Src/motor_base.c
    ${REPO_ROOT}/DeviceDrivers/Base/Src/telemetry_base.c
    ${REPO_ROOT}/DeviceDrivers/Base/Src/usart_base.c
    ${REPO_ROOT}/DeviceDrivers/Src/chassis_omni.c
    ${REPO_ROOT}/DeviceDrivers/Src/firewater_usart.c
    ${REPO_ROOT}/DeviceDrivers/Src/imu_wht101_usart.c
    ${REPO_ROOT}/DeviceDrivers/Src/justfloat_usart.c
    ${REPO_ROOT}/DeviceDrivers/Src/led_gpio.c
    ${REPO_ROOT}/Filter/Base/Src/filter_base.c
    ${REPO_ROOT}/Filter/Src/filter_chain.c
    ${REPO_ROOT}/Filter/Src/filter_distributor.c
    ${REPO_ROOT}/Filter/Src/filter_lowpass.c
    ${REPO_ROOT}/Filter/Src/filter_passthrough.c
    ${REPO_ROOT}/Platform/Src/stm32f1/platform_crc.c
    ${REPO_ROOT}/Platform/Src/stm32f1/platform_exti.c
    ${REPO_ROOT}/Platform/Src/stm32f1/platform_gpio.c
    ${REPO_ROOT}/Platform/Src/stm32f1/platform_iwdg.c
    ${REPO_ROOT}/Platform/Src/stm32f1/platform_wwdg.c
    ${REPO_ROOT}/Policy/Base/Src/omni_policy_base.c
    ${REPO_ROOT}/Policy/Src/policy_omni_kinematics.c
    ${REPO_ROOT}/RTOS/Src/rtos_control.c
    ${REPO_ROOT}/RTOS/Src/rtos_monitor.c
    ${REPO_ROOT}/RTOS/Src/rtos_shared.c
    ${REPO_ROOT}/RTOS/Src/rtos_usart.c
    ${REPO_ROOT}/System/Src/system_critical.c
    ${REPO_ROOT}/System/Src/system_fault.c
    ${REPO_ROOT}/System/Src/system_log_base.c
    ${REPO_ROOT}/System/Src/system_log.c
    ${REPO_ROOT}/System/Src/system_watchdog.c
)

if(PLATFORM_BUILD_I2C)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/Platform/Src/stm32f1/platform_i2c.c
        ${REPO_ROOT}/DeviceDrivers/Src/mpu6050_i2c.c
    )
endif()
if(PLATFORM_BUILD_SPI)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/Platform/Src/stm32f1/platform_spi.c
    )
endif()
if(PLATFORM_BUILD_ADC)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/Platform/Src/stm32f1/platform_adc.c
    )
endif()
if(PLATFORM_BUILD_CAN)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/Platform/Src/stm32f1/platform_can.c
    )
endif()
if(PLATFORM_BUILD_RTC)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/Platform/Src/stm32f1/platform_rtc.c
    )
endif()
if(PLATFORM_BUILD_FLASH)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/Platform/Src/stm32f1/platform_flash.c
    )
endif()

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${REPO_ROOT}/App/Inc
    ${REPO_ROOT}/Controller/Base/Inc
    ${REPO_ROOT}/Controller/Inc
    ${REPO_ROOT}/DeviceDrivers/Base/Inc
    ${REPO_ROOT}/DeviceDrivers/Inc
    ${REPO_ROOT}/Filter/Base/Inc
    ${REPO_ROOT}/Filter/Inc
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
    ${REPO_ROOT}/Policy/Base/Inc
    ${REPO_ROOT}/Policy/Inc
    ${REPO_ROOT}/RTOS/Inc
    ${REPO_ROOT}/System/Inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    USE_FREERTOS
    STM32F1xx
    PLATFORM_ENABLE_GPIO_D=1
    PLATFORM_ENABLE_GPIO_E=0
    PLATFORM_ENABLE_CRC=1
    PLATFORM_ENABLE_IWDG=1
    PLATFORM_ENABLE_WWDG=1
)

if(SYS_LOG_USE_RTT)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/System/Src/system_log_rtt.c
        ${REPO_ROOT}/Middlewares/Third_Party/SEGGER_RTT/RTT/SEGGER_RTT.c
    )

    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/Middlewares/Third_Party/SEGGER_RTT/Config
        ${REPO_ROOT}/Middlewares/Third_Party/SEGGER_RTT/RTT
    )

    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        SYS_LOG_USE_RTT=1
    )
endif()

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

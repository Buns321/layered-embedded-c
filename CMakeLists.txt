cmake_minimum_required(VERSION 3.22)

#
# 本文件只在首次生成时创建，
# 多次调用转换器也不会重新生成。
#
# 用户可以根据需要自由修改本文件。
#

# 配置编译器选项
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# 定义构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# 设置工程名称
set(CMAKE_PROJECT_NAME layered-embedded-c)

# 可选调试服务
option(SYS_LOG_USE_RTT "启用 SEGGER RTT System 日志后端" ON)
option(PLATFORM_BUILD_I2C "编译 Platform I2C 实现" OFF)
option(PLATFORM_BUILD_SPI "编译 Platform SPI 实现" OFF)
option(PLATFORM_BUILD_ADC "编译 Platform ADC 实现" OFF)
option(PLATFORM_BUILD_DAC "编译 Platform DAC 实现" OFF)
option(PLATFORM_BUILD_CAN "编译 Platform CAN 实现" OFF)
option(PLATFORM_BUILD_RTC "编译 Platform RTC 实现" OFF)
option(PLATFORM_BUILD_FLASH "编译 Platform Flash 实现" OFF)

# 生成编译命令，便于 clangd 等工具建立索引
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# 工程核心设置
project(${CMAKE_PROJECT_NAME})
message("构建类型: " ${CMAKE_BUILD_TYPE})

# 启用 CMake 对 C 和 ASM 语言的支持
enable_language(C ASM)

# 创建可执行目标
add_executable(${CMAKE_PROJECT_NAME}
        Platform/Inc/platform_config.h
        Platform/Inc/platform_crc.h
        Platform/Inc/platform_adc.h
        Platform/Inc/platform_can.h
        Platform/Inc/platform_dac.h
        Platform/Inc/platform_def.h
        Platform/Inc/platform_flash.h
        Platform/Inc/platform_i2c.h
        Platform/Inc/platform_iwdg.h
        Platform/Inc/platform_rng.h
        Platform/Inc/platform_rtc.h
        Platform/Inc/platform_spi.h
        Platform/Inc/platform_wwdg.h
        Platform/Src/platform_hal_internal.h
        Platform/Src/stm32f4/platform_gpio.c
        Platform/Src/common/platform_gpio_common.c
        Platform/Src/common/platform_exti_common.c
        Platform/Src/common/platform_crc_common.c
        Platform/Src/common/platform_iwdg_common.c
        Platform/Src/common/platform_rng_common.c
        Platform/Src/common/platform_encoder_common.c
        Platform/Src/common/platform_pwm_common.c
        Platform/Src/common/platform_wwdg_common.c
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
        System/Inc/system_critical.h
        System/Src/system_critical.c
        System/Inc/system_fault.h
        System/Src/system_fault.c
        System/Inc/system_watchdog.h
        System/Src/system_watchdog.c
        System/Inc/system_log_base.h
        System/Src/system_log_base.c
        System/Inc/system_log.h
        System/Src/system_log.c
        Platform/Src/stm32f4/platform_pwm.c
        Platform/Inc/platform_pwm.h
        Platform/Inc/platform_tim.h
        DeviceDrivers/Src/motor_tb6612.c
        DeviceDrivers/Inc/motor_tb6612.h
        DeviceDrivers/Base/Src/motor_base.c
        DeviceDrivers/Base/Inc/motor_base.h
        Platform/Src/stm32f4/platform_encoder.c
        Platform/Inc/platform_encoder.h
        Platform/Src/common/platform_uart_common.c
        Platform/Src/stm32f4/platform_uart.c
        Platform/Inc/platform_uart.h
        App/Src/app_motor_demo.c
        App/Inc/app_motor_demo.h
        DeviceDrivers/Base/Src/uart_base.c
        DeviceDrivers/Base/Inc/uart_base.h
        DeviceDrivers/Src/uart_port.c
        DeviceDrivers/Inc/uart_port.h
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
        DeviceDrivers/Base/Inc/telemetry_base.h
        DeviceDrivers/Base/Src/telemetry_base.c
        DeviceDrivers/Inc/firewater_uart.h
        DeviceDrivers/Src/firewater_uart.c
        DeviceDrivers/Inc/justfloat_uart.h
        DeviceDrivers/Src/justfloat_uart.c
        RTOS/Src/rtos_shared.c
        RTOS/Inc/rtos_shared.h
        RTOS/Src/rtos_monitor.c
        RTOS/Inc/rtos_monitor.h
        RTOS/Src/rtos_control.c
        RTOS/Inc/rtos_control.h
        DeviceDrivers/Base/Src/imu_base.c
        DeviceDrivers/Base/Inc/imu_base.h
        DeviceDrivers/Src/imu_wht101_uart.c
        DeviceDrivers/Inc/imu_wht101_uart.h
        DeviceDrivers/Inc/mpu6050_i2c.h
        RTOS/Src/rtos_uart.c
        RTOS/Inc/rtos_uart.h
        Controller/Base/Inc/controller_base.h
        Controller/Base/Src/controller_base.c
        Controller/Inc/controller_linear.h
        Controller/Src/controller_linear.c
        Controller/Inc/controller_compose.h
        Controller/Src/controller_compose.c
        Policy/Base/Inc/policy_omni_base.h
        Policy/Base/Src/policy_omni_base.c
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

if(PLATFORM_BUILD_I2C)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        Platform/Src/common/platform_i2c_common.c
        Platform/Src/stm32f4/platform_i2c.c
        DeviceDrivers/Src/mpu6050_i2c.c
    )
endif()
if(PLATFORM_BUILD_SPI)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        Platform/Src/common/platform_spi_common.c
        Platform/Src/stm32f4/platform_spi.c
    )
endif()
if(PLATFORM_BUILD_ADC)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        Platform/Src/common/platform_adc_common.c
        Platform/Src/stm32f4/platform_adc.c
    )
endif()
if(PLATFORM_BUILD_DAC)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        Platform/Src/common/platform_dac_common.c
        Platform/Src/stm32f4/platform_dac.c
    )
endif()
if(PLATFORM_BUILD_CAN)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        Platform/Src/common/platform_can_common.c
        Platform/Src/stm32f4/platform_can.c
    )
endif()
if(PLATFORM_BUILD_RTC)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        Platform/Src/common/platform_rtc_common.c
        Platform/Src/stm32f4/platform_rtc.c
    )
endif()
if(PLATFORM_BUILD_FLASH)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        Platform/Src/common/platform_flash_common.c
        Platform/Src/stm32f4/platform_flash.c
    )
endif()

# 添加 STM32CubeMX 生成的源文件
add_subdirectory(cmake/stm32cubemx)

# 第三方源码可能产生本项目无法控制的告警。
if(TARGET STM32_Drivers)
    target_compile_options(STM32_Drivers PRIVATE -Wno-unused-parameter)
endif()
if(TARGET FreeRTOS)
    target_compile_options(FreeRTOS PRIVATE -Wno-unused-parameter)
endif()

# 配置链接目录
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # 在此添加用户自定义库搜索路径
)

# 向可执行目标添加源文件
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # 在此添加用户源文件
)

# 添加头文件搜索路径
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

# 添加工程符号（宏）
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
    # 在此添加用户自定义符号
)

if(SYS_LOG_USE_RTT)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        System/Inc/system_log_rtt.h
        System/Src/system_log_rtt.c
        Middlewares/Third_Party/SEGGER_RTT/RTT/SEGGER_RTT.c
        Middlewares/Third_Party/SEGGER_RTT/RTT/SEGGER_RTT.h
        Middlewares/Third_Party/SEGGER_RTT/RTT/SEGGER_RTT_ConfDefaults.h
    )

    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/SEGGER_RTT/Config
        ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/SEGGER_RTT/RTT
    )

    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        SYS_LOG_USE_RTT=1
    )
endif()

# 使用 C++ 文件时移除错误的 libob.a 库依赖
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# 添加链接库
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # 在此添加用户自定义库
)

# 可选的 Doxygen 文档目标
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

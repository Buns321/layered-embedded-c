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
set(CMAKE_PROJECT_NAME f407vet6)

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
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
message("构建类型: " ${CMAKE_BUILD_TYPE})

# 启用 CMake 对 C 和 ASM 语言的支持
enable_language(C ASM)

# 创建可执行目标
add_executable(${CMAKE_PROJECT_NAME}
        ${REPO_ROOT}/Platform/Inc/platform_config.h
        ${REPO_ROOT}/Platform/Inc/platform_crc.h
        ${REPO_ROOT}/Platform/Inc/platform_adc.h
        ${REPO_ROOT}/Platform/Inc/platform_can.h
        ${REPO_ROOT}/Platform/Inc/platform_dac.h
        ${REPO_ROOT}/Platform/Inc/platform_def.h
        ${REPO_ROOT}/Platform/Inc/platform_flash.h
        ${REPO_ROOT}/Platform/Inc/platform_i2c.h
        ${REPO_ROOT}/Platform/Inc/platform_iwdg.h
        ${REPO_ROOT}/Platform/Inc/platform_rng.h
        ${REPO_ROOT}/Platform/Inc/platform_rtc.h
        ${REPO_ROOT}/Platform/Inc/platform_spi.h
        ${REPO_ROOT}/Platform/Inc/platform_wwdg.h
        ${REPO_ROOT}/Platform/Src/platform_hal_internal.h
        ${REPO_ROOT}/Platform/Src/stm32f4/platform_gpio.c
        ${REPO_ROOT}/Platform/Src/common/platform_gpio_common.c
        ${REPO_ROOT}/Platform/Src/common/platform_exti_common.c
        ${REPO_ROOT}/Platform/Src/common/platform_crc_common.c
        ${REPO_ROOT}/Platform/Src/common/platform_iwdg_common.c
        ${REPO_ROOT}/Platform/Src/common/platform_rng_common.c
        ${REPO_ROOT}/Platform/Src/common/platform_encoder_common.c
        ${REPO_ROOT}/Platform/Src/common/platform_pwm_common.c
        ${REPO_ROOT}/Platform/Src/common/platform_wwdg_common.c
        ${REPO_ROOT}/Platform/Src/stm32f4/platform_crc.c
        ${REPO_ROOT}/Platform/Src/stm32f4/platform_iwdg.c
        ${REPO_ROOT}/Platform/Src/stm32f4/platform_rng.c
        ${REPO_ROOT}/Platform/Src/stm32f4/platform_wwdg.c
        ${REPO_ROOT}/Platform/Inc/platform_gpio.h
        ${REPO_ROOT}/DeviceDrivers/Base/Src/led_base.c
        ${REPO_ROOT}/DeviceDrivers/Base/Inc/led_base.h
        ${REPO_ROOT}/DeviceDrivers/Src/led_gpio.c
        ${REPO_ROOT}/DeviceDrivers/Inc/led_gpio.h
        ${REPO_ROOT}/DeviceDrivers/Src/led_pwm.c
        ${REPO_ROOT}/DeviceDrivers/Inc/led_pwm.h
        ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_blink.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc/app_blink.h
        ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_car.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc/app_car.h
        ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc/app_car_config.h
        ${REPO_ROOT}/System/Inc/system_delay.h
        ${REPO_ROOT}/System/Inc/system_abs.h
        ${REPO_ROOT}/System/Inc/system_math_def.h
        ${REPO_ROOT}/System/Inc/system_minmax.h
        ${REPO_ROOT}/System/Inc/system_clamp.h
        ${REPO_ROOT}/System/Inc/system_sign.h
        ${REPO_ROOT}/System/Inc/system_check.h
        ${REPO_ROOT}/System/Inc/system_math.h
        ${REPO_ROOT}/System/Inc/system_critical.h
        ${REPO_ROOT}/System/Src/system_critical.c
        ${REPO_ROOT}/System/Inc/system_fault.h
        ${REPO_ROOT}/System/Src/system_fault.c
        ${REPO_ROOT}/System/Inc/system_watchdog.h
        ${REPO_ROOT}/System/Src/system_watchdog.c
        ${REPO_ROOT}/System/Inc/system_log_base.h
        ${REPO_ROOT}/System/Src/system_log_base.c
        ${REPO_ROOT}/System/Inc/system_log.h
        ${REPO_ROOT}/System/Src/system_log.c
        ${REPO_ROOT}/Platform/Src/stm32f4/platform_pwm.c
        ${REPO_ROOT}/Platform/Inc/platform_pwm.h
        ${REPO_ROOT}/Platform/Inc/platform_tim.h
        ${REPO_ROOT}/DeviceDrivers/Src/motor_tb6612.c
        ${REPO_ROOT}/DeviceDrivers/Inc/motor_tb6612.h
        ${REPO_ROOT}/DeviceDrivers/Base/Src/motor_base.c
        ${REPO_ROOT}/DeviceDrivers/Base/Inc/motor_base.h
        ${REPO_ROOT}/Platform/Src/stm32f4/platform_encoder.c
        ${REPO_ROOT}/Platform/Inc/platform_encoder.h
        ${REPO_ROOT}/Platform/Src/common/platform_uart_common.c
        ${REPO_ROOT}/Platform/Src/stm32f4/platform_uart.c
        ${REPO_ROOT}/Platform/Inc/platform_uart.h
        ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_motor_demo.c
        ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc/app_motor_demo.h
        ${REPO_ROOT}/DeviceDrivers/Base/Src/uart_base.c
        ${REPO_ROOT}/DeviceDrivers/Base/Inc/uart_base.h
        ${REPO_ROOT}/DeviceDrivers/Src/uart_port.c
        ${REPO_ROOT}/DeviceDrivers/Inc/uart_port.h
        ${REPO_ROOT}/Platform/Src/stm32f4/platform_exti.c
        ${REPO_ROOT}/Platform/Inc/platform_exti.h
        ${REPO_ROOT}/DeviceDrivers/Base/Src/encoder_base.c
        ${REPO_ROOT}/DeviceDrivers/Base/Inc/encoder_base.h
        ${REPO_ROOT}/DeviceDrivers/Src/encoder_tim.c
        ${REPO_ROOT}/DeviceDrivers/Inc/encoder_tim.h
        ${REPO_ROOT}/DeviceDrivers/Base/Inc/chassis_base.h
        ${REPO_ROOT}/DeviceDrivers/Base/Src/chassis_base.c
        ${REPO_ROOT}/DeviceDrivers/Base/Inc/joycon_base.h
        ${REPO_ROOT}/DeviceDrivers/Base/Src/joycon_base.c
        ${REPO_ROOT}/DeviceDrivers/Inc/chassis_omni.h
        ${REPO_ROOT}/DeviceDrivers/Src/chassis_omni.c
        ${REPO_ROOT}/DeviceDrivers/Base/Inc/telemetry_base.h
        ${REPO_ROOT}/DeviceDrivers/Base/Src/telemetry_base.c
        ${REPO_ROOT}/DeviceDrivers/Inc/firewater_uart.h
        ${REPO_ROOT}/DeviceDrivers/Src/firewater_uart.c
        ${REPO_ROOT}/DeviceDrivers/Inc/justfloat_uart.h
        ${REPO_ROOT}/DeviceDrivers/Src/justfloat_uart.c
        ${REPO_ROOT}/DeviceDrivers/Inc/joycon_uart.h
        ${REPO_ROOT}/DeviceDrivers/Src/joycon_uart.c
        ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Src/rtos_shared.c
        ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc/rtos_shared.h
        ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc/rtos_config.h
        ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Src/rtos_monitor.c
        ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc/rtos_monitor.h
        ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Src/rtos_control.c
        ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc/rtos_control.h
        ${REPO_ROOT}/DeviceDrivers/Base/Src/imu_base.c
        ${REPO_ROOT}/DeviceDrivers/Base/Inc/imu_base.h
        ${REPO_ROOT}/DeviceDrivers/Src/imu_wht101_uart.c
        ${REPO_ROOT}/DeviceDrivers/Inc/imu_wht101_uart.h
        ${REPO_ROOT}/DeviceDrivers/Inc/mpu6050_i2c.h
        ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Src/rtos_uart.c
        ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc/rtos_uart.h
        ${REPO_ROOT}/Controller/Base/Inc/controller_base.h
        ${REPO_ROOT}/Controller/Base/Src/controller_base.c
        ${REPO_ROOT}/Controller/Inc/controller_linear.h
        ${REPO_ROOT}/Controller/Src/controller_linear.c
        ${REPO_ROOT}/Controller/Inc/controller_compose.h
        ${REPO_ROOT}/Controller/Src/controller_compose.c
        ${REPO_ROOT}/Controller/Inc/controller_pid.h
        ${REPO_ROOT}/Controller/Src/controller_pid.c
        ${REPO_ROOT}/Policy/Base/Inc/policy_omni_base.h
        ${REPO_ROOT}/Policy/Base/Src/policy_omni_base.c
        ${REPO_ROOT}/Policy/Inc/policy_omni_kinematics.h
        ${REPO_ROOT}/Policy/Src/policy_omni_kinematics.c
        ${REPO_ROOT}/Filter/Base/Inc/filter_sample.h
        ${REPO_ROOT}/Filter/Base/Inc/filter_base.h
        ${REPO_ROOT}/Filter/Base/Src/filter_base.c
        ${REPO_ROOT}/Filter/Inc/filter_lowpass.h
        ${REPO_ROOT}/Filter/Src/filter_lowpass.c
        ${REPO_ROOT}/Filter/Inc/filter_chain.h
        ${REPO_ROOT}/Filter/Src/filter_chain.c
        ${REPO_ROOT}/Filter/Inc/filter_distributor.h
        ${REPO_ROOT}/Filter/Src/filter_distributor.c
        ${REPO_ROOT}/Filter/Inc/filter_passthrough.h
        ${REPO_ROOT}/Filter/Src/filter_passthrough.c
)

if(PLATFORM_BUILD_I2C)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/Platform/Src/common/platform_i2c_common.c
        ${REPO_ROOT}/Platform/Src/stm32f4/platform_i2c.c
        ${REPO_ROOT}/DeviceDrivers/Src/mpu6050_i2c.c
    )
endif()
if(PLATFORM_BUILD_SPI)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/Platform/Src/common/platform_spi_common.c
        ${REPO_ROOT}/Platform/Src/stm32f4/platform_spi.c
    )
endif()
if(PLATFORM_BUILD_ADC)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/Platform/Src/common/platform_adc_common.c
        ${REPO_ROOT}/Platform/Src/stm32f4/platform_adc.c
    )
endif()
if(PLATFORM_BUILD_DAC)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/Platform/Src/common/platform_dac_common.c
        ${REPO_ROOT}/Platform/Src/stm32f4/platform_dac.c
    )
endif()
if(PLATFORM_BUILD_CAN)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/Platform/Src/common/platform_can_common.c
        ${REPO_ROOT}/Platform/Src/stm32f4/platform_can.c
    )
endif()
if(PLATFORM_BUILD_RTC)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/Platform/Src/common/platform_rtc_common.c
        ${REPO_ROOT}/Platform/Src/stm32f4/platform_rtc.c
    )
endif()
if(PLATFORM_BUILD_FLASH)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/Platform/Src/common/platform_flash_common.c
        ${REPO_ROOT}/Platform/Src/stm32f4/platform_flash.c
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
    ${REPO_ROOT}/Controller/Base/Inc
    ${REPO_ROOT}/Controller/Inc
    ${REPO_ROOT}/DeviceDrivers/Base/Inc
    ${REPO_ROOT}/DeviceDrivers/Inc
    ${REPO_ROOT}/Filter/Base/Inc
    ${REPO_ROOT}/Filter/Inc
    ${REPO_ROOT}/Policy/Base/Inc
    ${REPO_ROOT}/Policy/Inc
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
    ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc
    ${REPO_ROOT}/System/Inc
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
    PLATFORM_ENABLE_UART3=1
    PLATFORM_ENABLE_CRC=1
    PLATFORM_ENABLE_RNG=1
    PLATFORM_ENABLE_IWDG=1
    PLATFORM_ENABLE_WWDG=1
    # 在此添加用户自定义符号
)

if(SYS_LOG_USE_RTT)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/System/Inc/system_log_rtt.h
        ${REPO_ROOT}/System/Src/system_log_rtt.c
        ${REPO_ROOT}/Middlewares/Third_Party/SEGGER_RTT/RTT/SEGGER_RTT.c
        ${REPO_ROOT}/Middlewares/Third_Party/SEGGER_RTT/RTT/SEGGER_RTT.h
        ${REPO_ROOT}/Middlewares/Third_Party/SEGGER_RTT/RTT/SEGGER_RTT_ConfDefaults.h
    )

    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        ${REPO_ROOT}/Middlewares/Third_Party/SEGGER_RTT/Config
        ${REPO_ROOT}/Middlewares/Third_Party/SEGGER_RTT/RTT
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
        COMMAND ${DOXYGEN_EXECUTABLE} ${REPO_ROOT}/doc/doxygen/Doxyfile
        WORKING_DIRECTORY ${REPO_ROOT}
        COMMENT "Generating documentation with Doxygen"
        VERBATIM
    )
endif()

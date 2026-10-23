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
set(CMAKE_PROJECT_NAME f103c8t6)

# 可选调试服务
option(SYS_LOG_USE_RTT "启用 SEGGER RTT System 日志后端" ON)
option(PLATFORM_BUILD_I2C "编译 Platform I2C 实现" OFF)
option(PLATFORM_BUILD_SPI "编译 Platform SPI 实现" OFF)
option(PLATFORM_BUILD_ADC "编译 Platform ADC 实现" OFF)
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
add_executable(${CMAKE_PROJECT_NAME})

# 添加 STM32CubeMX 生成的源文件
add_subdirectory(cmake/stm32cubemx)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

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

# 添加头文件搜索路径
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

# 添加工程符号（宏）
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

# 使用 C++ 文件时移除错误的 libob.a 库依赖
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# 添加链接库
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # 在此添加用户自定义库
)

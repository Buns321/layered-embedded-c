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

add_library(layered_board_config INTERFACE)
target_compile_definitions(layered_board_config INTERFACE
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
)

# 启用 CMake 对 C 和 ASM 语言的支持
enable_language(C ASM)

# 创建可执行目标
add_executable(${CMAKE_PROJECT_NAME})

# 添加 STM32CubeMX 生成的源文件
add_subdirectory(cmake/stm32cubemx)

set(LAYERED_PLATFORM_FEATURES
    gpio
    exti
    crc
    rng
    pwm
    encoder
    uart
)
if(PLATFORM_BUILD_I2C)
    list(APPEND LAYERED_PLATFORM_FEATURES i2c)
endif()
if(PLATFORM_BUILD_SPI)
    list(APPEND LAYERED_PLATFORM_FEATURES spi)
endif()
if(PLATFORM_BUILD_ADC)
    list(APPEND LAYERED_PLATFORM_FEATURES adc)
endif()
if(PLATFORM_BUILD_DAC)
    list(APPEND LAYERED_PLATFORM_FEATURES dac)
endif()
if(PLATFORM_BUILD_CAN)
    list(APPEND LAYERED_PLATFORM_FEATURES can)
endif()
if(PLATFORM_BUILD_RTC)
    list(APPEND LAYERED_PLATFORM_FEATURES rtc)
endif()
if(PLATFORM_BUILD_FLASH)
    list(APPEND LAYERED_PLATFORM_FEATURES flash)
endif()
list(REMOVE_DUPLICATES LAYERED_PLATFORM_FEATURES)

set(LAYERED_PLATFORM_BACKEND stm32f4)
set(LAYERED_HAL_TARGET stm32cubemx)
add_subdirectory(${REPO_ROOT}/cmake/framework framework)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_blink.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc/app_blink.h
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_car.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc/app_car.h
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc/app_car_config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_motor_demo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc/app_motor_demo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Src/rtos_shared.c
    ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc/rtos_shared.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc/rtos_config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Src/rtos_monitor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc/rtos_monitor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Src/rtos_control.c
    ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc/rtos_control.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Src/rtos_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc/rtos_uart.h
)

# 添加头文件搜索路径
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc
)

# 使用 C++ 文件时移除错误的 libob.a 库依赖
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# 添加链接库
target_link_libraries(${CMAKE_PROJECT_NAME}
    layered_framework
    stm32cubemx
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

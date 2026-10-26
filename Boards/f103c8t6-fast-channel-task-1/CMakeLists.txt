cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_PROJECT_NAME f103c8t6-fast-channel-task-1)
option(SYS_LOG_USE_RTT "启用 SEGGER RTT System 日志后端" ON)

project(${CMAKE_PROJECT_NAME} C ASM)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

add_executable(${CMAKE_PROJECT_NAME})
add_subdirectory(cmake/stm32cubemx)

if(TARGET STM32_Drivers)
  target_compile_options(STM32_Drivers PRIVATE -Os -Wno-unused-parameter)
endif()
if(TARGET FreeRTOS)
  target_compile_options(FreeRTOS PRIVATE -Os -Wno-unused-parameter)
endif()

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Os -Wall -Wextra)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_fast_channel.c
  ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_calibration.c
  ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_command_parser.c
  ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_uart_link.c
  ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Src/rtos_app.c

  ${REPO_ROOT}/DeviceDrivers/Base/Src/imu_base.c
  ${REPO_ROOT}/DeviceDrivers/Base/Src/telemetry_base.c
  ${REPO_ROOT}/DeviceDrivers/Base/Src/uart_base.c
  ${REPO_ROOT}/DeviceDrivers/Src/justfloat_uart.c
  ${REPO_ROOT}/DeviceDrivers/Src/mpu6050_i2c.c
  ${REPO_ROOT}/DeviceDrivers/Src/uart_port.c

  ${REPO_ROOT}/Filter/Base/Src/filter_base.c
  ${REPO_ROOT}/Filter/Src/filter_chain.c
  ${REPO_ROOT}/Filter/Src/filter_ekf.c
  ${REPO_ROOT}/Filter/Src/filter_imu_calibration.c

  ${REPO_ROOT}/Platform/Src/common/platform_gpio_common.c
  ${REPO_ROOT}/Platform/Src/common/platform_i2c_common.c
  ${REPO_ROOT}/Platform/Src/common/platform_flash_common.c
  ${REPO_ROOT}/Platform/Src/common/platform_uart_common.c
  ${REPO_ROOT}/Platform/Src/stm32f1/platform_flash.c
  ${REPO_ROOT}/Platform/Src/stm32f1/platform_gpio.c
  ${REPO_ROOT}/Platform/Src/stm32f1/platform_i2c.c
  ${REPO_ROOT}/Platform/Src/stm32f1/platform_uart.c

  ${REPO_ROOT}/System/Src/system_critical.c
  ${REPO_ROOT}/System/Src/system_fault.c
  ${REPO_ROOT}/System/Src/system_log.c
  ${REPO_ROOT}/System/Src/system_log_base.c
  ${REPO_ROOT}/System/Src/system_record_store.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc
  ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc
  ${REPO_ROOT}/DeviceDrivers/Base/Inc
  ${REPO_ROOT}/DeviceDrivers/Inc
  ${REPO_ROOT}/Filter/Base/Inc
  ${REPO_ROOT}/Filter/Inc
  ${REPO_ROOT}/Platform/Inc
  ${REPO_ROOT}/Platform/Src
  ${REPO_ROOT}/System/Inc
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
  USE_FREERTOS
  STM32F1xx
  TELEMETRY_CHANNEL_CAPACITY=32
  PLATFORM_ENABLE_GPIO_D=0
  PLATFORM_ENABLE_GPIO_E=0
  PLATFORM_ENABLE_UART1=1
  PLATFORM_ENABLE_I2C1=1
  PLATFORM_ENABLE_FLASH=1
  PLATFORM_ENABLE_CRC=1
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

list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

target_link_libraries(${CMAKE_PROJECT_NAME}
  stm32cubemx
  m
)

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

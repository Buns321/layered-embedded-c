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

add_library(layered_board_config INTERFACE)
target_compile_definitions(layered_board_config INTERFACE
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

set(LAYERED_PLATFORM_BACKEND stm32f1)
set(LAYERED_HAL_TARGET stm32cubemx)
set(LAYERED_FRAMEWORK_OPTIMIZATION -Os)
set(LAYERED_PLATFORM_FEATURES
  gpio
  crc
  i2c
  flash
  uart
)
add_subdirectory(${REPO_ROOT}/cmake/framework framework)

if(TARGET STM32_Drivers)
  target_compile_options(STM32_Drivers PRIVATE -Os -Wno-unused-parameter)
endif()
if(TARGET FreeRTOS)
  target_compile_options(FreeRTOS PRIVATE -Os -Wno-unused-parameter)
endif()

target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Os -Wall -Wextra)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_fast_channel.c
  ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_fast_channel_telemetry.c
  ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_attitude_reference.c
  ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_calibration_flow.c
  ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_calibration_record.c
  ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_imu_session.c
  ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_calibration.c
  ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_command_parser.c
  ${CMAKE_CURRENT_SOURCE_DIR}/App/Src/app_status_indicator.c
  ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Src/rtos_app.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/App/Inc
  ${CMAKE_CURRENT_SOURCE_DIR}/RTOS/Inc
)

list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

target_link_libraries(${CMAKE_PROJECT_NAME}
  layered_framework
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

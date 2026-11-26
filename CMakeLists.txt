cmake_minimum_required(VERSION 3.22)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_HOST_TESTS "构建宿主机 CTest 单元测试" OFF)
option(SYS_LOG_USE_RTT  "启用 SEGGER RTT 日志后端" ON)

if(BUILD_HOST_TESTS)
  project(layered_car_tests C)
  enable_testing()
  add_subdirectory(tests)
  return()
endif()

set(CMAKE_PROJECT_NAME f407vet6)
project(${CMAKE_PROJECT_NAME} C ASM)
add_executable(${PROJECT_NAME})
add_subdirectory(cmake/stm32cubemx)
include(cmake/car_framework.cmake)

car_configure_board(
  TARGET        ${PROJECT_NAME}
  MCU_FAMILY    STM32F4xx
  HAL_TARGET    stm32cubemx
  USE_FREERTOS  ON
  USE_RTT       ${SYS_LOG_USE_RTT}
  APP_SOURCES   User/app_car.c
  BOARD_SOURCES User/board_cfg.c
  RTOS_SOURCES  User/rtos_tasks.c
  USER_INCLUDES User
)

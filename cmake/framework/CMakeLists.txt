cmake_minimum_required(VERSION 3.22)

# Framework 是板级工程的构建子图，不单独创建 project。
get_filename_component(LAYERED_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if(NOT TARGET layered_board_config)
  message(FATAL_ERROR "layered_board_config target is required before adding Framework")
endif()

if(NOT DEFINED LAYERED_PLATFORM_BACKEND)
  message(FATAL_ERROR "LAYERED_PLATFORM_BACKEND must be stm32f1 or stm32f4")
endif()

if(NOT DEFINED LAYERED_HAL_TARGET)
  set(LAYERED_HAL_TARGET stm32cubemx)
endif()

if(NOT TARGET ${LAYERED_HAL_TARGET})
  message(FATAL_ERROR "HAL target ${LAYERED_HAL_TARGET} does not exist")
endif()

if(NOT DEFINED LAYERED_PLATFORM_FEATURES)
  message(FATAL_ERROR "LAYERED_PLATFORM_FEATURES must list enabled Platform features")
endif()

string(TOLOWER "${LAYERED_PLATFORM_BACKEND}" LAYERED_PLATFORM_BACKEND)

set(LAYERED_FRAMEWORK_COMPILE_OPTIONS)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  list(APPEND LAYERED_FRAMEWORK_COMPILE_OPTIONS -Wall -Wextra)
endif()
if(DEFINED LAYERED_FRAMEWORK_OPTIMIZATION)
  list(APPEND LAYERED_FRAMEWORK_COMPILE_OPTIONS
    ${LAYERED_FRAMEWORK_OPTIMIZATION}
  )
endif()

function(layered_add_framework_library target_name)
  add_library(${target_name} STATIC ${ARGN})
  if(LAYERED_FRAMEWORK_COMPILE_OPTIONS)
    target_compile_options(${target_name} PRIVATE
      ${LAYERED_FRAMEWORK_COMPILE_OPTIONS}
    )
  endif()
endfunction()

add_library(layered_platform_contract INTERFACE)
target_include_directories(layered_platform_contract INTERFACE
  ${LAYERED_REPO_ROOT}/Platform/Inc
)
target_link_libraries(layered_platform_contract INTERFACE
  layered_board_config
)

if(SYS_LOG_USE_RTT)
  layered_add_framework_library(layered_segger_rtt
    ${LAYERED_REPO_ROOT}/Middlewares/Third_Party/SEGGER_RTT/RTT/SEGGER_RTT.c
  )
  target_include_directories(layered_segger_rtt PUBLIC
    ${LAYERED_REPO_ROOT}/Middlewares/Third_Party/SEGGER_RTT/Config
    ${LAYERED_REPO_ROOT}/Middlewares/Third_Party/SEGGER_RTT/RTT
  )
endif()

layered_add_framework_library(layered_system_base
  ${LAYERED_REPO_ROOT}/System/Src/system_angle.c
  ${LAYERED_REPO_ROOT}/System/Src/system_byte_ring.c
  ${LAYERED_REPO_ROOT}/System/Src/system_critical.c
  ${LAYERED_REPO_ROOT}/System/Src/system_fault.c
  ${LAYERED_REPO_ROOT}/System/Src/system_log.c
  ${LAYERED_REPO_ROOT}/System/Src/system_log_base.c
  ${LAYERED_REPO_ROOT}/System/Src/system_text.c
)
target_include_directories(layered_system_base PUBLIC
  ${LAYERED_REPO_ROOT}/System/Inc
)
target_link_libraries(layered_system_base PUBLIC
  layered_board_config
  ${LAYERED_HAL_TARGET}
  m
)

if(SYS_LOG_USE_RTT)
  target_sources(layered_system_base PRIVATE
    ${LAYERED_REPO_ROOT}/System/Src/system_log_rtt.c
  )
  target_compile_definitions(layered_system_base PUBLIC
    SYS_LOG_USE_RTT=1
  )
  target_link_libraries(layered_system_base PUBLIC
    layered_segger_rtt
  )
endif()

set(LAYERED_PLATFORM_COMMON_SOURCES)
set(LAYERED_PLATFORM_BACKEND_SOURCES)

foreach(feature IN LISTS LAYERED_PLATFORM_FEATURES)
  string(TOLOWER "${feature}" feature)

  if(feature STREQUAL "gpio")
    list(APPEND LAYERED_PLATFORM_COMMON_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/common/platform_gpio_common.c
    )
    list(APPEND LAYERED_PLATFORM_BACKEND_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/${LAYERED_PLATFORM_BACKEND}/platform_gpio.c
    )
  elseif(feature STREQUAL "exti")
    list(APPEND LAYERED_PLATFORM_COMMON_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/common/platform_exti_common.c
    )
    list(APPEND LAYERED_PLATFORM_BACKEND_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/${LAYERED_PLATFORM_BACKEND}/platform_exti.c
    )
  elseif(feature STREQUAL "uart")
    list(APPEND LAYERED_PLATFORM_COMMON_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/common/platform_uart_common.c
    )
    list(APPEND LAYERED_PLATFORM_BACKEND_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/${LAYERED_PLATFORM_BACKEND}/platform_uart.c
    )
  elseif(feature STREQUAL "i2c")
    list(APPEND LAYERED_PLATFORM_COMMON_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/common/platform_i2c_common.c
    )
    list(APPEND LAYERED_PLATFORM_BACKEND_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/${LAYERED_PLATFORM_BACKEND}/platform_i2c.c
    )
  elseif(feature STREQUAL "spi")
    list(APPEND LAYERED_PLATFORM_COMMON_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/common/platform_spi_common.c
    )
    list(APPEND LAYERED_PLATFORM_BACKEND_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/${LAYERED_PLATFORM_BACKEND}/platform_spi.c
    )
  elseif(feature STREQUAL "adc")
    list(APPEND LAYERED_PLATFORM_COMMON_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/common/platform_adc_common.c
    )
    list(APPEND LAYERED_PLATFORM_BACKEND_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/${LAYERED_PLATFORM_BACKEND}/platform_adc.c
    )
  elseif(feature STREQUAL "dac")
    list(APPEND LAYERED_PLATFORM_COMMON_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/common/platform_dac_common.c
    )
    list(APPEND LAYERED_PLATFORM_BACKEND_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/${LAYERED_PLATFORM_BACKEND}/platform_dac.c
    )
  elseif(feature STREQUAL "can")
    list(APPEND LAYERED_PLATFORM_COMMON_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/common/platform_can_common.c
    )
    list(APPEND LAYERED_PLATFORM_BACKEND_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/${LAYERED_PLATFORM_BACKEND}/platform_can.c
    )
  elseif(feature STREQUAL "rtc")
    list(APPEND LAYERED_PLATFORM_COMMON_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/common/platform_rtc_common.c
    )
    list(APPEND LAYERED_PLATFORM_BACKEND_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/${LAYERED_PLATFORM_BACKEND}/platform_rtc.c
    )
  elseif(feature STREQUAL "flash")
    list(APPEND LAYERED_PLATFORM_COMMON_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/common/platform_flash_common.c
    )
    list(APPEND LAYERED_PLATFORM_BACKEND_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/${LAYERED_PLATFORM_BACKEND}/platform_flash.c
    )
  elseif(feature STREQUAL "crc")
    list(APPEND LAYERED_PLATFORM_COMMON_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/common/platform_crc_common.c
    )
    list(APPEND LAYERED_PLATFORM_BACKEND_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/${LAYERED_PLATFORM_BACKEND}/platform_crc.c
    )
  elseif(feature STREQUAL "rng")
    list(APPEND LAYERED_PLATFORM_COMMON_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/common/platform_rng_common.c
    )
    list(APPEND LAYERED_PLATFORM_BACKEND_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/${LAYERED_PLATFORM_BACKEND}/platform_rng.c
    )
  elseif(feature STREQUAL "encoder")
    list(APPEND LAYERED_PLATFORM_COMMON_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/common/platform_encoder_common.c
    )
    list(APPEND LAYERED_PLATFORM_BACKEND_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/${LAYERED_PLATFORM_BACKEND}/platform_encoder.c
    )
  elseif(feature STREQUAL "pwm")
    list(APPEND LAYERED_PLATFORM_COMMON_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/common/platform_pwm_common.c
    )
    list(APPEND LAYERED_PLATFORM_BACKEND_SOURCES
      ${LAYERED_REPO_ROOT}/Platform/Src/${LAYERED_PLATFORM_BACKEND}/platform_pwm.c
    )
  else()
    message(FATAL_ERROR "Unsupported Platform feature: ${feature}")
  endif()
endforeach()

list(REMOVE_DUPLICATES LAYERED_PLATFORM_COMMON_SOURCES)
list(REMOVE_DUPLICATES LAYERED_PLATFORM_BACKEND_SOURCES)

if(NOT LAYERED_PLATFORM_COMMON_SOURCES OR NOT LAYERED_PLATFORM_BACKEND_SOURCES)
  message(FATAL_ERROR "At least one Platform feature must be enabled")
endif()

if(LAYERED_PLATFORM_BACKEND STREQUAL "stm32f1")
  set(LAYERED_PLATFORM_BACKEND_TARGET layered_platform_stm32f1)
elseif(LAYERED_PLATFORM_BACKEND STREQUAL "stm32f4")
  set(LAYERED_PLATFORM_BACKEND_TARGET layered_platform_stm32f4)
else()
  message(FATAL_ERROR
    "Unsupported LAYERED_PLATFORM_BACKEND: ${LAYERED_PLATFORM_BACKEND}"
  )
endif()

layered_add_framework_library(${LAYERED_PLATFORM_BACKEND_TARGET}
  ${LAYERED_PLATFORM_COMMON_SOURCES}
  ${LAYERED_PLATFORM_BACKEND_SOURCES}
)
target_include_directories(${LAYERED_PLATFORM_BACKEND_TARGET} PRIVATE
  ${LAYERED_REPO_ROOT}/Platform/Src
)
target_link_libraries(${LAYERED_PLATFORM_BACKEND_TARGET} PUBLIC
  layered_platform_contract
  layered_system_base
)
target_link_libraries(${LAYERED_PLATFORM_BACKEND_TARGET} PRIVATE
  ${LAYERED_HAL_TARGET}
)

add_library(layered_platform INTERFACE)
target_link_libraries(layered_platform INTERFACE
  layered_platform_contract
  ${LAYERED_PLATFORM_BACKEND_TARGET}
)

add_library(layered_system INTERFACE)
target_link_libraries(layered_system INTERFACE
  layered_system_base
)

if("flash" IN_LIST LAYERED_PLATFORM_FEATURES)
  layered_add_framework_library(layered_system_storage
    ${LAYERED_REPO_ROOT}/System/Src/system_record_store.c
  )
  target_link_libraries(layered_system_storage PUBLIC
    layered_platform
    layered_system_base
  )
  target_link_libraries(layered_system INTERFACE
    layered_system_storage
  )
endif()

layered_add_framework_library(layered_controller
  ${LAYERED_REPO_ROOT}/Controller/Base/Src/controller_base.c
  ${LAYERED_REPO_ROOT}/Controller/Src/controller_compose.c
  ${LAYERED_REPO_ROOT}/Controller/Src/controller_linear.c
  ${LAYERED_REPO_ROOT}/Controller/Src/controller_pid.c
)
target_include_directories(layered_controller PUBLIC
  ${LAYERED_REPO_ROOT}/Controller/Base/Inc
  ${LAYERED_REPO_ROOT}/Controller/Inc
)
target_link_libraries(layered_controller PUBLIC
  layered_system
  m
)

layered_add_framework_library(layered_filter
  ${LAYERED_REPO_ROOT}/Filter/Base/Src/filter_base.c
  ${LAYERED_REPO_ROOT}/Filter/Src/filter_chain.c
  ${LAYERED_REPO_ROOT}/Filter/Src/filter_distributor.c
  ${LAYERED_REPO_ROOT}/Filter/Src/filter_ekf.c
  ${LAYERED_REPO_ROOT}/Filter/Src/filter_imu_calibration.c
  ${LAYERED_REPO_ROOT}/Filter/Src/filter_lowpass.c
  ${LAYERED_REPO_ROOT}/Filter/Src/filter_passthrough.c
)
target_include_directories(layered_filter PUBLIC
  ${LAYERED_REPO_ROOT}/Filter/Base/Inc
  ${LAYERED_REPO_ROOT}/Filter/Inc
)
target_link_libraries(layered_filter PUBLIC
  layered_system
  m
)

layered_add_framework_library(layered_policy
  ${LAYERED_REPO_ROOT}/Policy/Base/Src/policy_omni_base.c
  ${LAYERED_REPO_ROOT}/Policy/Src/policy_omni_kinematics.c
)
target_include_directories(layered_policy PUBLIC
  ${LAYERED_REPO_ROOT}/Policy/Base/Inc
  ${LAYERED_REPO_ROOT}/Policy/Inc
)
target_link_libraries(layered_policy PUBLIC
  layered_system
  m
)

layered_add_framework_library(layered_device_base
  ${LAYERED_REPO_ROOT}/DeviceDrivers/Base/Src/chassis_base.c
  ${LAYERED_REPO_ROOT}/DeviceDrivers/Base/Src/encoder_base.c
  ${LAYERED_REPO_ROOT}/DeviceDrivers/Base/Src/imu_base.c
  ${LAYERED_REPO_ROOT}/DeviceDrivers/Base/Src/joycon_base.c
  ${LAYERED_REPO_ROOT}/DeviceDrivers/Base/Src/led_base.c
  ${LAYERED_REPO_ROOT}/DeviceDrivers/Base/Src/motor_base.c
  ${LAYERED_REPO_ROOT}/DeviceDrivers/Base/Src/telemetry_base.c
  ${LAYERED_REPO_ROOT}/DeviceDrivers/Base/Src/uart_base.c
)
target_include_directories(layered_device_base PUBLIC
  ${LAYERED_REPO_ROOT}/DeviceDrivers/Base/Inc
)
target_link_libraries(layered_device_base PUBLIC
  layered_controller
  layered_filter
  layered_system
)

set(LAYERED_DEVICE_DRIVER_SOURCES
  ${LAYERED_REPO_ROOT}/DeviceDrivers/Src/chassis_omni.c
  ${LAYERED_REPO_ROOT}/DeviceDrivers/Src/firewater_uart.c
  ${LAYERED_REPO_ROOT}/DeviceDrivers/Src/imu_wht101_uart.c
  ${LAYERED_REPO_ROOT}/DeviceDrivers/Src/joycon_uart.c
  ${LAYERED_REPO_ROOT}/DeviceDrivers/Src/justfloat_uart.c
  ${LAYERED_REPO_ROOT}/DeviceDrivers/Src/uart_link.c
)

if("gpio" IN_LIST LAYERED_PLATFORM_FEATURES)
  list(APPEND LAYERED_DEVICE_DRIVER_SOURCES
    ${LAYERED_REPO_ROOT}/DeviceDrivers/Src/led_gpio.c
  )
endif()

if("pwm" IN_LIST LAYERED_PLATFORM_FEATURES)
  list(APPEND LAYERED_DEVICE_DRIVER_SOURCES
    ${LAYERED_REPO_ROOT}/DeviceDrivers/Src/led_pwm.c
    ${LAYERED_REPO_ROOT}/DeviceDrivers/Src/motor_tb6612.c
  )
endif()

if("encoder" IN_LIST LAYERED_PLATFORM_FEATURES)
  list(APPEND LAYERED_DEVICE_DRIVER_SOURCES
    ${LAYERED_REPO_ROOT}/DeviceDrivers/Src/encoder_tim.c
  )
endif()

if("i2c" IN_LIST LAYERED_PLATFORM_FEATURES)
  list(APPEND LAYERED_DEVICE_DRIVER_SOURCES
    ${LAYERED_REPO_ROOT}/DeviceDrivers/Src/mpu6050_i2c.c
  )
endif()

if("uart" IN_LIST LAYERED_PLATFORM_FEATURES)
  list(APPEND LAYERED_DEVICE_DRIVER_SOURCES
    ${LAYERED_REPO_ROOT}/DeviceDrivers/Src/uart_port.c
  )
endif()

layered_add_framework_library(layered_device_drivers
  ${LAYERED_DEVICE_DRIVER_SOURCES}
)
target_include_directories(layered_device_drivers PUBLIC
  ${LAYERED_REPO_ROOT}/DeviceDrivers/Inc
)
target_link_libraries(layered_device_drivers PUBLIC
  layered_device_base
  layered_platform
  layered_policy
  layered_system
)

add_library(layered_framework INTERFACE)
target_link_libraries(layered_framework INTERFACE
  layered_platform
  layered_system
  layered_device_drivers
  layered_controller
  layered_filter
  layered_policy
)

cmake_minimum_required(VERSION 3.22)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(layered_car_tests C)
  set(CMAKE_C_STANDARD 11)
  set(CMAKE_C_STANDARD_REQUIRED ON)
  set(CMAKE_C_EXTENSIONS OFF)
  enable_testing()
endif()

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

set(HOST_TEST_COMPILE_OPTIONS
  -std=c11
  -pedantic
  -Wall
  -Wextra
  -Werror
  -UNDEBUG
)

add_library(car_bsp_host STATIC
  ${REPO_ROOT}/BSP/Src/bsp_dwt.c
  ${REPO_ROOT}/BSP/Src/bsp_gpio.c
  ${REPO_ROOT}/BSP/Src/bsp_pwm.c
  ${REPO_ROOT}/BSP/Src/bsp_encoder.c
  ${REPO_ROOT}/BSP/Src/bsp_uart.c
  ${REPO_ROOT}/BSP/Src/bsp_i2c.c
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs/bsp_stub.c
)
target_include_directories(car_bsp_host PUBLIC
  ${REPO_ROOT}/BSP/Inc
  ${REPO_ROOT}/Framework/Core/Inc
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
)
target_compile_definitions(car_bsp_host PUBLIC BSP_HOST_STUB=1)
target_compile_options(car_bsp_host PRIVATE ${HOST_TEST_COMPILE_OPTIONS})

add_library(car_core_host STATIC
  ${REPO_ROOT}/Framework/Core/Src/sys_angle.c
  ${REPO_ROOT}/Framework/Core/Src/sys_byte_ring.c
  ${REPO_ROOT}/Framework/Core/Src/sys_text.c
  ${REPO_ROOT}/Framework/Core/Src/sys_critical.c
  ${REPO_ROOT}/Framework/Core/Src/sys_fault.c
  ${REPO_ROOT}/Framework/Core/Src/sys_log_base.c
  ${REPO_ROOT}/Framework/Core/Src/sys_log.c
  ${REPO_ROOT}/Framework/Core/Src/sys_record_store.c
  ${REPO_ROOT}/Framework/Core/Src/sys_daemon.c
  ${REPO_ROOT}/Framework/Core/Src/rtos_task.c
)
target_include_directories(car_core_host PUBLIC
  ${REPO_ROOT}/Framework/Core/Inc
  ${REPO_ROOT}/BSP/Inc
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
)
target_compile_definitions(car_core_host PUBLIC BSP_HOST_STUB=1)
target_compile_options(car_core_host PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(car_core_host PUBLIC car_bsp_host m)
target_link_libraries(car_bsp_host PUBLIC car_core_host)

add_executable(test_sys_utils
  ${CMAKE_CURRENT_SOURCE_DIR}/test_sys_utils.c
)
target_compile_options(test_sys_utils PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(test_sys_utils PRIVATE car_core_host car_bsp_host m)
add_test(NAME test_sys_utils COMMAND test_sys_utils)

add_executable(test_bsp_encoder
  ${CMAKE_CURRENT_SOURCE_DIR}/test_bsp_encoder.c
)
target_compile_options(test_bsp_encoder PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(test_bsp_encoder PRIVATE car_bsp_host car_core_host m)
add_test(NAME test_bsp_encoder COMMAND test_bsp_encoder)

add_executable(test_sys_record_store
  ${CMAKE_CURRENT_SOURCE_DIR}/test_sys_record_store.c
)
target_compile_options(test_sys_record_store PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(test_sys_record_store PRIVATE car_core_host car_bsp_host m)
add_test(NAME test_sys_record_store COMMAND test_sys_record_store)

add_executable(test_sys_daemon
  ${CMAKE_CURRENT_SOURCE_DIR}/test_sys_daemon.c
)
target_compile_options(test_sys_daemon PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(test_sys_daemon PRIVATE car_core_host car_bsp_host m)
add_test(NAME test_sys_daemon COMMAND test_sys_daemon)

add_executable(test_sys_topic
  ${CMAKE_CURRENT_SOURCE_DIR}/test_sys_topic.c
)
target_compile_options(test_sys_topic PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(test_sys_topic PRIVATE car_core_host car_bsp_host m)
add_test(NAME test_sys_topic COMMAND test_sys_topic)

# car_control_host 严禁引入 BSP/Inc 与 tests/stubs，从编译期锁死 100% HAL-Free 边界。
add_library(car_control_host STATIC
  ${REPO_ROOT}/Framework/Control/Src/controller_base.c
  ${REPO_ROOT}/Framework/Control/Src/controller_pid.c
  ${REPO_ROOT}/Framework/Control/Src/controller_linear.c
  ${REPO_ROOT}/Framework/Control/Src/controller_compose.c
  ${REPO_ROOT}/Framework/Control/Src/filter_base.c
  ${REPO_ROOT}/Framework/Control/Src/filter_passthrough.c
  ${REPO_ROOT}/Framework/Control/Src/filter_lowpass.c
  ${REPO_ROOT}/Framework/Control/Src/filter_chain.c
  ${REPO_ROOT}/Framework/Control/Src/filter_imu_cal.c
  ${REPO_ROOT}/Framework/Control/Src/kinematics_omni.c
)
target_include_directories(car_control_host PUBLIC
  ${REPO_ROOT}/Framework/Control/Inc
  ${REPO_ROOT}/Framework/Core/Inc
)
target_compile_options(car_control_host PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(car_control_host PUBLIC m)

add_library(car_drivers_host STATIC
  ${REPO_ROOT}/Framework/Drivers/Src/motor_base.c
  ${REPO_ROOT}/Framework/Drivers/Src/motor_tb6612.c
  ${REPO_ROOT}/Framework/Drivers/Src/imu_base.c
  ${REPO_ROOT}/Framework/Drivers/Src/imu_wht101.c
  ${REPO_ROOT}/Framework/Drivers/Src/imu_mpu6050.c
  ${REPO_ROOT}/Framework/Drivers/Src/rc_joycon.c
  ${REPO_ROOT}/Framework/Drivers/Src/telemetry_base.c
  ${REPO_ROOT}/Framework/Drivers/Src/telem_uart_tx.c
  ${REPO_ROOT}/Framework/Drivers/Src/telem_justfloat.c
  ${REPO_ROOT}/Framework/Drivers/Src/telem_firewater.c
  ${REPO_ROOT}/Framework/Drivers/Src/led.c
)
target_include_directories(car_drivers_host PUBLIC
  ${REPO_ROOT}/Framework/Drivers/Inc
)
target_include_directories(car_drivers_host PRIVATE
  ${REPO_ROOT}/Framework/Drivers/Src
)
target_compile_options(car_drivers_host PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(car_drivers_host PUBLIC car_bsp_host car_core_host car_control_host m)

add_library(car_chassis_host STATIC
  ${REPO_ROOT}/Framework/Chassis/Src/chassis_base.c
  ${REPO_ROOT}/Framework/Chassis/Src/chassis_omni.c
  ${REPO_ROOT}/Framework/Chassis/Src/car_omni.c
)
target_include_directories(car_chassis_host PUBLIC
  ${REPO_ROOT}/Framework/Chassis/Inc
)
target_compile_options(car_chassis_host PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(car_chassis_host PUBLIC car_core_host car_control_host car_drivers_host m)

add_executable(test_controllers ${CMAKE_CURRENT_SOURCE_DIR}/test_controllers.c)
target_compile_options(test_controllers PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(test_controllers PRIVATE car_control_host car_core_host car_bsp_host m)
add_test(NAME test_controllers COMMAND test_controllers)

add_executable(test_filters ${CMAKE_CURRENT_SOURCE_DIR}/test_filters.c)
target_compile_options(test_filters PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(test_filters PRIVATE car_control_host car_core_host car_bsp_host m)
add_test(NAME test_filters COMMAND test_filters)

add_executable(test_kinematics_omni ${CMAKE_CURRENT_SOURCE_DIR}/test_kinematics_omni.c)
target_compile_options(test_kinematics_omni PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(test_kinematics_omni PRIVATE car_control_host m)
add_test(NAME test_kinematics_omni COMMAND test_kinematics_omni)

add_executable(test_motor_tb6612 ${CMAKE_CURRENT_SOURCE_DIR}/test_motor_tb6612.c)
target_compile_options(test_motor_tb6612 PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(test_motor_tb6612 PRIVATE car_drivers_host car_control_host car_core_host car_bsp_host m)
add_test(NAME test_motor_tb6612 COMMAND test_motor_tb6612)

add_executable(test_imu_wht101 ${CMAKE_CURRENT_SOURCE_DIR}/test_imu_wht101.c)
target_compile_options(test_imu_wht101 PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(test_imu_wht101 PRIVATE car_drivers_host car_control_host car_core_host car_bsp_host m)
add_test(NAME test_imu_wht101 COMMAND test_imu_wht101)

add_executable(test_imu_mpu6050 ${CMAKE_CURRENT_SOURCE_DIR}/test_imu_mpu6050.c)
target_compile_options(test_imu_mpu6050 PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(test_imu_mpu6050 PRIVATE car_drivers_host car_control_host car_core_host car_bsp_host m)
add_test(NAME test_imu_mpu6050 COMMAND test_imu_mpu6050)

add_executable(test_rc_joycon ${CMAKE_CURRENT_SOURCE_DIR}/test_rc_joycon.c)
target_compile_options(test_rc_joycon PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(test_rc_joycon PRIVATE car_drivers_host car_core_host car_bsp_host m)
add_test(NAME test_rc_joycon COMMAND test_rc_joycon)

add_executable(test_telemetry ${CMAKE_CURRENT_SOURCE_DIR}/test_telemetry.c)
target_compile_options(test_telemetry PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(test_telemetry PRIVATE car_drivers_host car_core_host car_bsp_host m)
add_test(NAME test_telemetry COMMAND test_telemetry)

add_executable(test_car_omni ${CMAKE_CURRENT_SOURCE_DIR}/test_car_omni.c)
target_compile_options(test_car_omni PRIVATE ${HOST_TEST_COMPILE_OPTIONS})
target_link_libraries(test_car_omni PRIVATE car_chassis_host car_drivers_host car_control_host car_core_host car_bsp_host m)
add_test(NAME test_car_omni COMMAND test_car_omni)

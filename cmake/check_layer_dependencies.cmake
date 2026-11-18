cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED MODE)
  message(FATAL_ERROR "MODE must be app, rtos or rtos_public")
endif()
if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

file(GLOB_RECURSE layer_sources
  "${SOURCE_DIR}/*.c"
  "${SOURCE_DIR}/*.h"
)

if(MODE STREQUAL "app")
  set(forbidden_pattern
    "#include[ \t]+[<\"]((platform_|stm32|cmsis_os2|FreeRTOS|task|chassis_omni|encoder_tim|controller_pid|filter_passthrough|imu_wht101_uart|joycon_uart|justfloat_uart|motor_tb6612|policy_omni_kinematics|uart_port|mpu6050_i2c|core_board|car_board_config)[^>\"]*)")
elseif(MODE STREQUAL "rtos")
  set(forbidden_pattern
    "#include[ \t]+[<\"]((platform_|stm32|encoder_tim|controller_pid|filter_passthrough|imu_wht101_uart|joycon_uart|justfloat_uart|motor_tb6612|policy_omni_kinematics|uart_port|mpu6050_i2c|core_board|app_car_dependencies)[^>\"]*)")
elseif(MODE STREQUAL "rtos_public")
  set(forbidden_pattern
    "#include[ \t]+[<\"]((platform_|stm32|FreeRTOS|task|encoder_tim|controller_pid|filter_passthrough|imu_wht101_uart|joycon_uart|justfloat_uart|motor_tb6612|policy_omni_kinematics|uart_port|mpu6050_i2c|core_board|app_car_dependencies)[^>\"]*)")
else()
  message(FATAL_ERROR "Unsupported MODE: ${MODE}")
endif()

foreach(source IN LISTS layer_sources)
  file(READ "${source}" content)
  string(REGEX MATCH "${forbidden_pattern}" forbidden_include "${content}")
  if(forbidden_include)
    message(FATAL_ERROR
      "${MODE} layer dependency violation in ${source}: ${forbidden_include}"
    )
  endif()
endforeach()

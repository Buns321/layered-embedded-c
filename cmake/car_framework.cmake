include_guard(GLOBAL)

get_filename_component(CAR_FRAMEWORK_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

function(car_configure_board)
  cmake_parse_arguments(
    ARG
    ""
    "TARGET;MCU_FAMILY;HAL_TARGET;USE_FREERTOS;USE_RTT"
    "APP_SOURCES;BOARD_SOURCES;RTOS_SOURCES;USER_INCLUDES"
    ${ARGN}
  )

  if(NOT ARG_TARGET)
    message(FATAL_ERROR "car_configure_board: TARGET is required")
  endif()
  if(NOT ARG_MCU_FAMILY)
    message(FATAL_ERROR "car_configure_board: MCU_FAMILY is required")
  endif()
  if(NOT ARG_HAL_TARGET OR NOT TARGET "${ARG_HAL_TARGET}")
    message(FATAL_ERROR "car_configure_board: valid HAL_TARGET is required")
  endif()

  if(TARGET STM32_Drivers)
    target_compile_options(STM32_Drivers PRIVATE -Wno-unused-parameter)
    set_target_properties(STM32_Drivers PROPERTIES C_EXTENSIONS ON)
  endif()
  if(TARGET FreeRTOS)
    target_compile_options(FreeRTOS PRIVATE -Wno-unused-parameter)
    set_target_properties(FreeRTOS PROPERTIES C_EXTENSIONS ON)
  endif()

  set(_strict_c_flags -std=c11 -Wall -Wextra -Werror)

  if(ARG_USE_RTT)
    add_library(car_rtt STATIC
      "${CAR_FRAMEWORK_ROOT}/Framework/ThirdParty/SEGGER_RTT/RTT/SEGGER_RTT.c"
    )
    target_include_directories(car_rtt PUBLIC
      "${CAR_FRAMEWORK_ROOT}/Framework/ThirdParty/SEGGER_RTT/RTT"
      "${CAR_FRAMEWORK_ROOT}/Framework/ThirdParty/SEGGER_RTT/Config"
    )
    target_compile_options(car_rtt PRIVATE -Wno-unused-parameter)
  endif()

  add_library(car_bsp STATIC
    "${CAR_FRAMEWORK_ROOT}/BSP/Src/bsp_dwt.c"
    "${CAR_FRAMEWORK_ROOT}/BSP/Src/bsp_gpio.c"
    "${CAR_FRAMEWORK_ROOT}/BSP/Src/bsp_pwm.c"
    "${CAR_FRAMEWORK_ROOT}/BSP/Src/bsp_encoder.c"
    "${CAR_FRAMEWORK_ROOT}/BSP/Src/bsp_uart.c"
    "${CAR_FRAMEWORK_ROOT}/BSP/Src/bsp_i2c.c"
    "${CAR_FRAMEWORK_ROOT}/BSP/Src/bsp_flash_f4.c"
  )
  target_include_directories(car_bsp
    PUBLIC  "${CAR_FRAMEWORK_ROOT}/BSP/Inc"
    PRIVATE "${CAR_FRAMEWORK_ROOT}/Framework/Core/Inc"
  )
  target_compile_definitions(car_bsp PUBLIC "${ARG_MCU_FAMILY}")
  if(ARG_USE_FREERTOS)
    target_compile_definitions(car_bsp PUBLIC USE_FREERTOS)
  endif()
  target_compile_options(car_bsp PRIVATE ${_strict_c_flags})
  target_link_libraries(car_bsp PUBLIC "${ARG_HAL_TARGET}")

  set(_core_sources
    "${CAR_FRAMEWORK_ROOT}/Framework/Core/Src/sys_angle.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Core/Src/sys_byte_ring.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Core/Src/sys_text.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Core/Src/sys_critical.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Core/Src/sys_fault.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Core/Src/sys_log_base.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Core/Src/sys_log.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Core/Src/sys_record_store.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Core/Src/sys_daemon.c"
  )
  if(ARG_USE_FREERTOS)
    list(APPEND _core_sources "${CAR_FRAMEWORK_ROOT}/Framework/Core/Src/rtos_task.c")
  endif()
  if(ARG_USE_RTT)
    list(APPEND _core_sources "${CAR_FRAMEWORK_ROOT}/Framework/Core/Src/sys_log_rtt.c")
  endif()

  add_library(car_core STATIC ${_core_sources})
  target_include_directories(car_core
    PUBLIC  "${CAR_FRAMEWORK_ROOT}/Framework/Core/Inc"
    PRIVATE "${CAR_FRAMEWORK_ROOT}/BSP/Inc"
  )
  target_compile_definitions(car_core PUBLIC "${ARG_MCU_FAMILY}")
  if(ARG_USE_FREERTOS)
    target_compile_definitions(car_core PUBLIC USE_FREERTOS)
  endif()
  if(ARG_USE_RTT)
    target_compile_definitions(car_core PUBLIC SYS_LOG_USE_RTT)
    target_link_libraries(car_core PRIVATE car_rtt)
  endif()
  target_compile_options(car_core PRIVATE ${_strict_c_flags})
  target_link_libraries(car_core PRIVATE "${ARG_HAL_TARGET}" car_bsp)
  target_link_libraries(car_bsp PRIVATE car_core)

  add_library(car_control STATIC
    "${CAR_FRAMEWORK_ROOT}/Framework/Control/Src/controller_base.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Control/Src/controller_pid.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Control/Src/controller_linear.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Control/Src/controller_compose.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Control/Src/filter_base.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Control/Src/filter_passthrough.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Control/Src/filter_lowpass.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Control/Src/filter_chain.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Control/Src/filter_imu_cal.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Control/Src/kinematics_omni.c"
  )
  target_include_directories(car_control
    PUBLIC  "${CAR_FRAMEWORK_ROOT}/Framework/Control/Inc"
    PRIVATE "${CAR_FRAMEWORK_ROOT}/Framework/Core/Inc"
  )
  target_compile_options(car_control PRIVATE ${_strict_c_flags})
  target_link_libraries(car_control PUBLIC m)

  add_library(car_drivers STATIC
    "${CAR_FRAMEWORK_ROOT}/Framework/Drivers/Src/motor_base.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Drivers/Src/motor_tb6612.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Drivers/Src/imu_base.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Drivers/Src/imu_wht101.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Drivers/Src/imu_mpu6050.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Drivers/Src/rc_joycon.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Drivers/Src/telemetry_base.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Drivers/Src/telem_uart_tx.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Drivers/Src/telem_justfloat.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Drivers/Src/telem_firewater.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Drivers/Src/led.c"
  )
  target_include_directories(car_drivers
    PUBLIC  "${CAR_FRAMEWORK_ROOT}/Framework/Drivers/Inc"
    PRIVATE "${CAR_FRAMEWORK_ROOT}/Framework/Drivers/Src"
  )
  target_compile_options(car_drivers PRIVATE ${_strict_c_flags})
  target_link_libraries(car_drivers PUBLIC car_bsp car_core car_control)

  add_library(car_chassis STATIC
    "${CAR_FRAMEWORK_ROOT}/Framework/Chassis/Src/chassis_base.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Chassis/Src/chassis_omni.c"
    "${CAR_FRAMEWORK_ROOT}/Framework/Chassis/Src/car_omni.c"
  )
  target_include_directories(car_chassis
    PUBLIC "${CAR_FRAMEWORK_ROOT}/Framework/Chassis/Inc"
  )
  target_compile_options(car_chassis PRIVATE ${_strict_c_flags})
  target_link_libraries(car_chassis PUBLIC car_core car_control car_drivers)

  add_library(board_user_app STATIC ${ARG_APP_SOURCES})
  target_include_directories(board_user_app PRIVATE
    ${ARG_USER_INCLUDES}
    "${CAR_FRAMEWORK_ROOT}/Framework/Chassis/Inc"
    "${CAR_FRAMEWORK_ROOT}/Framework/Drivers/Inc"
    "${CAR_FRAMEWORK_ROOT}/Framework/Control/Inc"
    "${CAR_FRAMEWORK_ROOT}/Framework/Core/Inc"
  )
  target_compile_definitions(board_user_app PRIVATE CAR_OMNI_APP_VIEW=1)
  target_compile_options(board_user_app PRIVATE ${_strict_c_flags})

  add_library(board_user_cfg STATIC ${ARG_BOARD_SOURCES})
  target_include_directories(board_user_cfg PRIVATE ${ARG_USER_INCLUDES})
  target_compile_options(board_user_cfg PRIVATE ${_strict_c_flags})
  target_link_libraries(board_user_cfg PUBLIC car_chassis car_bsp car_core "${ARG_HAL_TARGET}")
  if(ARG_USE_RTT)
    target_compile_definitions(board_user_cfg PRIVATE SYS_LOG_USE_RTT)
  endif()

  set(_rtos_target "")
  if(ARG_RTOS_SOURCES)
    get_target_property(_hal_inc_dirs "${ARG_HAL_TARGET}" INTERFACE_INCLUDE_DIRECTORIES)
    set(_rtos_inc_dirs "")
    foreach(_dir IN LISTS _hal_inc_dirs)
      if(_dir MATCHES "(Core/Inc|FreeRTOS)")
        list(APPEND _rtos_inc_dirs "${_dir}")
      endif()
    endforeach()
    add_library(board_user_rtos STATIC ${ARG_RTOS_SOURCES})
    target_include_directories(board_user_rtos PRIVATE
      ${ARG_USER_INCLUDES}
      "${CAR_FRAMEWORK_ROOT}/Framework/Core/Inc"
      ${_rtos_inc_dirs}
    )
    if(ARG_USE_FREERTOS)
      target_compile_definitions(board_user_rtos PRIVATE USE_FREERTOS)
    endif()
    target_compile_options(board_user_rtos PRIVATE ${_strict_c_flags})
    set(_rtos_target board_user_rtos)
  endif()

  target_include_directories("${ARG_TARGET}" PRIVATE
    ${ARG_USER_INCLUDES}
    "${CAR_FRAMEWORK_ROOT}/Framework/Core/Inc"
  )
  target_link_libraries("${ARG_TARGET}"
    ${_rtos_target}
    board_user_app
    board_user_cfg
    car_chassis
    car_drivers
    car_control
    car_core
    car_bsp
    $<$<BOOL:${ARG_USE_RTT}>:car_rtt>
    "${ARG_HAL_TARGET}"
  )

  set(_nm_exec "${CMAKE_NM}")
  if(NOT _nm_exec)
    find_program(_nm_exec NAMES arm-none-eabi-nm nm)
  endif()
  set(_verify_hook "OFF")
  if(ARG_USE_FREERTOS)
    set(_verify_hook "ON")
  endif()
  add_custom_command(
    TARGET "${ARG_TARGET}"
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}"
      "-DNM_EXECUTABLE=${_nm_exec}"
      "-DELF_FILE=$<TARGET_FILE:${ARG_TARGET}>"
      "-DREQUIRE_STACK_OVERFLOW_HOOK=${_verify_hook}"
      -P "${CAR_FRAMEWORK_ROOT}/cmake/verify_static_elf.cmake"
    COMMENT "检查静态内存符号与栈溢出钩子"
    VERBATIM
  )
endfunction()

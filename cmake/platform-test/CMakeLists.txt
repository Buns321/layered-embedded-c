cmake_minimum_required(VERSION 3.22)

project(layered_embedded_platform_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # These tests use assert() as their failure path; keep it active in Release.
    add_compile_options(-UNDEBUG)
endif()

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

add_executable(platform_validation_test
    platform_validation_test.c
)

add_executable(imu_pipeline_test
    imu_pipeline_test.c
    ${REPO_ROOT}/DeviceDrivers/Base/Src/imu_base.c
    ${REPO_ROOT}/DeviceDrivers/Base/Src/uart_base.c
    ${REPO_ROOT}/DeviceDrivers/Src/mpu6050_i2c.c
    ${REPO_ROOT}/DeviceDrivers/Src/imu_wht101_uart.c
    ${REPO_ROOT}/Filter/Base/Src/filter_base.c
    ${REPO_ROOT}/Filter/Src/filter_passthrough.c
    ${REPO_ROOT}/System/Src/system_angle.c
    ${REPO_ROOT}/System/Src/system_byte_ring.c
)

add_executable(filter_imu_calibration_contract_test
    filter_imu_calibration_contract_test.c
    ${REPO_ROOT}/Filter/Base/Src/filter_base.c
    ${REPO_ROOT}/Filter/Src/filter_imu_calibration.c
)

add_executable(app_calibration_contract_test
    app_calibration_contract_test.c
    ${REPO_ROOT}/Boards/f103c8t6-fast-channel-task-1/App/Src/app_calibration.c
)

add_executable(app_attitude_reference_contract_test
    app_attitude_reference_contract_test.c
    ${REPO_ROOT}/Boards/f103c8t6-fast-channel-task-1/App/Src/app_attitude_reference.c
    ${REPO_ROOT}/System/Src/system_angle.c
)

add_executable(app_calibration_record_contract_test
    app_calibration_record_contract_test.c
    ${REPO_ROOT}/Boards/f103c8t6-fast-channel-task-1/App/Src/app_calibration_record.c
)

add_executable(app_calibration_flow_contract_test
    app_calibration_flow_contract_test.c
    ${REPO_ROOT}/Boards/f103c8t6-fast-channel-task-1/App/Src/app_calibration_flow.c
    ${REPO_ROOT}/Boards/f103c8t6-fast-channel-task-1/App/Src/app_calibration.c
)

add_executable(app_status_indicator_contract_test
    app_status_indicator_contract_test.c
    ${REPO_ROOT}/Boards/f103c8t6-fast-channel-task-1/App/Src/app_status_indicator.c
)

add_executable(app_command_parser_contract_test
    app_command_parser_contract_test.c
    ${REPO_ROOT}/Boards/f103c8t6-fast-channel-task-1/App/Src/app_command_parser.c
    ${REPO_ROOT}/System/Src/system_text.c
)

add_executable(system_text_contract_test
    system_text_contract_test.c
    ${REPO_ROOT}/System/Src/system_text.c
)

add_executable(system_time_window_contract_test
    system_time_window_contract_test.c
)

add_executable(system_angle_contract_test
    system_angle_contract_test.c
    ${REPO_ROOT}/System/Src/system_angle.c
)

add_executable(system_byte_ring_contract_test
    system_byte_ring_contract_test.c
    ${REPO_ROOT}/System/Src/system_byte_ring.c
)

add_executable(app_fast_channel_telemetry_contract_test
    app_fast_channel_telemetry_contract_test.c
    ${REPO_ROOT}/Boards/f103c8t6-fast-channel-task-1/App/Src/app_fast_channel_telemetry.c
)

add_executable(uart_link_contract_test
    uart_link_contract_test.c
    ${REPO_ROOT}/DeviceDrivers/Src/uart_link.c
    ${REPO_ROOT}/System/Src/system_byte_ring.c
)

add_executable(filter_ekf_contract_test
    filter_ekf_contract_test.c
    ${REPO_ROOT}/Filter/Base/Src/filter_base.c
    ${REPO_ROOT}/Filter/Src/filter_ekf.c
)

add_executable(system_record_store_contract_test
    system_record_store_contract_test.c
    ${REPO_ROOT}/System/Src/system_record_store.c
    ${REPO_ROOT}/Platform/Src/common/platform_flash_common.c
)

add_executable(system_critical_contract_test
    system_critical_contract_test.c
    ${REPO_ROOT}/System/Src/system_critical.c
)

add_executable(platform_rtc_contract_test
    rtc_contract_test.c
    ${REPO_ROOT}/Platform/Src/common/platform_rtc_common.c
)

add_executable(platform_flash_contract_test
    flash_contract_test.c
    ${REPO_ROOT}/Platform/Src/common/platform_flash_common.c
)

add_executable(platform_uart_contract_test
    uart_contract_test.c
    ${REPO_ROOT}/Platform/Src/common/platform_uart_common.c
)

add_executable(platform_i2c_contract_test
    i2c_contract_test.c
    ${REPO_ROOT}/Platform/Src/common/platform_i2c_common.c
)

add_executable(platform_spi_contract_test
    spi_contract_test.c
    ${REPO_ROOT}/Platform/Src/common/platform_spi_common.c
)

add_executable(platform_can_contract_test
    can_contract_test.c
    ${REPO_ROOT}/Platform/Src/common/platform_can_common.c
)

add_executable(platform_adc_contract_test
    adc_contract_test.c
    ${REPO_ROOT}/Platform/Src/common/platform_adc_common.c
)

add_executable(platform_dac_contract_test
    dac_contract_test.c
    ${REPO_ROOT}/Platform/Src/common/platform_dac_common.c
)

add_executable(platform_pwm_contract_test
    pwm_contract_test.c
    ${REPO_ROOT}/Platform/Src/common/platform_pwm_common.c
)

add_executable(platform_encoder_contract_test
    encoder_contract_test.c
    ${REPO_ROOT}/Platform/Src/common/platform_encoder_common.c
)

add_executable(platform_exti_contract_test
    exti_contract_test.c
    ${REPO_ROOT}/Platform/Src/common/platform_exti_common.c
)

add_executable(platform_crc_contract_test
    crc_contract_test.c
    ${REPO_ROOT}/Platform/Src/common/platform_crc_common.c
)

add_executable(platform_rng_contract_test
    rng_contract_test.c
    ${REPO_ROOT}/Platform/Src/common/platform_rng_common.c
)

add_executable(platform_gpio_contract_test
    gpio_contract_test.c
    ${REPO_ROOT}/Platform/Src/common/platform_gpio_common.c
)

add_executable(system_log_contract_test
    system_log_contract_test.c
    ${REPO_ROOT}/System/Src/system_log.c
    ${REPO_ROOT}/System/Src/system_log_base.c
)

add_executable(controller_pid_contract_test
    controller_pid_contract_test.c
    ${REPO_ROOT}/Controller/Base/Src/controller_base.c
    ${REPO_ROOT}/Controller/Src/controller_pid.c
)

add_executable(joycon_uart_contract_test
    joycon_uart_contract_test.c
    ${REPO_ROOT}/DeviceDrivers/Base/Src/joycon_base.c
    ${REPO_ROOT}/DeviceDrivers/Base/Src/uart_base.c
    ${REPO_ROOT}/DeviceDrivers/Src/joycon_uart.c
    ${REPO_ROOT}/System/Src/system_byte_ring.c
)

target_include_directories(platform_validation_test PRIVATE
    ${REPO_ROOT}/Platform/Src
)

target_include_directories(imu_pipeline_test PRIVATE
    ${REPO_ROOT}/DeviceDrivers/Base/Inc
    ${REPO_ROOT}/DeviceDrivers/Inc
    ${REPO_ROOT}/Filter/Base/Inc
    ${REPO_ROOT}/Filter/Inc
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/System/Inc
)

target_compile_definitions(imu_pipeline_test PRIVATE
    PLATFORM_ENABLE_I2C1=1
)

target_include_directories(filter_imu_calibration_contract_test PRIVATE
    ${REPO_ROOT}/Filter/Base/Inc
    ${REPO_ROOT}/Filter/Inc
    ${REPO_ROOT}/System/Inc
)

target_include_directories(app_calibration_contract_test PRIVATE
    ${REPO_ROOT}/Boards/f103c8t6-fast-channel-task-1/App/Inc
    ${REPO_ROOT}/System/Inc
)

target_include_directories(app_attitude_reference_contract_test PRIVATE
    ${REPO_ROOT}/Boards/f103c8t6-fast-channel-task-1/App/Inc
    ${REPO_ROOT}/DeviceDrivers/Base/Inc
    ${REPO_ROOT}/DeviceDrivers/Inc
    ${REPO_ROOT}/Filter/Base/Inc
    ${REPO_ROOT}/Filter/Inc
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/System/Inc
)

target_compile_definitions(app_attitude_reference_contract_test PRIVATE
    PLATFORM_ENABLE_UART1=1
    PLATFORM_ENABLE_I2C1=1
)

target_include_directories(app_calibration_record_contract_test PRIVATE
    ${REPO_ROOT}/Boards/f103c8t6-fast-channel-task-1/App/Inc
    ${REPO_ROOT}/DeviceDrivers/Base/Inc
    ${REPO_ROOT}/DeviceDrivers/Inc
    ${REPO_ROOT}/Filter/Base/Inc
    ${REPO_ROOT}/Filter/Inc
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/System/Inc
)

target_compile_definitions(app_calibration_record_contract_test PRIVATE
    PLATFORM_ENABLE_FLASH=1
    PLATFORM_ENABLE_UART1=1
    PLATFORM_ENABLE_I2C1=1
)

target_include_directories(app_calibration_flow_contract_test PRIVATE
    ${REPO_ROOT}/Boards/f103c8t6-fast-channel-task-1/App/Inc
    ${REPO_ROOT}/DeviceDrivers/Base/Inc
    ${REPO_ROOT}/DeviceDrivers/Inc
    ${REPO_ROOT}/Filter/Base/Inc
    ${REPO_ROOT}/Filter/Inc
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/System/Inc
)

target_compile_definitions(app_calibration_flow_contract_test PRIVATE
    PLATFORM_ENABLE_FLASH=1
    PLATFORM_ENABLE_UART1=1
    PLATFORM_ENABLE_I2C1=1
)

target_include_directories(app_status_indicator_contract_test PRIVATE
    ${REPO_ROOT}/Boards/f103c8t6-fast-channel-task-1/App/Inc
    ${REPO_ROOT}/DeviceDrivers/Base/Inc
    ${REPO_ROOT}/DeviceDrivers/Inc
    ${REPO_ROOT}/Filter/Base/Inc
    ${REPO_ROOT}/Filter/Inc
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/System/Inc
)

target_compile_definitions(app_status_indicator_contract_test PRIVATE
    PLATFORM_ENABLE_FLASH=1
    PLATFORM_ENABLE_UART1=1
    PLATFORM_ENABLE_I2C1=1
)

target_include_directories(app_command_parser_contract_test PRIVATE
    ${REPO_ROOT}/Boards/f103c8t6-fast-channel-task-1/App/Inc
    ${REPO_ROOT}/System/Inc
)

target_include_directories(system_text_contract_test PRIVATE
    ${REPO_ROOT}/System/Inc
)

target_include_directories(system_time_window_contract_test PRIVATE
    ${REPO_ROOT}/System/Inc
)

target_include_directories(system_angle_contract_test PRIVATE
    ${REPO_ROOT}/System/Inc
)

target_include_directories(system_byte_ring_contract_test PRIVATE
    ${REPO_ROOT}/System/Inc
)

target_include_directories(app_fast_channel_telemetry_contract_test PRIVATE
    ${REPO_ROOT}/Boards/f103c8t6-fast-channel-task-1/App/Inc
    ${REPO_ROOT}/DeviceDrivers/Base/Inc
    ${REPO_ROOT}/DeviceDrivers/Inc
    ${REPO_ROOT}/Filter/Base/Inc
    ${REPO_ROOT}/Filter/Inc
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/System/Inc
)

target_compile_definitions(app_fast_channel_telemetry_contract_test PRIVATE
    PLATFORM_ENABLE_UART1=1
    PLATFORM_ENABLE_I2C1=1
    PLATFORM_ENABLE_FLASH=1
    TELEMETRY_CHANNEL_CAPACITY=32
)

target_include_directories(uart_link_contract_test PRIVATE
    ${REPO_ROOT}/DeviceDrivers/Base/Inc
    ${REPO_ROOT}/DeviceDrivers/Inc
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/System/Inc
)

target_compile_definitions(uart_link_contract_test PRIVATE
    PLATFORM_ENABLE_UART1=1
)

target_include_directories(filter_ekf_contract_test PRIVATE
    ${REPO_ROOT}/Filter/Base/Inc
    ${REPO_ROOT}/Filter/Inc
    ${REPO_ROOT}/System/Inc
)

target_include_directories(system_record_store_contract_test PRIVATE
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
    ${REPO_ROOT}/System/Inc
)

target_compile_definitions(system_record_store_contract_test PRIVATE
    PLATFORM_ENABLE_FLASH=1
    STM32F4xx=1
)

target_include_directories(system_critical_contract_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/system_critical_stub
    ${REPO_ROOT}/System/Inc
)

target_compile_definitions(system_critical_contract_test PRIVATE
    USE_FREERTOS=1
    STM32F1xx=1
)

target_include_directories(platform_rtc_contract_test PRIVATE
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
)

target_compile_definitions(platform_rtc_contract_test PRIVATE
    PLATFORM_ENABLE_RTC=1
    STM32F4xx=1
)

target_include_directories(platform_flash_contract_test PRIVATE
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
    ${REPO_ROOT}/System/Inc
)

target_compile_definitions(platform_flash_contract_test PRIVATE
    PLATFORM_ENABLE_FLASH=1
    STM32F4xx=1
)

target_include_directories(platform_uart_contract_test PRIVATE
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
)

target_compile_definitions(platform_uart_contract_test PRIVATE
    PLATFORM_ENABLE_UART1=1
    PLATFORM_ENABLE_UART2=0
    STM32F4xx=1
)

target_include_directories(platform_i2c_contract_test PRIVATE
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
)

target_compile_definitions(platform_i2c_contract_test PRIVATE
    PLATFORM_ENABLE_I2C1=1
    STM32F4xx=1
)

target_include_directories(platform_spi_contract_test PRIVATE
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
)

target_compile_definitions(platform_spi_contract_test PRIVATE
    PLATFORM_ENABLE_SPI1=1
    STM32F4xx=1
)

target_include_directories(platform_can_contract_test PRIVATE
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
)

target_compile_definitions(platform_can_contract_test PRIVATE
    PLATFORM_ENABLE_CAN1=1
    STM32F4xx=1
)

target_include_directories(platform_adc_contract_test PRIVATE
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
)

target_compile_definitions(platform_adc_contract_test PRIVATE
    PLATFORM_ENABLE_ADC1=1
    STM32F4xx=1
)

target_include_directories(platform_dac_contract_test PRIVATE
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
)

target_compile_definitions(platform_dac_contract_test PRIVATE
    PLATFORM_ENABLE_DAC1=1
    STM32F4xx=1
)

target_include_directories(platform_pwm_contract_test PRIVATE
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
)

target_compile_definitions(platform_pwm_contract_test PRIVATE
    PLATFORM_ENABLE_PWM_TIM1=1
    PLATFORM_ENABLE_PWM_TIM2=1
    PLATFORM_ENABLE_PWM_TIM3=1
    PLATFORM_ENABLE_PWM_TIM4=1
    STM32F4xx=1
)

target_include_directories(platform_encoder_contract_test PRIVATE
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
)

target_compile_definitions(platform_encoder_contract_test PRIVATE
    PLATFORM_ENABLE_ENCODER_TIM2=1
    STM32F4xx=1
)

target_include_directories(platform_exti_contract_test PRIVATE
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
)

target_include_directories(platform_crc_contract_test PRIVATE
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
)

target_compile_definitions(platform_crc_contract_test PRIVATE
    PLATFORM_ENABLE_CRC=1
    STM32F4xx=1
)

target_include_directories(platform_rng_contract_test PRIVATE
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
)

target_compile_definitions(platform_rng_contract_test PRIVATE
    PLATFORM_ENABLE_RNG=1
    STM32F4xx=1
)

target_include_directories(platform_gpio_contract_test PRIVATE
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
)

target_include_directories(system_log_contract_test PRIVATE
    ${REPO_ROOT}/System/Inc
)

target_include_directories(controller_pid_contract_test PRIVATE
    ${REPO_ROOT}/Controller/Base/Inc
    ${REPO_ROOT}/Controller/Inc
    ${REPO_ROOT}/System/Inc
)

target_include_directories(joycon_uart_contract_test PRIVATE
    ${REPO_ROOT}/DeviceDrivers/Base/Inc
    ${REPO_ROOT}/DeviceDrivers/Inc
    ${REPO_ROOT}/System/Inc
)

target_compile_definitions(system_log_contract_test PRIVATE
    SYS_LOG_EXTERNAL_TIME_SOURCE=Sys_Log_TestGetTimeMs
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(platform_validation_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(imu_pipeline_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(filter_imu_calibration_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(app_calibration_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(app_attitude_reference_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(app_calibration_record_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(app_calibration_flow_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(app_status_indicator_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(app_command_parser_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(system_text_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(system_time_window_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(system_angle_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(system_byte_ring_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(app_fast_channel_telemetry_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(uart_link_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(filter_ekf_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(system_record_store_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(system_critical_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(platform_rtc_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(platform_flash_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(platform_uart_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(platform_i2c_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(platform_spi_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(platform_can_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(platform_adc_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(platform_dac_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(platform_pwm_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(platform_encoder_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(platform_exti_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(platform_crc_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(platform_rng_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(platform_gpio_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(system_log_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(controller_pid_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
    target_compile_options(joycon_uart_contract_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
endif()

if(NOT MSVC)
    target_link_libraries(imu_pipeline_test PRIVATE m)
    target_link_libraries(filter_imu_calibration_contract_test PRIVATE m)
    target_link_libraries(app_calibration_contract_test PRIVATE m)
    target_link_libraries(app_attitude_reference_contract_test PRIVATE m)
    target_link_libraries(app_calibration_flow_contract_test PRIVATE m)
    target_link_libraries(app_command_parser_contract_test PRIVATE m)
    target_link_libraries(system_text_contract_test PRIVATE m)
    target_link_libraries(system_angle_contract_test PRIVATE m)
    target_link_libraries(app_fast_channel_telemetry_contract_test PRIVATE m)
    target_link_libraries(filter_ekf_contract_test PRIVATE m)
endif()

enable_testing()
add_test(
    NAME platform_validation_test
    COMMAND platform_validation_test
)
add_test(
    NAME imu_pipeline_test
    COMMAND imu_pipeline_test
)
add_test(
    NAME filter_imu_calibration_contract_test
    COMMAND filter_imu_calibration_contract_test
)
add_test(
    NAME app_calibration_contract_test
    COMMAND app_calibration_contract_test
)
add_test(
    NAME app_attitude_reference_contract_test
    COMMAND app_attitude_reference_contract_test
)
add_test(
    NAME app_calibration_record_contract_test
    COMMAND app_calibration_record_contract_test
)
add_test(
    NAME app_calibration_flow_contract_test
    COMMAND app_calibration_flow_contract_test
)
add_test(
    NAME app_status_indicator_contract_test
    COMMAND app_status_indicator_contract_test
)
add_test(
    NAME app_command_parser_contract_test
    COMMAND app_command_parser_contract_test
)
add_test(
    NAME system_text_contract_test
    COMMAND system_text_contract_test
)
add_test(
    NAME system_time_window_contract_test
    COMMAND system_time_window_contract_test
)
add_test(
    NAME system_angle_contract_test
    COMMAND system_angle_contract_test
)
add_test(
    NAME system_byte_ring_contract_test
    COMMAND system_byte_ring_contract_test
)
add_test(
    NAME app_fast_channel_telemetry_contract_test
    COMMAND app_fast_channel_telemetry_contract_test
)
add_test(
    NAME uart_link_contract_test
    COMMAND uart_link_contract_test
)
add_test(
    NAME filter_ekf_contract_test
    COMMAND filter_ekf_contract_test
)
add_test(
    NAME system_record_store_contract_test
    COMMAND system_record_store_contract_test
)
add_test(
    NAME system_critical_contract_test
    COMMAND system_critical_contract_test
)
add_test(
    NAME platform_rtc_contract_test
    COMMAND platform_rtc_contract_test
)
add_test(
    NAME platform_flash_contract_test
    COMMAND platform_flash_contract_test
)
add_test(
    NAME platform_uart_contract_test
    COMMAND platform_uart_contract_test
)
add_test(
    NAME platform_i2c_contract_test
    COMMAND platform_i2c_contract_test
)
add_test(
    NAME platform_spi_contract_test
    COMMAND platform_spi_contract_test
)
add_test(
    NAME platform_can_contract_test
    COMMAND platform_can_contract_test
)
add_test(
    NAME platform_adc_contract_test
    COMMAND platform_adc_contract_test
)
add_test(
    NAME platform_dac_contract_test
    COMMAND platform_dac_contract_test
)
add_test(
    NAME platform_pwm_contract_test
    COMMAND platform_pwm_contract_test
)
add_test(
    NAME platform_encoder_contract_test
    COMMAND platform_encoder_contract_test
)
add_test(
    NAME platform_exti_contract_test
    COMMAND platform_exti_contract_test
)
add_test(
    NAME platform_crc_contract_test
    COMMAND platform_crc_contract_test
)
add_test(
    NAME platform_rng_contract_test
    COMMAND platform_rng_contract_test
)
add_test(
    NAME platform_gpio_contract_test
    COMMAND platform_gpio_contract_test
)
add_test(
    NAME system_log_contract_test
    COMMAND system_log_contract_test
)
add_test(
    NAME controller_pid_contract_test
    COMMAND controller_pid_contract_test
)
add_test(
    NAME joycon_uart_contract_test
    COMMAND joycon_uart_contract_test
)

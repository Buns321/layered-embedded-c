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
)

add_executable(platform_watchdog_contract_test
    watchdog_contract_test.c
    ${REPO_ROOT}/Platform/Src/common/platform_iwdg_common.c
    ${REPO_ROOT}/Platform/Src/common/platform_wwdg_common.c
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

target_include_directories(platform_watchdog_contract_test PRIVATE
    ${REPO_ROOT}/Platform/Inc
    ${REPO_ROOT}/Platform/Src
)

target_compile_definitions(platform_watchdog_contract_test PRIVATE
    PLATFORM_ENABLE_IWDG=1
    PLATFORM_ENABLE_WWDG=1
    STM32F4xx=1
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
    target_compile_options(platform_watchdog_contract_test PRIVATE
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
endif()

if(NOT MSVC)
    target_link_libraries(imu_pipeline_test PRIVATE m)
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
    NAME platform_watchdog_contract_test
    COMMAND platform_watchdog_contract_test
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

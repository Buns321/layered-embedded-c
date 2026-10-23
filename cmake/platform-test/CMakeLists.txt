cmake_minimum_required(VERSION 3.22)

project(layered_embedded_platform_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

add_executable(platform_validation_test
    platform_validation_test.c
)

add_executable(imu_pipeline_test
    imu_pipeline_test.c
    ${REPO_ROOT}/DeviceDrivers/Base/Src/imu_base.c
    ${REPO_ROOT}/DeviceDrivers/Base/Src/usart_base.c
    ${REPO_ROOT}/DeviceDrivers/Src/mpu6050_i2c.c
    ${REPO_ROOT}/DeviceDrivers/Src/imu_wht101_usart.c
    ${REPO_ROOT}/Filter/Base/Src/filter_base.c
    ${REPO_ROOT}/Filter/Src/filter_passthrough.c
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

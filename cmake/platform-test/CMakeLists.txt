cmake_minimum_required(VERSION 3.22)

project(layered_embedded_platform_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

add_executable(platform_validation_test
    platform_validation_test.c
)

target_include_directories(platform_validation_test PRIVATE
    ${REPO_ROOT}/Platform/Src
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(platform_validation_test PRIVATE
        -Wall
        -Wextra
        -Werror
    )
endif()

enable_testing()
add_test(
    NAME platform_validation_test
    COMMAND platform_validation_test
)

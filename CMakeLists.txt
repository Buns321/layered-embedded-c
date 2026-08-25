cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME layered-embedded-c)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
        Platform/Inc/platform_def.h
        Platform/Src/stm32f1/platform_gpio.c
        Platform/Inc/platform_gpio.h
        DeviceDrivers/Base/Src/led_base.c
        DeviceDrivers/Base/Inc/led_base.h
        DeviceDrivers/Src/led_gpio.c
        DeviceDrivers/Inc/led_gpio.h
        App/Src/app_blink.c
        App/Inc/app_blink.h
        System/Src/system_delay.c
        System/Inc/system_delay.h
        Platform/Src/stm32f1/platform_pwm.c
        Platform/Inc/platform_pwm.h
        Platform/Src/stm32f1/platform_tim.c
        Platform/Inc/platform_tim.h
        DeviceDrivers/Src/motor_tb6612.c
        DeviceDrivers/Inc/motor_tb6612.h
        DeviceDrivers/Base/Src/motor_base.c
        DeviceDrivers/Base/Inc/motor_base.h
        Platform/Src/stm32f1/platform_encoder.c
        Platform/Inc/platform_encoder.h
        Platform/Src/stm32f1/platform_usart.c
        Platform/Inc/platform_usart.h
        App/Src/app_motor_demo.c
        App/Inc/app_motor_demo.h
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

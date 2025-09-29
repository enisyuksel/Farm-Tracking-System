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

# Read configuration from config.json
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.json")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/config.json" CONFIG_JSON)
    
    # Extract build mode from JSON (simple string matching)
    string(REGEX MATCH "\"mode\"[ ]*:[ ]*\"([^\"]+)\"" _ ${CONFIG_JSON})
    set(BUILD_MODE ${CMAKE_MATCH_1})
    
    if(BUILD_MODE STREQUAL "GATEWAY")
        message(STATUS "🏭 Building GATEWAY configuration")
        set(PROJECT_SUFFIX "Gateway")
        add_definitions(-DBUILD_MODE_GATEWAY=1)
    elseif(BUILD_MODE STREQUAL "NODE") 
        message(STATUS "📡 Building NODE configuration")
        set(PROJECT_SUFFIX "Node")
        add_definitions(-DBUILD_MODE_NODE=1)
    else()
        message(WARNING "⚠️  Unknown build mode: ${BUILD_MODE}, defaulting to GATEWAY")
        set(BUILD_MODE "GATEWAY")
        set(PROJECT_SUFFIX "Gateway")
        add_definitions(-DBUILD_MODE_GATEWAY=1)
    endif()
else()
    message(WARNING "⚠️  config.json not found, defaulting to GATEWAY mode")
    set(BUILD_MODE "GATEWAY")
    set(PROJECT_SUFFIX "Gateway")
    add_definitions(-DBUILD_MODE_GATEWAY=1)
endif()

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()


# Set the project name with suffix
set(CMAKE_PROJECT_NAME SubGHz_Phy_PingPong_${PROJECT_SUFFIX})

# Set output directory for all build types to build/Debug
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/Debug")

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Src/common.c
    Core/Src/EMPA_MqttAws.c
    Core/Src/myESP32AT.c
    
    # OTA System sources
    SubGHz_Phy/App/lora_ota_gateway.c
    SubGHz_Phy/App/github_ota_downloader.c
    SubGHz_Phy/App/ota_integration_app.c
    SubGHz_Phy/App/lora_ota_node.c
    SubGHz_Phy/App/ota_node_integration_app.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    SubGHz_Phy/App
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# Generate .bin and .hex files from .elf
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_PROJECT_NAME}.hex
    COMMENT "Generating .bin and .hex files"
)

# Print file sizes
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMENT "Firmware size:"
)

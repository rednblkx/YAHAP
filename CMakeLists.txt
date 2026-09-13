# YAHAP library root.
#
# Two consumption modes:
#
# 1. Standalone CMake project (native build, tests): normal CMake flow.
# 2. ESP-IDF component: this repo (or a symlink to it) placed under a
#    project's components/ directory. idf_component_register() declares the
#    sources; there are no external dependencies.
#
if(NOT ESP_PLATFORM)
    cmake_minimum_required(VERSION 3.20)
    project(horizon-hap VERSION 0.1.0 LANGUAGES CXX)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    # Default build type only when the user hasn't chosen one. NOTE: tests use
    # their own CHECK macros (not <cassert>), so they stay live in any mode.
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE "MinSizeRel" CACHE STRING "Build type" FORCE)
    endif()
endif()

set(YAHAP_SOURCES
    src/core/AccessoryServer.cpp
    src/common/JsonValue.cpp
    src/common/TaskScheduler.cpp
    src/core/TLV8.cpp
    src/core/Characteristic.cpp
    src/core/AttributeDatabaseJSON.cpp
    src/core/CharacteristicSerializer.cpp
    src/core/IIDManager.cpp
    src/transport/HTTP.cpp
    src/transport/SecureSession.cpp
    src/transport/Router.cpp
    src/platform/Ble.cpp
    src/transport/BleTransport.cpp
    src/transport/ConnectionContext.cpp
    src/transport/PairingEndpoints.cpp
    src/transport/AccessoryEndpoints.cpp
    src/transport/ble/HapPdu.cpp
    src/transport/ble/BleTlvBuilder.cpp
    src/transport/ble/BleSessionManager.cpp
    src/pairing/PairingKeys.cpp
    src/pairing/PairSetup.cpp
    src/pairing/PairVerify.cpp
    src/types/CharacteristicTypes.cpp
    src/types/ServiceTypes.cpp
)

# Compile-time log level: 0=Debug 1=Info 2=Warning 3=Error. Messages below the
# level vanish at compile time (format strings, std::string building and all).
# Default Info keeps pairing/bootstrap visibility; set 0 when debugging.
# Declared before the ESP-IDF branch so both build modes see it.
set(YAHAP_LOG_LEVEL "1" CACHE STRING "HAP log level: 0=Debug 1=Info 2=Warning 3=Error")

set(YAHAP_EMBEDDED_CXX_FLAGS
    -fno-exceptions -fno-rtti
    -fno-unwind-tables -fno-asynchronous-unwind-tables
    -ffunction-sections -fdata-sections
    -fno-threadsafe-statics)

if(ESP_PLATFORM)
    # Mode 2: proper ESP-IDF component.
    idf_component_register(
        SRCS ${YAHAP_SOURCES}
        INCLUDE_DIRS include
    )
    target_compile_definitions(${COMPONENT_LIB} PUBLIC HAP_LOG_LEVEL=${YAHAP_LOG_LEVEL})
    target_compile_options(${COMPONENT_LIB} PUBLIC ${YAHAP_EMBEDDED_CXX_FLAGS})
    return()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(hap)
target_sources(hap PRIVATE ${YAHAP_SOURCES})

target_include_directories(hap PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_options(hap PUBLIC ${YAHAP_EMBEDDED_CXX_FLAGS})
target_compile_options(hap PRIVATE -Wall -Wextra -Wpedantic -Werror)

if(APPLE)
    target_link_options(hap PRIVATE -Wl,-dead_strip)
else()
    target_link_options(hap PRIVATE -Wl,--gc-sections)
endif()

# Compile-time log level for the native build (the value is declared above the
# ESP-IDF branch so both modes share it).
target_compile_definitions(hap PUBLIC HAP_LOG_LEVEL=${YAHAP_LOG_LEVEL})

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()

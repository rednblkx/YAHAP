# YAHAP library root.
#
# Two consumption modes:
#
# 1. Standalone CMake project (native build, tests): normal CMake flow with
#    FetchContent for nlohmann/json.
# 2. ESP-IDF component: this repo (or a symlink to it) placed under a
#    project's components/ directory. idf_component_register() declares the
#    sources and the nlohmann-json dependency, which the IDF component
#    manager resolves from the registry.
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
    src/AccessoryServer.cpp
    src/common/TaskScheduler.cpp
    src/core/TLV8.cpp
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

if(ESP_PLATFORM)
    # Mode 2: proper ESP-IDF component.
    idf_component_register(
        SRCS ${YAHAP_SOURCES}
        INCLUDE_DIRS include
        REQUIRES nlohmann-json
    )
    target_compile_definitions(${COMPONENT_LIB} PRIVATE JSON_NOEXCEPTION)
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

include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)
target_link_libraries(hap PRIVATE $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>)
target_compile_definitions(hap PRIVATE JSON_NOEXCEPTION)

if(MSVC)
    target_compile_options(hap PRIVATE /W4 /WX)
else()
    target_compile_options(hap PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()

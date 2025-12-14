cmake_minimum_required(VERSION 3.20)
project(horizon-hap VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Library target
add_library(hap)

# Include directories
target_include_directories(hap PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Dependencies
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

target_link_libraries(hap PRIVATE $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>)
target_compile_definitions(hap PRIVATE JSON_NOEXCEPTION)

# Source files (will be populated as we add them)
target_sources(hap PRIVATE
    src/hap.cpp
    src/AccessoryServer.cpp
    src/core/TLV8.cpp
    src/core/AttributeDatabaseJSON.cpp
    src/transport/HTTP.cpp
    src/transport/SecureSession.cpp
    src/transport/Router.cpp
    src/platform/Ble.cpp
    src/transport/BleTransport.cpp
    src/transport/ConnectionContext.cpp
    src/transport/PairingEndpoints.cpp
    src/transport/AccessoryEndpoints.cpp
    src/pairing/PairSetup.cpp
    src/pairing/PairVerify.cpp
)

# Compiler warnings (High quality code)
if(MSVC)
    target_compile_options(hap PRIVATE /W4 /WX)
else()
    target_compile_options(hap PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Testing (only if standalone build)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()

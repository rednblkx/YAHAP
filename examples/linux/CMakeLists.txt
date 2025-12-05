cmake_minimum_required(VERSION 3.20)
project(hap_linux_example CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find dependencies
find_package(OpenSSL REQUIRED)

# Fetch simplesrp library for SRP-6a
include(FetchContent)
FetchContent_Declare(
    simplesrp
    GIT_REPOSITORY https://github.com/Alkenso/simplesrp.git
    GIT_TAG main
)
FetchContent_MakeAvailable(simplesrp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the main HAP library
add_subdirectory(../.. hap)

# Example executable
add_executable(hap_lightbulb_example
    src/main.cpp
    src/LinuxCrypto.cpp
    src/LinuxNetwork.cpp
)

target_include_directories(hap_lightbulb_example PRIVATE
    include
    ../../include
    # Avahi headers are usually in standard paths
)

target_link_libraries(hap_lightbulb_example PRIVATE
    hap
    OpenSSL::SSL
    OpenSSL::Crypto
    simplesrp
    avahi-client
    avahi-common
)

# Compiler warnings
target_compile_options(hap_lightbulb_example PRIVATE
    -Wall -Wextra -Wpedantic
)

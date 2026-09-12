cmake_minimum_required(VERSION 3.20)
project(hap_linux_example CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVAHI REQUIRED avahi-client)

include(FetchContent)
FetchContent_Declare(
    simplesrp
    GIT_REPOSITORY https://github.com/Alkenso/simplesrp.git
    GIT_TAG ca72944d1d426180e1399011a431aaf634f5f8a9
)
FetchContent_MakeAvailable(simplesrp)

add_subdirectory(../.. hap)

add_subdirectory(components/HK-HomeKit-Lib)

add_executable(hap_lightbulb_example
    src/main.cpp
    src/LinuxCrypto.cpp
    src/LinuxNetwork.cpp
)

target_include_directories(hap_lightbulb_example PRIVATE
    include
    ../../include
)

target_link_libraries(hap_lightbulb_example PRIVATE
    hap
    OpenSSL::SSL
    OpenSSL::Crypto
    simplesrp
    ${AVAHI_LIBRARIES}
    avahi-common
    HK-HomeKit-Lib
)
target_include_directories(hap_lightbulb_example SYSTEM PRIVATE ${AVAHI_INCLUDE_DIRS})

target_compile_options(hap_lightbulb_example PRIVATE
    -Wall -Wextra -Wpedantic
)

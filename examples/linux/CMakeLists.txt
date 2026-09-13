cmake_minimum_required(VERSION 3.20)
project(hap_linux_example C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")

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

# tinycbor — required by DigitalDoorKey (CBOR encode/decode).
FetchContent_Declare(
    tinycbor
    GIT_REPOSITORY https://github.com/intel/tinycbor.git
    GIT_TAG        v0.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tinycbor)
file(GLOB TINYCBOR_SOURCES ${tinycbor_SOURCE_DIR}/src/*.c)
add_library(tinycbor STATIC ${TINYCBOR_SOURCES})
target_include_directories(tinycbor PUBLIC ${tinycbor_SOURCE_DIR}/src)

add_subdirectory(../.. hap)

# DigitalDoorKey (HomeKey protocol subset). The component's own CMakeLists
# globs all sources, which pulls in ESP-only translation units
# (src/homekey/Profile.cpp, src/auth/AttestationAuth.cpp — unguarded
# esp_random.h / esp_log_* includes), so the HomeKey-relevant sources are
# listed explicitly here instead of using add_subdirectory.
set(DDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/components/HK-HomeKit-Lib)
add_library(HK-HomeKit-Lib STATIC
    ${DDK_ROOT}/src/session/Session.cpp
    ${DDK_ROOT}/src/transport/ApduCommand.cpp
    ${DDK_ROOT}/src/transport/NfcChannel.cpp
    ${DDK_ROOT}/src/crypto/CommonCryptoUtils.cpp
    ${DDK_ROOT}/src/crypto/GcmSecureChannel.cpp
    ${DDK_ROOT}/src/crypto/ScbSecureChannel.cpp
    ${DDK_ROOT}/src/crypto/x963kdf.cpp
    ${DDK_ROOT}/src/auth/HKFastAuth.cpp
    ${DDK_ROOT}/src/auth/HKStandardAuth.cpp
    ${DDK_ROOT}/src/auth/AttestationAuth.cpp
    ${DDK_ROOT}/src/homekey/HomeKeyKeySchedule.cpp
    ${DDK_ROOT}/src/homekey/HomeKeySecureContext.cpp
    ${DDK_ROOT}/src/homekey/Profile.cpp
    ${DDK_ROOT}/src/homekey/ProfileFactory.cpp
    ${DDK_ROOT}/src/utils/HK_HomeKit.cpp
)
target_include_directories(HK-HomeKit-Lib PUBLIC
    ${DDK_ROOT}/include
    ${DDK_ROOT}/include/ddk
)
target_include_directories(HK-HomeKit-Lib SYSTEM PUBLIC
    ${DDK_ROOT}/priv
    ${CMAKE_CURRENT_SOURCE_DIR}/esp_shim)
target_link_libraries(HK-HomeKit-Lib PUBLIC mbedcrypto sodium tinycbor)

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

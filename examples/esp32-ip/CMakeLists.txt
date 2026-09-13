cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
# IP-only accessory: compile the HAP-BLE transport out of the yahap component
# (~40 KB less code).
set(YAHAP_BLE OFF CACHE STRING "" FORCE)
project(esp32-hap-ip-example)

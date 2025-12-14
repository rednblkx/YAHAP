enable_testing()

add_executable(verify_architecture VerifyArchitecture.cpp)
target_link_libraries(verify_architecture PRIVATE hap)

add_executable(BleTransportTest BleTransportTest.cpp)
target_link_libraries(BleTransportTest PRIVATE hap)
add_test(NAME BleTransportTest COMMAND BleTransportTest)

add_executable(tlv8_test TLV8Test.cpp)
target_link_libraries(tlv8_test PRIVATE hap)

add_executable(http_test HTTPTest.cpp)
target_link_libraries(http_test PRIVATE hap)

add_executable(json_test JSONTest.cpp)
target_link_libraries(json_test PRIVATE hap)

# In a real project we would fetch GoogleTest here
# For now, we just want to verify compilation and linking of the library structure

enable_testing()

add_executable(verify_architecture VerifyArchitecture.cpp)
target_link_libraries(verify_architecture PRIVATE hap)
add_test(NAME VerifyArchitectureTest COMMAND verify_architecture)

add_executable(BleTransportTest BleTransportTest.cpp)
target_link_libraries(BleTransportTest PRIVATE hap)
add_test(NAME BleTransportTest COMMAND BleTransportTest)

add_executable(tlv8_test TLV8Test.cpp)
target_link_libraries(tlv8_test PRIVATE hap)
add_test(NAME TLV8Test COMMAND tlv8_test)

add_executable(http_test HTTPTest.cpp)
target_link_libraries(http_test PRIVATE hap)
add_test(NAME HTTPTest COMMAND http_test)

add_executable(json_test JSONTest.cpp)
target_link_libraries(json_test PRIVATE hap)
add_test(NAME JSONTest COMMAND json_test)

add_executable(hap_validation_test HAPValidationTest.cpp)
target_link_libraries(hap_validation_test PRIVATE hap)
add_test(NAME HAPValidationTest COMMAND hap_validation_test)
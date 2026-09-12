enable_testing()

# VerifyArchitecture is not suffixed like the other test sources.
add_executable(VerifyArchitectureTest VerifyArchitecture.cpp)
target_link_libraries(VerifyArchitectureTest PRIVATE hap)
add_test(NAME VerifyArchitectureTest COMMAND VerifyArchitectureTest)

foreach(test_target
    BleTransportTest
    TLV8Test
    HTTPTest
    JSONTest
    HAPValidationTest
    CoreSecurityTest
)
    add_executable(${test_target} ${test_target}.cpp)
    target_link_libraries(${test_target} PRIVATE hap)
    add_test(NAME ${test_target} COMMAND ${test_target})
endforeach()

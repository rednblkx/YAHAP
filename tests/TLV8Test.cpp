#include "hap/core/TLV8.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <numeric>

using namespace hap::core;

void test_simple_encode_decode() {
    std::vector<TLV> input;
    input.emplace_back(1, std::vector<uint8_t>{0x01, 0x02});
    input.emplace_back(2, std::string("Hello"));

    auto encoded = TLV8::encode(input);
    
    // Type(1) Len(2) Val(01 02) | Type(2) Len(5) Val(Hello)
    assert(encoded.size() == 2 + 2 + 2 + 5);
    assert(encoded[0] == 1);
    assert(encoded[1] == 2);
    assert(encoded[2] == 0x01);
    assert(encoded[3] == 0x02);

    auto decoded = TLV8::parse(encoded);
    assert(decoded.size() == 2);
    assert(decoded[0].type == 1);
    assert(decoded[0].value.size() == 2);
    assert(decoded[1].type == 2);
    std::string s(decoded[1].value.begin(), decoded[1].value.end());
    assert(s == "Hello");
    
    std::cout << "test_simple_encode_decode passed" << std::endl;
}

void test_fragmentation() {
    std::vector<uint8_t> large_data(300);
    std::iota(large_data.begin(), large_data.end(), 0); // 0, 1, 2...

    std::vector<TLV> input;
    input.emplace_back(10, large_data);

    auto encoded = TLV8::encode(input);
    
    // Should be split into:
    // Type(10) Len(255) Val(...)
    // Type(10) Len(45) Val(...)
    assert(encoded.size() == (2 + 255) + (2 + 45));
    assert(encoded[0] == 10);
    assert(encoded[1] == 255);
    assert(encoded[2 + 255] == 10);
    assert(encoded[2 + 255 + 1] == 45);

    auto decoded = TLV8::parse(encoded);
    assert(decoded.size() == 1);
    assert(decoded[0].type == 10);
    assert(decoded[0].value.size() == 300);
    assert(decoded[0].value == large_data);

    std::cout << "test_fragmentation passed" << std::endl;
}

void test_helpers() {
    std::vector<TLV> input;
    input.emplace_back(1, uint8_t(42));
    input.emplace_back(2, "World");

    assert(TLV8::find_uint8(input, 1) == 42);
    assert(TLV8::find_string(input, 2) == "World");
    assert(!TLV8::find(input, 3).has_value());

    std::cout << "test_helpers passed" << std::endl;
}

int main() {
    test_simple_encode_decode();
    test_fragmentation();
    test_helpers();
    return 0;
}

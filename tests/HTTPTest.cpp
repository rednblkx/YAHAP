#include "hap/transport/HTTP.hpp"
#include "hap/transport/AccessoryEndpoints.hpp"
#include "hap/core/AttributeDatabase.hpp"
#include "hap/core/Service.hpp"
#include "hap/core/Characteristic.hpp"
#include "MockPal.hpp"
#include "TestUtil.hpp"
#include <iostream>
#include <string>

using namespace hap::transport;

void test_simple_request() {
    std::string http_req = "GET /accessories HTTP/1.1\r\nHost: example.com\r\n\r\n";
    std::vector<uint8_t> data(http_req.begin(), http_req.end());

    HTTPParser parser;
    bool complete = parser.feed(data);

    CHECK(complete);
    
    auto req = parser.take_request();
    CHECK(req.method == Method::GET);
    CHECK(req.path == "/accessories");
    CHECK(req.get_header("Host") == "example.com");
    CHECK(req.body.empty());

    std::cout << "test_simple_request passed" << std::endl;
}

void test_post_with_body() {
    std::string http_req = "POST /pair-setup HTTP/1.1\r\nContent-Length: 5\r\n\r\nHello";
    std::vector<uint8_t> data(http_req.begin(), http_req.end());

    HTTPParser parser;
    bool complete = parser.feed(data);

    CHECK(complete);
    
    auto req = parser.take_request();
    CHECK(req.method == Method::POST);
    CHECK(req.path == "/pair-setup");
    CHECK(req.body.size() == 5);
    std::string body_str(req.body.begin(), req.body.end());
    CHECK(body_str == "Hello");

    std::cout << "test_post_with_body passed" << std::endl;
}

void test_chunked_parsing() {
    std::string http_req = "GET /test HTTP/1.1\r\nHost: example.com\r\n\r\n";
    
    HTTPParser parser;
    
    // Feed in chunks
    std::vector<uint8_t> chunk1(http_req.begin(), http_req.begin() + 10);
    std::vector<uint8_t> chunk2(http_req.begin() + 10, http_req.end());

    CHECK(!parser.feed(chunk1)); // Not complete yet
    CHECK(parser.feed(chunk2));  // Now complete

    auto req = parser.take_request();
    CHECK(req.method == Method::GET);
    CHECK(req.path == "/test");

    std::cout << "test_chunked_parsing passed" << std::endl;
}

void test_response_builder() {
    Response resp(Status::OK);
    resp.set_header("Content-Type", "application/json");
    resp.set_body(R"({"test": 123})");

    auto data = HTTPBuilder::build(resp);
    std::string result(data.begin(), data.end());

    CHECK(result.find("HTTP/1.1 200 OK") == 0);
    CHECK(result.find("Content-Type: application/json") != std::string::npos);
    CHECK(result.find("Content-Length: 13") != std::string::npos);
    CHECK(result.find(R"({"test": 123})") != std::string::npos);

    std::cout << "test_response_builder passed" << std::endl;
}

void test_base64_alphabet_decode();

int main() {
    test_simple_request();
    test_post_with_body();
    test_chunked_parsing();
    test_response_builder();
    test_base64_alphabet_decode();
    return 0;
}

// ---------------------------------------------------------------------------
// base64 decoding through the PUT /characteristics path.
//
// Regression: the decode table once mapped '+' and '/' to "invalid", so any
// TLV8/Data payload containing them (e.g. HomeKit control-point writes, which
// are dense binary) was silently corrupted — bytes dropped, subsequent TLVs
// misparsed. Decode must handle the full alphabet.
// ---------------------------------------------------------------------------

void test_base64_alphabet_decode() {
    testmock::MockCrypto crypto;
    testmock::MockStorage storage;
    testmock::MockSystem system{true};

    hap::core::AttributeDatabase db;
    auto acc = std::make_shared<hap::core::Accessory>(1);
    auto svc = std::make_shared<hap::core::Service>(0x4A, "Test");
    // Data-format characteristic with PairedWrite: values arrive base64.
    auto data_char = std::make_shared<hap::core::Characteristic>(
        0x01, hap::core::Format::Data,
        std::vector{hap::core::Permission::PairedWrite});
    svc->add_characteristic(data_char);
    acc->add_service(svc);
    CHECK(db.add_accessory(acc) == hap::core::ValidationResult::Success);

    hap::transport::AccessoryEndpoints endpoints(&db);

    // Raw payload contains 0x2F ('/'); base64 "AQECBi/7/wo=" covers '+', '/'
    // and alphanumerics: decodes to 01 01 02 06 2F FB FF 0A.
    const std::string body =
        R"({"characteristics":[{"aid":1,"iid":2,"value":"AQECBi/7/wo="}]})";

    hap::transport::Request req;
    req.method = Method::PUT;
    req.path = "/characteristics";
    req.headers["Content-Type"] = "application/hap+json";
    req.body.assign(body.begin(), body.end());

    hap::transport::ConnectionContext ctx(&crypto, &system, 1);
    auto resp = endpoints.handle_put_characteristics(req, ctx);

    // 204 No Content = the write was accepted (value type-checked OK).
    CHECK(resp.status == Status::NoContent);

    // The stored value must be the exact decoded bytes, including 0x2F ('/')
    // and 0xFB/0xFF (from '+/8' style runs).
    auto stored = data_char->get_value();
    CHECK(std::holds_alternative<hap::core::Value>(stored));
    auto& bytes = std::get<std::vector<uint8_t>>(std::get<hap::core::Value>(stored));
    const std::vector<uint8_t> expected = {0x01, 0x01, 0x02, 0x06, 0x2F, 0xFB, 0xFF, 0x0A};
    CHECK(bytes == expected);

    std::cout << "test_base64_alphabet_decode passed" << std::endl;
}

// ---------------------------------------------------------------------------
// base64 decoding through the PUT /characteristics path.
//
// Regression: the decode table once mapped '+' and '/' to "invalid", so any
// TLV8/Data payload containing them (e.g. HomeKit control-point writes, which
// are dense binary) was silently corrupted — bytes dropped, subsequent TLVs
// misparsed. Decode must handle the full alphabet.

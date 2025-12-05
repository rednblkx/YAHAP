#include "hap/transport/HTTP.hpp"
#include <cassert>
#include <iostream>
#include <string>

using namespace hap::transport;

void test_simple_request() {
    std::string http_req = "GET /accessories HTTP/1.1\r\nHost: example.com\r\n\r\n";
    std::vector<uint8_t> data(http_req.begin(), http_req.end());

    HTTPParser parser;
    bool complete = parser.feed(data);

    assert(complete);
    
    auto req = parser.take_request();
    assert(req.method == Method::GET);
    assert(req.path == "/accessories");
    assert(req.get_header("Host") == "example.com");
    assert(req.body.empty());

    std::cout << "test_simple_request passed" << std::endl;
}

void test_post_with_body() {
    std::string http_req = "POST /pair-setup HTTP/1.1\r\nContent-Length: 5\r\n\r\nHello";
    std::vector<uint8_t> data(http_req.begin(), http_req.end());

    HTTPParser parser;
    bool complete = parser.feed(data);

    assert(complete);
    
    auto req = parser.take_request();
    assert(req.method == Method::POST);
    assert(req.path == "/pair-setup");
    assert(req.body.size() == 5);
    std::string body_str(req.body.begin(), req.body.end());
    assert(body_str == "Hello");

    std::cout << "test_post_with_body passed" << std::endl;
}

void test_chunked_parsing() {
    std::string http_req = "GET /test HTTP/1.1\r\nHost: example.com\r\n\r\n";
    
    HTTPParser parser;
    
    // Feed in chunks
    std::vector<uint8_t> chunk1(http_req.begin(), http_req.begin() + 10);
    std::vector<uint8_t> chunk2(http_req.begin() + 10, http_req.end());

    assert(!parser.feed(chunk1)); // Not complete yet
    assert(parser.feed(chunk2));  // Now complete

    auto req = parser.take_request();
    assert(req.method == Method::GET);
    assert(req.path == "/test");

    std::cout << "test_chunked_parsing passed" << std::endl;
}

void test_response_builder() {
    Response resp(Status::OK);
    resp.set_header("Content-Type", "application/json");
    resp.set_body(R"({"test": 123})");

    auto data = HTTPBuilder::build(resp);
    std::string result(data.begin(), data.end());

    assert(result.find("HTTP/1.1 200 OK") == 0);
    assert(result.find("Content-Type: application/json") != std::string::npos);
    assert(result.find("Content-Length: 13") != std::string::npos);
    assert(result.find(R"({"test": 123})") != std::string::npos);

    std::cout << "test_response_builder passed" << std::endl;
}

int main() {
    test_simple_request();
    test_post_with_body();
    test_chunked_parsing();
    test_response_builder();
    return 0;
}

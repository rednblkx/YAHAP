#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <span>
#include <cstdint>

namespace hap::transport {

enum class Method {
    GET,
    POST,
    PUT,
    DELETE,
    OPTIONS
};

enum class Status {
    OK = 200,
    NoContent = 204,
    MultiStatus = 207,
    BadRequest = 400,
    Unauthorized = 401,
    NotFound = 404,
    UnprocessableEntity = 422,
    MethodNotAllowed = 405,
    InternalServerError = 500,
    ServiceUnavailable = 503
};

// HTTP messages carry a handful of headers, so a flat vector beats a
// std::map on code size, allocation count and cache behavior. Lookup is a
// linear scan and keys keep insertion order in the serialized response.
using Headers = std::vector<std::pair<std::string, std::string>>;

inline const std::string* find_header(const Headers& headers, std::string_view key) {
    for (const auto& [k, v] : headers) {
        if (k == key) return &v;
    }
    return nullptr;
}

inline void set_header(Headers& headers, std::string key, std::string value) {
    for (auto& [k, v] : headers) {
        if (k == key) {
            v = std::move(value);
            return;
        }
    }
    headers.emplace_back(std::move(key), std::move(value));
}

struct Request {
    Method method;
    std::string path;
    Headers headers;
    std::vector<uint8_t> body;

    std::string get_header(std::string_view key) const {
        const std::string* v = find_header(headers, key);
        return v ? *v : "";
    }
};

struct Response {
    Status status;
    Headers headers;
    std::vector<uint8_t> body;

    Response(Status s = Status::OK) : status(s) {}

    void set_header(std::string key, std::string value) {
        transport::set_header(headers, std::move(key), std::move(value));
    }

    void set_body(std::vector<uint8_t> b) {
        body = std::move(b);
        set_header("Content-Length", std::to_string(body.size()));
    }

    void set_body(std::string_view text) {
        body.assign(text.begin(), text.end());
        set_header("Content-Length", std::to_string(body.size()));
    }
};

/**
 * @brief HTTP/1.1 Request Parser
 * Handles incremental parsing of TCP stream data.
 */
class HTTPParser {
public:
    HTTPParser();

    // Feed data from TCP stream
    // Returns true if a complete request was parsed
    bool feed(std::span<const uint8_t> data);

    // Get the parsed request (only valid after feed returns true)
    Request take_request();

    // Reset parser state for next request
    void reset();

private:
    // Parser limits: a malformed peer must not be able to grow the buffers
    // without bound (HAP requests are small JSON/TLV payloads).
    static constexpr size_t kMaxHeaderLineLength = 8 * 1024;
    static constexpr size_t kMaxRequestLineLength = 2 * 1024;
    static constexpr size_t kMaxHeaderCount = 64;
    static constexpr size_t kMaxBodyLength = 64 * 1024;

    enum class State {
        RequestLine,
        Headers,
        Body,
        Complete,
        Error
    };

    State state_;
    std::vector<uint8_t> buffer_;
    Request current_request_;
    size_t body_bytes_read_;
    size_t expected_body_length_;

    bool parse_request_line();
    bool parse_headers();
};

/**
 * @brief HTTP/1.1 Response Builder
 */
class HTTPBuilder {
public:
    static std::vector<uint8_t> build(const Response& response);
};

} // namespace hap::transport

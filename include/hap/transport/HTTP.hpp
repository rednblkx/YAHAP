#pragma once

#include <string>
#include <vector>
#include <map>
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

using Headers = std::map<std::string, std::string>;

struct Request {
    Method method;
    std::string path;
    Headers headers;
    std::vector<uint8_t> body;

    std::string get_header(const std::string& key) const {
        auto it = headers.find(key);
        return it != headers.end() ? it->second : "";
    }
};

struct Response {
    Status status;
    Headers headers;
    std::vector<uint8_t> body;

    Response(Status s = Status::OK) : status(s) {}

    void set_header(const std::string& key, const std::string& value) {
        headers[key] = value;
    }

    void set_body(std::vector<uint8_t> b) {
        body = std::move(b);
        headers["Content-Length"] = std::to_string(body.size());
    }

    void set_body(std::string_view text) {
        body.assign(text.begin(), text.end());
        headers["Content-Length"] = std::to_string(body.size());
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
    enum class State {
        RequestLine,
        Headers,
        Body,
        Complete
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

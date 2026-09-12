#include "hap/transport/HTTP.hpp"
#include <charconv>
#include <algorithm>
#include <iostream>
#include <sstream>

namespace hap::transport {

HTTPParser::HTTPParser() : state_(State::RequestLine), body_bytes_read_(0), expected_body_length_(0) {}

bool HTTPParser::feed(std::span<const uint8_t> data) {
    buffer_.insert(buffer_.end(), data.begin(), data.end());

    while (state_ != State::Complete) {
        if (state_ == State::Error) {
            return false; // Sticky error state until reset()
        }
        if (state_ == State::RequestLine) {
            if (!parse_request_line()) {
                // Incomplete so far; fail if the peer exceeds the request-line
                // budget without terminating it.
                if (buffer_.size() > kMaxRequestLineLength) {
                    state_ = State::Error;
                }
                return false;
            }
        } else if (state_ == State::Headers) {
            if (!parse_headers()) {
                if (buffer_.size() > kMaxHeaderLineLength ||
                    current_request_.headers.size() > kMaxHeaderCount) {
                    state_ = State::Error;
                }
                return false;
            }
        } else if (state_ == State::Body) {
            size_t remaining = expected_body_length_ - body_bytes_read_;
            size_t available = buffer_.size();
            size_t to_read = std::min(remaining, available);

            current_request_.body.insert(current_request_.body.end(), buffer_.begin(), buffer_.begin() + to_read);
            buffer_.erase(buffer_.begin(), buffer_.begin() + to_read);
            body_bytes_read_ += to_read;

            if (body_bytes_read_ >= expected_body_length_) {
                state_ = State::Complete;
            }
        }
    }

    return state_ == State::Complete;
}

Request HTTPParser::take_request() {
    return std::move(current_request_);
}

void HTTPParser::reset() {
    state_ = State::RequestLine;
    buffer_.clear();
    current_request_ = Request{};
    body_bytes_read_ = 0;
    expected_body_length_ = 0;
}

bool HTTPParser::parse_request_line() {
    // Look for \r\n
    const char crlf[] = "\r\n";
    auto it = std::search(buffer_.begin(), buffer_.end(), crlf, crlf + 2);
    if (it == buffer_.end()) return false;

    std::string line(buffer_.begin(), it);
    buffer_.erase(buffer_.begin(), it + 2);

    // Parse "METHOD /path HTTP/1.1"
    std::istringstream iss(line);
    std::string method_str, path, version;
    iss >> method_str >> path >> version;

    if (method_str == "GET") current_request_.method = Method::GET;
    else if (method_str == "POST") current_request_.method = Method::POST;
    else if (method_str == "PUT") current_request_.method = Method::PUT;
    else if (method_str == "DELETE") current_request_.method = Method::DELETE;
    else if (method_str == "OPTIONS") current_request_.method = Method::OPTIONS;
    else {
        state_ = State::Error;
        return false;
    }

    current_request_.path = path;
    state_ = State::Headers;
    return true;
}

bool HTTPParser::parse_headers() {
    const char crlf[] = "\r\n";
    while (true) {
        auto it = std::search(buffer_.begin(), buffer_.end(), crlf, crlf + 2);
        if (it == buffer_.end()) return false;

        std::string line(buffer_.begin(), it);
        buffer_.erase(buffer_.begin(), it + 2);

        if (line.size() > kMaxHeaderLineLength) {
            state_ = State::Error;
            return false;
        }

        if (line.empty()) {
            // End of headers
            // Check for Content-Length
            auto content_length_it = current_request_.headers.find("Content-Length");
            if (content_length_it != current_request_.headers.end()) {
                const std::string& cl = content_length_it->second;
                size_t content_length = 0;
                auto [ptr, ec] = std::from_chars(cl.data(), cl.data() + cl.size(), content_length);
                if (ec != std::errc() || ptr != cl.data() + cl.size() ||
                    content_length > kMaxBodyLength) {
                    // Malformed or oversized Content-Length: reject the request
                    // instead of throwing or buffering without bound.
                    state_ = State::Error;
                    return false;
                }
                expected_body_length_ = content_length;
                state_ = State::Body;
            } else {
                state_ = State::Complete;
            }
            return true;
        }

        // Parse "Key: Value"
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            // Trim whitespace from value
            value.erase(0, value.find_first_not_of(" \t"));
            current_request_.headers[key] = value;
        }
    }
}

std::vector<uint8_t> HTTPBuilder::build(const Response& response) {
    std::ostringstream ss;
    
    ss << "HTTP/1.1 " << static_cast<int>(response.status);
    switch (response.status) {
        case Status::OK: ss << " OK"; break;
        case Status::NoContent: ss << " No Content"; break;
        case Status::MultiStatus: ss << " Multi-Status"; break;
        case Status::BadRequest: ss << " Bad Request"; break;
        case Status::Unauthorized: ss << " Unauthorized"; break;
        case Status::NotFound: ss << " Not Found"; break;
        case Status::UnprocessableEntity: ss << " Unprocessable Entity"; break;
        case Status::MethodNotAllowed: ss << " Method Not Allowed"; break;
        case Status::InternalServerError: ss << " Internal Server Error"; break;
        case Status::ServiceUnavailable: ss << " Service Unavailable"; break;
    }
    ss << "\r\n";

    for (const auto& [key, value] : response.headers) {
        ss << key << ": " << value << "\r\n";
    }

    ss << "\r\n";

    std::string header_str = ss.str();
    std::vector<uint8_t> result(header_str.begin(), header_str.end());
    result.insert(result.end(), response.body.begin(), response.body.end());

    return result;
}

} // namespace hap::transport

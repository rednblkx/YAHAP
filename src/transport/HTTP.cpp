#include "hap/transport/HTTP.hpp"
#include <charconv>
#include <algorithm>

namespace hap::transport {

namespace {

// Splits a request line "METHOD /path HTTP/1.1" into its three tokens.
// Returns the number of tokens found (3 = well-formed).
size_t split_request_line(const std::string& line, std::string_view parts[3]) {
    size_t count = 0;
    size_t pos = 0;
    while (count < 3 && pos < line.size()) {
        size_t space = line.find(' ', pos);
        if (space == std::string::npos) space = line.size();
        if (space > pos) {
            parts[count++] = std::string_view(line).substr(pos, space - pos);
        }
        pos = space + 1;
    }
    return count;
}

bool parse_method(std::string_view token, Method& out) {
    if (token == "GET") { out = Method::GET; return true; }
    if (token == "POST") { out = Method::POST; return true; }
    if (token == "PUT") { out = Method::PUT; return true; }
    if (token == "DELETE") { out = Method::DELETE; return true; }
    if (token == "OPTIONS") { out = Method::OPTIONS; return true; }
    return false;
}

} // namespace

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
    std::string_view parts[3];
    size_t token_count = split_request_line(line, parts);
    Method method;
    if (token_count < 2 || !parse_method(parts[0], method)) {
        state_ = State::Error;
        return false;
    }

    current_request_.method = method;
    current_request_.path.assign(parts[1]);
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
            const std::string* cl_ptr = find_header(current_request_.headers, "Content-Length");
            if (cl_ptr) {
                const std::string& cl = *cl_ptr;
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
            set_header(current_request_.headers, std::move(key), std::move(value));
        }
    }
}

std::vector<uint8_t> HTTPBuilder::build(const Response& response) {
    static constexpr const char* kStatusTexts[] = {
        " OK", " No Content", " Multi-Status", " Bad Request", " Unauthorized",
        " Not Found", " Unprocessable Entity", " Method Not Allowed",
        " Internal Server Error", " Service Unavailable"
    };

    std::string header_str;
    header_str.reserve(96 + response.headers.size() * 24 + response.body.size());

    header_str += "HTTP/1.1 ";
    int status = static_cast<int>(response.status);
    char digits[8];
    auto [ptr, ec] = std::to_chars(digits, digits + sizeof(digits), status);
    header_str.append(digits, ptr);
    header_str += kStatusTexts[status - static_cast<int>(Status::OK)];
    header_str += "\r\n";

    for (const auto& [key, value] : response.headers) {
        header_str += key;
        header_str += ": ";
        header_str += value;
        header_str += "\r\n";
    }

    header_str += "\r\n";

    std::vector<uint8_t> result(header_str.begin(), header_str.end());
    result.insert(result.end(), response.body.begin(), response.body.end());

    return result;
}

} // namespace hap::transport

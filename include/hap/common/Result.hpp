#pragma once

#include <string>
#include <variant>
#include <optional>

namespace hap::common {

/**
 * @brief Error code enumeration for HAP operations.
 */
enum class ErrorCode {
    None = 0,
    InvalidParameter,
    InvalidState,
    NotFound,
    AuthenticationFailed,
    EncryptionFailed,
    DecryptionFailed,
    ParseError,
    BufferTooSmall,
    Timeout,
    NotSupported,
    InternalError
};

/**
 * @brief Error information for failed operations.
 */
struct Error {
    ErrorCode code = ErrorCode::None;
    std::string message;
    
    Error() = default;
    Error(ErrorCode c) : code(c) {}
    Error(ErrorCode c, std::string msg) : code(c), message(std::move(msg)) {}
    
    explicit operator bool() const { return code != ErrorCode::None; }
};

/**
 * @brief Exception-free result type for operations that may fail.
 * 
 * Provides a safe, embedded-friendly alternative to exceptions.
 * Uses std::variant internally for zero-overhead type safety.
 * 
 * Usage:
 * @code
 * Result<int> divide(int a, int b) {
 *     if (b == 0) return Result<int>::err(ErrorCode::InvalidParameter, "Division by zero");
 *     return Result<int>::ok(a / b);
 * }
 * 
 * auto result = divide(10, 2);
 * if (result.has_value()) {
 *     use(result.value());
 * } else {
 *     log(result.error().message);
 * }
 * @endcode
 */
template<typename T>
class Result {
public:
    Result() : data_(Error{ErrorCode::InternalError, "Uninitialized result"}) {}
    
    /**
     * @brief Create a successful result.
     */
    static Result ok(T value) {
        Result r;
        r.data_ = std::move(value);
        return r;
    }
    
    /**
     * @brief Create a failed result.
     */
    static Result err(ErrorCode code, std::string message = {}) {
        Result r;
        r.data_ = Error{code, std::move(message)};
        return r;
    }
    
    static Result err(Error e) {
        Result r;
        r.data_ = std::move(e);
        return r;
    }
    
    /**
     * @brief Check if the result contains a value.
     */
    [[nodiscard]] bool has_value() const {
        return std::holds_alternative<T>(data_);
    }
    
    /**
     * @brief Check if the result contains an error.
     */
    [[nodiscard]] bool has_error() const {
        return std::holds_alternative<Error>(data_);
    }
    
    /**
     * @brief Boolean conversion - true if has value.
     */
    explicit operator bool() const { return has_value(); }
    
    /**
     * @brief Get the contained value. Undefined behavior if has_error().
     */
    [[nodiscard]] T& value() & {
        return std::get<T>(data_);
    }
    
    [[nodiscard]] const T& value() const& {
        return std::get<T>(data_);
    }
    
    [[nodiscard]] T&& value() && {
        return std::get<T>(std::move(data_));
    }
    
    /**
     * @brief Get the contained error. Undefined behavior if has_value().
     */
    [[nodiscard]] const Error& error() const {
        return std::get<Error>(data_);
    }
    
    /**
     * @brief Get value or return a default.
     */
    [[nodiscard]] T value_or(T&& default_value) const& {
        if (has_value()) return value();
        return std::forward<T>(default_value);
    }
    
    [[nodiscard]] T value_or(T&& default_value) && {
        if (has_value()) return std::move(*this).value();
        return std::forward<T>(default_value);
    }

private:
    std::variant<T, Error> data_;
};

/**
 * @brief Specialization for void results (success/failure only).
 */
template<>
class Result<void> {
public:
    Result() : error_(std::nullopt) {}
    
    static Result ok() {
        return Result{};
    }
    
    static Result err(ErrorCode code, std::string message = {}) {
        Result r;
        r.error_ = Error{code, std::move(message)};
        return r;
    }
    
    static Result err(Error e) {
        Result r;
        r.error_ = std::move(e);
        return r;
    }
    
    [[nodiscard]] bool has_value() const { return !error_.has_value(); }
    [[nodiscard]] bool has_error() const { return error_.has_value(); }
    explicit operator bool() const { return has_value(); }
    
    [[nodiscard]] const Error& error() const { return *error_; }

private:
    std::optional<Error> error_;
};

} // namespace hap::common

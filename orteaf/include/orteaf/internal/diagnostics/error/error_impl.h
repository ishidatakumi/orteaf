#pragma once

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <utility>

namespace orteaf::internal::diagnostics::error {

inline const char* OrteafErrorCategory::name() const noexcept { return "orteaf"; }

inline std::string OrteafErrorCategory::message(int condition) const {
    switch (static_cast<OrteafErrc>(condition)) {
        case OrteafErrc::Success:
            return "success";
        case OrteafErrc::Unknown:
            return "unknown error";
        case OrteafErrc::InvalidArgument:
            return "invalid argument";
        case OrteafErrc::InvalidState:
            return "invalid state";
        case OrteafErrc::BackendUnavailable:
            return "backend unavailable";
        case OrteafErrc::OutOfMemory:
            return "out of memory";
        case OrteafErrc::OperationFailed:
            return "operation failed";
        default:
            return "unrecognized orteaf error";
    }
}

inline const std::error_category& orteaf_error_category() {
    static OrteafErrorCategory category;
    return category;
}

inline std::error_code make_error_code(OrteafErrc errc) {
    return {static_cast<int>(errc), orteaf_error_category()};
}

inline OrteafError::OrteafError()
    : code_(make_error_code(OrteafErrc::Unknown)) {}

inline OrteafError::OrteafError(std::error_code ec, std::string message)
    : code_(std::move(ec)), message_(std::move(message)) {}

inline OrteafError::OrteafError(OrteafErrc errc, std::string message)
    : OrteafError(make_error_code(errc), std::move(message)) {}

inline OrteafErrc OrteafError::errc() const noexcept {
    return static_cast<OrteafErrc>(code_.value());
}

inline std::string_view OrteafError::detail() const noexcept {
    return message_;
}

inline std::string OrteafError::describe() const {
    const auto base = code_.message();
    if (message_.empty()) {
        return base;
    }
    std::string out;
    out.reserve(base.size() + 2 + message_.size());
    out.append(base);
    out.append(": ");
    out.append(message_);
    return out;
}

inline const std::error_code& OrteafError::code() const noexcept {
    return code_;
}

inline const std::string& OrteafError::message() const noexcept {
    return message_;
}

inline void OrteafError::set_code(std::error_code ec) {
    code_ = std::move(ec);
}

inline void OrteafError::set_code(OrteafErrc errc) {
    code_ = make_error_code(errc);
}

inline void OrteafError::set_message(std::string message) {
    message_ = std::move(message);
}

inline OrteafError make_error(OrteafErrc errc, std::string message) {
    return OrteafError(errc, std::move(message));
}

inline OrteafError make_error(std::error_code ec, std::string message) {
    return OrteafError(std::move(ec), std::move(message));
}

inline void throw_error(const OrteafError& error) {
    throw std::system_error(error.code(), error.describe());
}

inline void throw_error(OrteafErrc errc, std::string message) {
    throw_error(make_error(errc, std::move(message)));
}

inline void throw_error(std::error_code ec, std::string message) {
    throw_error(make_error(ec, std::move(message)));
}

inline void fatal_error(const OrteafError& error) {
    std::fprintf(stderr, "[ORTEAF][FATAL] %s\n", error.describe().c_str());
    std::abort();
}

inline void fatal_error(OrteafErrc errc, std::string message) {
    fatal_error(make_error(errc, std::move(message)));
}

inline void fatal_error(std::error_code ec, std::string message) {
    fatal_error(make_error(ec, std::move(message)));
}

namespace detail {

template <typename T>
inline OrteafResultImpl<T> OrteafResultImpl<T>::success(T value) {
    return OrteafResultImpl(std::in_place_index<0>, std::move(value));
}

template <typename T>
inline OrteafResultImpl<T> OrteafResultImpl<T>::failure(OrteafError error) {
    return OrteafResultImpl(std::in_place_index<1>, std::move(error));
}

template <typename T>
inline bool OrteafResultImpl<T>::has_value() const noexcept {
    return value_.has_value();
}

template <typename T>
inline bool OrteafResultImpl<T>::has_error() const noexcept {
    return error_.has_value();
}

template <typename T>
inline T& OrteafResultImpl<T>::value() & {
    if (!value_) {
        throw_error(error());
    }
    return *value_;
}

template <typename T>
inline const T& OrteafResultImpl<T>::value() const& {
    if (!value_) {
        throw_error(error());
    }
    return *value_;
}

template <typename T>
inline T&& OrteafResultImpl<T>::value() && {
    if (!value_) {
        throw_error(error());
    }
    return std::move(*value_);
}

template <typename T>
inline T OrteafResultImpl<T>::value_or(T default_value) const& {
    if (value_) {
        return *value_;
    }
    return std::move(default_value);
}

template <typename T>
inline OrteafError OrteafResultImpl<T>::error() const {
    if (!error_) {
        throw_error(make_error(OrteafErrc::Success, "result has no error"));
    }
    return *error_;
}

template <typename T>
template <typename V>
inline OrteafResultImpl<T>::OrteafResultImpl(std::in_place_index_t<0>, V&& value)
    : value_(std::forward<V>(value)) {}

template <typename T>
inline OrteafResultImpl<T>::OrteafResultImpl(std::in_place_index_t<1>, OrteafError error)
    : error_(std::move(error)) {}

inline OrteafResultImpl<void> OrteafResultImpl<void>::success() {
    return OrteafResultImpl();
}

inline OrteafResultImpl<void> OrteafResultImpl<void>::failure(OrteafError error) {
    OrteafResultImpl result;
    result.error_ = std::move(error);
    result.has_value_ = false;
    return result;
}

inline bool OrteafResultImpl<void>::has_value() const noexcept {
    return has_value_;
}

inline bool OrteafResultImpl<void>::has_error() const noexcept {
    return error_.has_value();
}

inline void OrteafResultImpl<void>::value() const {
    if (!has_value_) {
        throw_error(error());
    }
}

inline void OrteafResultImpl<void>::value_or() const {
    value();
}

inline OrteafError OrteafResultImpl<void>::error() const {
    if (!error_) {
        throw_error(make_error(OrteafErrc::Success, "result has no error"));
    }
    return *error_;
}

inline OrteafResultImpl<void>::OrteafResultImpl() = default;

}  // namespace detail

template <typename T>
inline OrteafResult<T> OrteafResult<T>::success(T value) {
    return OrteafResult(detail::OrteafResultImpl<T>::success(std::move(value)));
}

template <typename T>
inline OrteafResult<T> OrteafResult<T>::failure(OrteafErrc errc, std::string message) {
    return OrteafResult(detail::OrteafResultImpl<T>::failure(make_error(errc, std::move(message))));
}

template <typename T>
inline OrteafResult<T> OrteafResult<T>::failure(OrteafError error) {
    return OrteafResult(detail::OrteafResultImpl<T>::failure(std::move(error)));
}

template <typename T>
template <typename... Args>
inline OrteafResult<T> OrteafResult<T>::failure_with(OrteafErrc errc, Args&&... args) {
    return failure(make_error(errc, std::string(std::forward<Args>(args)...)));
}

template <typename T>
inline bool OrteafResult<T>::has_value() const noexcept {
    return impl_.has_value();
}

template <typename T>
inline bool OrteafResult<T>::has_error() const noexcept {
    return impl_.has_error();
}

template <typename T>
inline T& OrteafResult<T>::value() & {
    return impl_.value();
}

template <typename T>
inline const T& OrteafResult<T>::value() const& {
    return impl_.value();
}

template <typename T>
inline T&& OrteafResult<T>::value() && {
    return std::move(impl_).value();
}

template <typename T>
template <typename U>
inline T OrteafResult<T>::value_or(U&& default_value) const& {
    if (impl_.has_value()) {
        return impl_.value();
    }
    return static_cast<T>(std::forward<U>(default_value));
}

template <typename T>
inline OrteafError OrteafResult<T>::error() const {
    return impl_.error();
}

template <typename T>
inline OrteafResult<T>::OrteafResult(detail::OrteafResultImpl<T>&& impl)
    : impl_(std::move(impl)) {}

inline OrteafResult<void> OrteafResult<void>::success() {
    return OrteafResult(detail::OrteafResultImpl<void>::success());
}

inline OrteafResult<void> OrteafResult<void>::failure(OrteafErrc errc, std::string message) {
    return OrteafResult(detail::OrteafResultImpl<void>::failure(make_error(errc, std::move(message))));
}

inline OrteafResult<void> OrteafResult<void>::failure(OrteafError error) {
    return OrteafResult(detail::OrteafResultImpl<void>::failure(std::move(error)));
}

inline bool OrteafResult<void>::has_value() const noexcept {
    return impl_.has_value();
}

inline bool OrteafResult<void>::has_error() const noexcept {
    return impl_.has_error();
}

inline void OrteafResult<void>::value() const {
    impl_.value();
}

inline void OrteafResult<void>::value_or() const {
    impl_.value();
}

inline OrteafError OrteafResult<void>::error() const {
    return impl_.error();
}

inline OrteafResult<void>::OrteafResult(detail::OrteafResultImpl<void>&& impl)
    : impl_(std::move(impl)) {}

template <typename Fn>
auto capture_result(Fn&& fn) -> OrteafResult<std::invoke_result_t<Fn>> {
    using ReturnT = std::invoke_result_t<Fn>;
    try {
        if constexpr (std::is_void_v<ReturnT>) {
            std::invoke(std::forward<Fn>(fn));
            return OrteafResult<void>::success();
        } else {
            return OrteafResult<ReturnT>::success(std::invoke(std::forward<Fn>(fn)));
        }
    } catch (const std::system_error& ex) {
        auto err = make_error(ex.code(), ex.what());
        if constexpr (std::is_void_v<ReturnT>) {
            return OrteafResult<void>::failure(std::move(err));
        } else {
            return OrteafResult<ReturnT>::failure(std::move(err));
        }
    } catch (const std::exception& ex) {
        auto err = make_error(OrteafErrc::Unknown, ex.what());
        if constexpr (std::is_void_v<ReturnT>) {
            return OrteafResult<void>::failure(std::move(err));
        } else {
            return OrteafResult<ReturnT>::failure(std::move(err));
        }
    } catch (...) {
        auto err = make_error(OrteafErrc::Unknown, "unknown non-std exception");
        if constexpr (std::is_void_v<ReturnT>) {
            return OrteafResult<void>::failure(std::move(err));
        } else {
            return OrteafResult<ReturnT>::failure(std::move(err));
        }
    }
}

template <typename T>
inline T unwrap_or_throw(OrteafResult<T>&& result) {
    if (result.has_value()) {
        return std::move(result).value();
    }
    throw_error(result.error());
}

inline void unwrap_or_throw(OrteafResult<void>&& result) {
    if (result.has_value()) {
        return;
    }
    throw_error(result.error());
}

inline OrteafResult<void> capture_result(void (*fn)()) {
    return capture_result<std::function<void()>>([fn] {
        fn();
    });
}

}  // namespace orteaf::internal::diagnostics::error

#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::diagnostics::log {

enum class LogLevel : int {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Critical = 5,
    Off = 6
};

enum class LogCategory : int {
    Core,
    Tensor,
    Cuda,
    Mps,
    Io
};

namespace detail {

inline constexpr int kLogLevelGlobal =
#ifdef ORTEAF_LOG_LEVEL_GLOBAL_VALUE
    ORTEAF_LOG_LEVEL_GLOBAL_VALUE;
#else
    static_cast<int>(LogLevel::Off);
#endif

inline constexpr int kLogLevelCore =
#ifdef ORTEAF_LOG_LEVEL_CORE_VALUE
    ORTEAF_LOG_LEVEL_CORE_VALUE;
#else
    kLogLevelGlobal;
#endif

inline constexpr int kLogLevelTensor =
#ifdef ORTEAF_LOG_LEVEL_TENSOR_VALUE
    ORTEAF_LOG_LEVEL_TENSOR_VALUE;
#else
    kLogLevelGlobal;
#endif

inline constexpr int kLogLevelCuda =
#ifdef ORTEAF_LOG_LEVEL_CUDA_VALUE
    ORTEAF_LOG_LEVEL_CUDA_VALUE;
#else
    kLogLevelGlobal;
#endif

inline constexpr int kLogLevelMps =
#ifdef ORTEAF_LOG_LEVEL_MPS_VALUE
    ORTEAF_LOG_LEVEL_MPS_VALUE;
#else
    kLogLevelGlobal;
#endif

inline constexpr int kLogLevelIo =
#ifdef ORTEAF_LOG_LEVEL_IO_VALUE
    ORTEAF_LOG_LEVEL_IO_VALUE;
#else
    kLogLevelGlobal;
#endif
;

}  // namespace detail

constexpr int level_to_int(LogLevel level) {
    return static_cast<int>(level);
}

template <LogCategory Category>
constexpr int category_threshold();

template <>
constexpr int category_threshold<LogCategory::Core>() {
    return detail::kLogLevelCore;
}

template <>
constexpr int category_threshold<LogCategory::Tensor>() {
    return detail::kLogLevelTensor;
}

template <>
constexpr int category_threshold<LogCategory::Cuda>() {
    return detail::kLogLevelCuda;
}

template <>
constexpr int category_threshold<LogCategory::Mps>() {
    return detail::kLogLevelMps;
}

template <>
constexpr int category_threshold<LogCategory::Io>() {
    return detail::kLogLevelIo;
}

using LogSink = void (*)(LogCategory category, LogLevel level, std::string_view message, void* context);

void set_log_sink(LogSink sink, void* context = nullptr);
void reset_log_sink();

namespace detail {

void log_message(LogCategory category, LogLevel level, std::string message);

template <LogCategory Category, LogLevel Level, typename MessageBuilder>
inline void log_lazy(MessageBuilder&& builder) {
    if constexpr (level_to_int(Level) >= category_threshold<Category>()) {
        log_message(Category, Level, std::forward<MessageBuilder>(builder)());
    }
}

}  // namespace detail

constexpr const char* level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:
            return "TRACE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Critical:
            return "CRITICAL";
        case LogLevel::Off:
        default:
            return "OFF";
    }
}

constexpr const char* category_to_string(LogCategory category) {
    switch (category) {
        case LogCategory::Core:
            return "core";
        case LogCategory::Tensor:
            return "tensor";
        case LogCategory::Cuda:
            return "cuda";
        case LogCategory::Mps:
            return "mps";
        case LogCategory::Io:
        default:
            return "io";
    }
}

}  // namespace orteaf::internal::diagnostics::log

#define ORTEAF_LOG_INTERNAL(category, level, expr)                                                        \
    ::orteaf::internal::diagnostics::log::detail::log_lazy<                                                \
        ::orteaf::internal::diagnostics::log::LogCategory::category,                                       \
        ::orteaf::internal::diagnostics::log::LogLevel::level>(                                            \
        [&]() -> std::string { return std::string(expr); })

#define ORTEAF_LOG_TRACE(category, expr) ORTEAF_LOG_INTERNAL(category, Trace, expr)
#define ORTEAF_LOG_DEBUG(category, expr) ORTEAF_LOG_INTERNAL(category, Debug, expr)
#define ORTEAF_LOG_INFO(category, expr) ORTEAF_LOG_INTERNAL(category, Info, expr)
#define ORTEAF_LOG_WARN(category, expr) ORTEAF_LOG_INTERNAL(category, Warn, expr)
#define ORTEAF_LOG_ERROR(category, expr) ORTEAF_LOG_INTERNAL(category, Error, expr)
#define ORTEAF_LOG_CRITICAL(category, expr) ORTEAF_LOG_INTERNAL(category, Critical, expr)

#define ORTEAF_ASSERT(expr, message)                                                                        \
    do {                                                                                                    \
        if (!(expr)) {                                                                                      \
            const std::string _orteaf_assert_message = std::string(message);                                \
            ORTEAF_LOG_CRITICAL(Core, _orteaf_assert_message);                                              \
            ::orteaf::internal::diagnostics::error::fatal_error(                                            \
                ::orteaf::internal::diagnostics::error::make_error(                                         \
                    ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,                       \
                    _orteaf_assert_message));                                                               \
        }                                                                                                   \
    } while (0)

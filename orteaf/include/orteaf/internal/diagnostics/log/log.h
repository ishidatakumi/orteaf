#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::diagnostics::log {

/**
 * @brief Log severity levels.
 *
 * Log levels are ordered from most verbose (Trace) to least verbose (Off).
 * Higher numeric values indicate more severe or important messages.
 */
enum class LogLevel : int {
    Trace = 0,      ///< Most verbose level for detailed tracing information.
    Debug = 1,      ///< Debug information useful for development.
    Info = 2,       ///< Informational messages about normal operations.
    Warn = 3,       ///< Warning messages for potentially problematic situations.
    Error = 4,      ///< Error messages for error conditions.
    Critical = 5,   ///< Critical error messages requiring immediate attention.
    Off = 6         ///< Disables all logging output.
};

/**
 * @brief Log message categories.
 *
 * Categories allow filtering and separate threshold configuration for different
 * subsystems of the library.
 */
enum class LogCategory : int {
    Core,   ///< Core library functionality.
    Tensor, ///< Tensor operations and management.
    Cuda,   ///< CUDA-specific operations.
    Mps,    ///< Metal Performance Shaders (MPS) operations.
    Io      ///< Input/output operations.
};

namespace detail {

/**
 * @brief Global log level threshold.
 *
 * Default threshold used when category-specific thresholds are not set.
 * Controlled by the `ORTEAF_LOG_LEVEL_GLOBAL_VALUE` compile-time macro.
 * If not defined, defaults to `LogLevel::Off`.
 */
inline constexpr int kLogLevelGlobal =
#ifdef ORTEAF_LOG_LEVEL_GLOBAL_VALUE
    ORTEAF_LOG_LEVEL_GLOBAL_VALUE;
#else
    static_cast<int>(LogLevel::Off);
#endif

/**
 * @brief Core category log level threshold.
 *
 * Controlled by the `ORTEAF_LOG_LEVEL_CORE_VALUE` compile-time macro.
 * If not defined, falls back to `kLogLevelGlobal`.
 */
inline constexpr int kLogLevelCore =
#ifdef ORTEAF_LOG_LEVEL_CORE_VALUE
    ORTEAF_LOG_LEVEL_CORE_VALUE;
#else
    kLogLevelGlobal;
#endif

/**
 * @brief Tensor category log level threshold.
 *
 * Controlled by the `ORTEAF_LOG_LEVEL_TENSOR_VALUE` compile-time macro.
 * If not defined, falls back to `kLogLevelGlobal`.
 */
inline constexpr int kLogLevelTensor =
#ifdef ORTEAF_LOG_LEVEL_TENSOR_VALUE
    ORTEAF_LOG_LEVEL_TENSOR_VALUE;
#else
    kLogLevelGlobal;
#endif

/**
 * @brief CUDA category log level threshold.
 *
 * Controlled by the `ORTEAF_LOG_LEVEL_CUDA_VALUE` compile-time macro.
 * If not defined, falls back to `kLogLevelGlobal`.
 */
inline constexpr int kLogLevelCuda =
#ifdef ORTEAF_LOG_LEVEL_CUDA_VALUE
    ORTEAF_LOG_LEVEL_CUDA_VALUE;
#else
    kLogLevelGlobal;
#endif

/**
 * @brief MPS category log level threshold.
 *
 * Controlled by the `ORTEAF_LOG_LEVEL_MPS_VALUE` compile-time macro.
 * If not defined, falls back to `kLogLevelGlobal`.
 */
inline constexpr int kLogLevelMps =
#ifdef ORTEAF_LOG_LEVEL_MPS_VALUE
    ORTEAF_LOG_LEVEL_MPS_VALUE;
#else
    kLogLevelGlobal;
#endif

/**
 * @brief I/O category log level threshold.
 *
 * Controlled by the `ORTEAF_LOG_LEVEL_IO_VALUE` compile-time macro.
 * If not defined, falls back to `kLogLevelGlobal`.
 */
inline constexpr int kLogLevelIo =
#ifdef ORTEAF_LOG_LEVEL_IO_VALUE
    ORTEAF_LOG_LEVEL_IO_VALUE;
#else
    kLogLevelGlobal;
#endif
;

}  // namespace detail

/**
 * @brief Convert a LogLevel enum value to its integer representation.
 *
 * @param level The log level to convert.
 * @return Integer value corresponding to the log level.
 */
constexpr int levelToInt(LogLevel level) {
    return static_cast<int>(level);
}

/**
 * @brief Get the log level threshold for a specific category.
 *
 * This template function returns the compile-time configured threshold for the
 * specified log category. The threshold determines the minimum log level that
 * will be processed for messages in that category.
 *
 * @tparam Category The log category to get the threshold for.
 * @return Integer threshold value for the category.
 */
template <LogCategory Category>
constexpr int categoryThreshold();

/**
 * @brief Specialization for Core category threshold.
 * @return Core category log level threshold.
 */
template <>
constexpr int categoryThreshold<LogCategory::Core>() {
    return detail::kLogLevelCore;
}

/**
 * @brief Specialization for Tensor category threshold.
 * @return Tensor category log level threshold.
 */
template <>
constexpr int categoryThreshold<LogCategory::Tensor>() {
    return detail::kLogLevelTensor;
}

/**
 * @brief Specialization for CUDA category threshold.
 * @return CUDA category log level threshold.
 */
template <>
constexpr int categoryThreshold<LogCategory::Cuda>() {
    return detail::kLogLevelCuda;
}

/**
 * @brief Specialization for MPS category threshold.
 * @return MPS category log level threshold.
 */
template <>
constexpr int categoryThreshold<LogCategory::Mps>() {
    return detail::kLogLevelMps;
}

/**
 * @brief Specialization for I/O category threshold.
 * @return I/O category log level threshold.
 */
template <>
constexpr int categoryThreshold<LogCategory::Io>() {
    return detail::kLogLevelIo;
}

/**
 * @brief Function pointer type for custom log sinks.
 *
 * A log sink function receives log messages and can route them to custom
 * destinations (e.g., files, network, GUI, etc.).
 *
 * @param category The category of the log message.
 * @param level The severity level of the log message.
 * @param message The log message content.
 * @param context User-provided context pointer passed to setLogSink().
 */
using LogSink = void (*)(LogCategory category, LogLevel level, std::string_view message, void* context);

/**
 * @brief Set a custom log sink function.
 *
 * Sets a user-provided function to handle all log messages. If `sink` is `nullptr`,
 * logging falls back to the default sink (stderr).
 *
 * The sink function is called atomically, so it must be thread-safe if used in
 * multi-threaded environments.
 *
 * @param sink Pointer to the log sink function, or `nullptr` to use default.
 * @param context Optional user-provided context pointer passed to the sink function.
 */
void setLogSink(LogSink sink, void* context = nullptr);

/**
 * @brief Reset the log sink to the default behavior.
 *
 * Equivalent to calling `setLogSink(nullptr, nullptr)`.
 * After resetting, log messages are sent to stderr with default formatting.
 */
void resetLogSink();

namespace detail {

/**
 * @brief Internal function to log a message.
 *
 * Routes the message to the configured sink (if set) or the default sink.
 * This function is thread-safe.
 *
 * @param category The category of the log message.
 * @param level The severity level of the log message.
 * @param message The log message content.
 */
void logMessage(LogCategory category, LogLevel level, std::string message);

/**
 * @brief Lazy-evaluated logging function.
 *
 * Evaluates the message builder only if the log level meets the category threshold.
 * This allows expensive string formatting operations to be skipped when logging
 * is disabled for the given category/level combination.
 *
 * @tparam Category The log category.
 * @tparam Level The log level.
 * @tparam MessageBuilder Callable type that returns a std::string when invoked.
 * @param builder Functor or lambda that builds the log message.
 */
template <LogCategory Category, LogLevel Level, typename MessageBuilder>
inline void logLazy(MessageBuilder&& builder) {
    if constexpr (levelToInt(Level) >= categoryThreshold<Category>()) {
        logMessage(Category, Level, std::forward<MessageBuilder>(builder)());
    }
}

/**
 * @brief Lazy-evaluated logging function with conditional check.
 *
 * Evaluates both the condition and message builder only if the log level meets
 * the category threshold. This allows expensive condition checks and string
 * formatting operations to be skipped when logging is disabled.
 *
 * @tparam Category The log category.
 * @tparam Level The log level.
 * @tparam ConditionBuilder Callable type that returns a bool when invoked.
 * @tparam MessageBuilder Callable type that returns a std::string when invoked.
 * @param condition_builder Functor or lambda that builds the condition check.
 * @param message_builder Functor or lambda that builds the log message.
 */
template <LogCategory Category, LogLevel Level, typename ConditionBuilder, typename MessageBuilder>
inline void logLazyIf(ConditionBuilder&& condition_builder, MessageBuilder&& message_builder) {
    if constexpr (levelToInt(Level) >= categoryThreshold<Category>()) {
        if (std::forward<ConditionBuilder>(condition_builder)()) {
            logMessage(Category, Level, std::forward<MessageBuilder>(message_builder)());
        }
    }
}

}  // namespace detail

/**
 * @brief Convert a LogLevel enum value to its string representation.
 *
 * @param level The log level to convert.
 * @return String representation of the log level (e.g., "TRACE", "DEBUG", "INFO").
 */
constexpr const char* levelToString(LogLevel level) {
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

/**
 * @brief Convert a LogCategory enum value to its string representation.
 *
 * @param category The log category to convert.
 * @return String representation of the category (e.g., "core", "tensor", "cuda").
 */
constexpr const char* categoryToString(LogCategory category) {
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

/**
 * @def ORTEAF_LOG_INTERNAL(category, level, expr)
 * @brief Internal macro for lazy-evaluated logging.
 *
 * This macro is used internally by the public logging macros. It creates a lambda
 * that builds the log message only if the log level meets the category threshold.
 *
 * @param category Log category (e.g., `Core`, `Tensor`, `Cuda`).
 * @param level Log level (e.g., `Trace`, `Debug`, `Info`).
 * @param expr Expression that evaluates to the log message (e.g., string literal or function call).
 */

/**
 * @def ORTEAF_LOG_TRACE(category, expr)
 * @brief Log a message at TRACE level.
 *
 * @param category Log category (e.g., `Core`, `Tensor`, `Cuda`).
 * @param expr Expression that evaluates to the log message.
 */

/**
 * @def ORTEAF_LOG_DEBUG(category, expr)
 * @brief Log a message at DEBUG level.
 *
 * @param category Log category (e.g., `Core`, `Tensor`, `Cuda`).
 * @param expr Expression that evaluates to the log message.
 */

/**
 * @def ORTEAF_LOG_INFO(category, expr)
 * @brief Log a message at INFO level.
 *
 * @param category Log category (e.g., `Core`, `Tensor`, `Cuda`).
 * @param expr Expression that evaluates to the log message.
 */

/**
 * @def ORTEAF_LOG_WARN(category, expr)
 * @brief Log a message at WARN level.
 *
 * @param category Log category (e.g., `Core`, `Tensor`, `Cuda`).
 * @param expr Expression that evaluates to the log message.
 */

/**
 * @def ORTEAF_LOG_ERROR(category, expr)
 * @brief Log a message at ERROR level.
 *
 * @param category Log category (e.g., `Core`, `Tensor`, `Cuda`).
 * @param expr Expression that evaluates to the log message.
 */

/**
 * @def ORTEAF_LOG_CRITICAL(category, expr)
 * @brief Log a message at CRITICAL level.
 *
 * @param category Log category (e.g., `Core`, `Tensor`, `Cuda`).
 * @param expr Expression that evaluates to the log message.
 */

#define ORTEAF_LOG_INTERNAL(category, level, expr)                                                        \
    ::orteaf::internal::diagnostics::log::detail::logLazy<                                                \
        ::orteaf::internal::diagnostics::log::LogCategory::category,                                       \
        ::orteaf::internal::diagnostics::log::LogLevel::level>(                                            \
        [&]() -> std::string { return std::string(expr); })

#define ORTEAF_LOG_TRACE(category, expr) ORTEAF_LOG_INTERNAL(category, Trace, expr)
#define ORTEAF_LOG_DEBUG(category, expr) ORTEAF_LOG_INTERNAL(category, Debug, expr)
#define ORTEAF_LOG_INFO(category, expr) ORTEAF_LOG_INTERNAL(category, Info, expr)
#define ORTEAF_LOG_WARN(category, expr) ORTEAF_LOG_INTERNAL(category, Warn, expr)
#define ORTEAF_LOG_ERROR(category, expr) ORTEAF_LOG_INTERNAL(category, Error, expr)
#define ORTEAF_LOG_CRITICAL(category, expr) ORTEAF_LOG_INTERNAL(category, Critical, expr)

/**
 * @def ORTEAF_LOG_TRACE_IF(category, condition, expr)
 * @brief Log a message at TRACE level if condition is true.
 *
 * The condition is only evaluated if logging is enabled for this category/level.
 *
 * @param category Log category (e.g., `Core`, `Tensor`, `Cuda`).
 * @param condition Boolean expression that determines if the log should be emitted.
 * @param expr Expression that evaluates to the log message.
 */

/**
 * @def ORTEAF_LOG_DEBUG_IF(category, condition, expr)
 * @brief Log a message at DEBUG level if condition is true.
 *
 * The condition is only evaluated if logging is enabled for this category/level.
 *
 * @param category Log category (e.g., `Core`, `Tensor`, `Cuda`).
 * @param condition Boolean expression that determines if the log should be emitted.
 * @param expr Expression that evaluates to the log message.
 */

/**
 * @def ORTEAF_LOG_INFO_IF(category, condition, expr)
 * @brief Log a message at INFO level if condition is true.
 *
 * The condition is only evaluated if logging is enabled for this category/level.
 *
 * @param category Log category (e.g., `Core`, `Tensor`, `Cuda`).
 * @param condition Boolean expression that determines if the log should be emitted.
 * @param expr Expression that evaluates to the log message.
 */

/**
 * @def ORTEAF_LOG_WARN_IF(category, condition, expr)
 * @brief Log a message at WARN level if condition is true.
 *
 * The condition is only evaluated if logging is enabled for this category/level.
 *
 * @param category Log category (e.g., `Core`, `Tensor`, `Cuda`).
 * @param condition Boolean expression that determines if the log should be emitted.
 * @param expr Expression that evaluates to the log message.
 */

/**
 * @def ORTEAF_LOG_ERROR_IF(category, condition, expr)
 * @brief Log a message at ERROR level if condition is true.
 *
 * The condition is only evaluated if logging is enabled for this category/level.
 *
 * @param category Log category (e.g., `Core`, `Tensor`, `Cuda`).
 * @param condition Boolean expression that determines if the log should be emitted.
 * @param expr Expression that evaluates to the log message.
 */

/**
 * @def ORTEAF_LOG_CRITICAL_IF(category, condition, expr)
 * @brief Log a message at CRITICAL level if condition is true.
 *
 * The condition is only evaluated if logging is enabled for this category/level.
 *
 * @param category Log category (e.g., `Core`, `Tensor`, `Cuda`).
 * @param condition Boolean expression that determines if the log should be emitted.
 * @param expr Expression that evaluates to the log message.
 */

#define ORTEAF_LOG_INTERNAL_IF(category, level, condition, expr)                                             \
    ::orteaf::internal::diagnostics::log::detail::logLazyIf<                                                \
        ::orteaf::internal::diagnostics::log::LogCategory::category,                                         \
        ::orteaf::internal::diagnostics::log::LogLevel::level>(                                              \
        [&]() -> bool { return (condition); },                                                               \
        [&]() -> std::string { return std::string(expr); })

#define ORTEAF_LOG_TRACE_IF(category, condition, expr) ORTEAF_LOG_INTERNAL_IF(category, Trace, condition, expr)
#define ORTEAF_LOG_DEBUG_IF(category, condition, expr) ORTEAF_LOG_INTERNAL_IF(category, Debug, condition, expr)
#define ORTEAF_LOG_INFO_IF(category, condition, expr) ORTEAF_LOG_INTERNAL_IF(category, Info, condition, expr)
#define ORTEAF_LOG_WARN_IF(category, condition, expr) ORTEAF_LOG_INTERNAL_IF(category, Warn, condition, expr)
#define ORTEAF_LOG_ERROR_IF(category, condition, expr) ORTEAF_LOG_INTERNAL_IF(category, Error, condition, expr)
#define ORTEAF_LOG_CRITICAL_IF(category, condition, expr) ORTEAF_LOG_INTERNAL_IF(category, Critical, condition, expr)

/**
 * @def ORTEAF_ASSERT(expr, message)
 * @brief Assertion macro with logging and error handling.
 *
 * If the expression evaluates to false, this macro:
 * 1. Logs a CRITICAL level message to the Core category.
 * 2. Calls fatalError() with an InvalidState error code.
 *
 * This macro is intended for runtime assertions that indicate an invalid program state.
 * The program will terminate after logging and error handling.
 *
 * @param expr Boolean expression to assert.
 * @param message String message describing the assertion failure.
 */
#define ORTEAF_ASSERT(expr, message)                                                                        \
    do {                                                                                                    \
        if (!(expr)) {                                                                                      \
            const std::string _orteaf_assert_message = std::string(message);                                \
            ORTEAF_LOG_CRITICAL(Core, _orteaf_assert_message);                                              \
            ::orteaf::internal::diagnostics::error::fatalError(                                            \
                ::orteaf::internal::diagnostics::error::makeError(                                         \
                    ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,                       \
                    _orteaf_assert_message));                                                               \
        }                                                                                                   \
    } while (0)

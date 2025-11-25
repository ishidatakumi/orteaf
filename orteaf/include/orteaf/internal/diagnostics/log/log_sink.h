#pragma once

/**
 * @file log_sink.h
 * @brief ログシンク（出力先）の設定と内部ログ関数。
 */

#include <string>
#include <string_view>
#include <utility>

#include "orteaf/internal/diagnostics/log/log_types.h"

namespace orteaf::internal::diagnostics::log {

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

}  // namespace orteaf::internal::diagnostics::log

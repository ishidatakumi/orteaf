#include "orteaf/internal/diagnostics/log/log.h"

#include <atomic>
#include <cstdio>
#include <string>
#include <string_view>

namespace orteaf::internal::diagnostics::log {

namespace {

/**
 * @brief Global log sink function pointer.
 *
 * Thread-safe atomic pointer to the user-provided log sink function.
 * If nullptr, the default sink (stderr) is used.
 */
std::atomic<LogSink> g_sink{nullptr};

/**
 * @brief Global context pointer for the log sink.
 *
 * Thread-safe atomic pointer to user-provided context data.
 * This pointer is passed to the log sink function when logging messages.
 */
std::atomic<void*> g_context{nullptr};

/**
 * @brief Default log sink function.
 *
 * Writes log messages to stderr with the format:
 * `[ORTEAF][category][level] message`
 *
 * @param category The log category.
 * @param level The log severity level.
 * @param message The log message content.
 */
void defaultSink(LogCategory category, LogLevel level, std::string_view message) {
    std::fprintf(stderr, "[ORTEAF][%s][%s] %.*s\n",
                 categoryToString(category),
                 levelToString(level),
                 static_cast<int>(message.size()),
                 message.data());
}

}  // namespace

/**
 * @brief Set a custom log sink function.
 *
 * Implementation of setLogSink() declared in log.h.
 * Atomically stores the sink function pointer and context using release memory ordering.
 *
 * @param sink Pointer to the log sink function, or nullptr to use default.
 * @param context Optional user-provided context pointer passed to the sink function.
 */
void setLogSink(LogSink sink, void* context) {
    g_context.store(context, std::memory_order_release);
    g_sink.store(sink, std::memory_order_release);
}

/**
 * @brief Reset the log sink to the default behavior.
 *
 * Implementation of resetLogSink() declared in log.h.
 * Equivalent to calling setLogSink(nullptr, nullptr).
 */
void resetLogSink() {
    setLogSink(nullptr, nullptr);
}

namespace detail {

/**
 * @brief Internal function to log a message.
 *
 * Implementation of logMessage() declared in log.h.
 * Routes the message to the configured sink (if set) or the default sink.
 * Uses acquire memory ordering to read the sink and context atomically.
 *
 * @param category The category of the log message.
 * @param level The severity level of the log message.
 * @param message The log message content.
 */
void logMessage(LogCategory category, LogLevel level, std::string message) {
    if (auto sink = g_sink.load(std::memory_order_acquire)) {
        sink(category, level, message, g_context.load(std::memory_order_acquire));
        return;
    }
    defaultSink(category, level, message);
}

}  // namespace detail

}  // namespace orteaf::internal::diagnostics::log

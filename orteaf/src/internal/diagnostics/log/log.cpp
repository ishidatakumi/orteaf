#include "orteaf/internal/diagnostics/log/log.h"

#include <atomic>
#include <cstdio>
#include <string>
#include <string_view>

namespace orteaf::internal::diagnostics::log {

namespace {

std::atomic<LogSink> g_sink{nullptr};
std::atomic<void*> g_context{nullptr};

void default_sink(LogCategory category, LogLevel level, std::string_view message) {
    std::fprintf(stderr, "[ORTEAF][%s][%s] %.*s\n",
                 category_to_string(category),
                 level_to_string(level),
                 static_cast<int>(message.size()),
                 message.data());
}

}  // namespace

void set_log_sink(LogSink sink, void* context) {
    g_context.store(context, std::memory_order_release);
    g_sink.store(sink, std::memory_order_release);
}

void reset_log_sink() {
    set_log_sink(nullptr, nullptr);
}

namespace detail {

void log_message(LogCategory category, LogLevel level, std::string message) {
    if (auto sink = g_sink.load(std::memory_order_acquire)) {
        sink(category, level, message, g_context.load(std::memory_order_acquire));
        return;
    }
    default_sink(category, level, message);
}

}  // namespace detail

}  // namespace orteaf::internal::diagnostics::log

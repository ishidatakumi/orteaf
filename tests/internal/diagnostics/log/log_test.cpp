#include "orteaf/internal/diagnostics/log/log.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

using namespace orteaf::internal::diagnostics;

namespace {

struct SinkCapture {
    std::vector<std::string> messages;
};

void test_sink(log::LogCategory, log::LogLevel, std::string_view message, void* context) {
    auto* capture = static_cast<SinkCapture*>(context);
    capture->messages.emplace_back(message);
}

}  // namespace

TEST(DiagnosticsLog, InfoEmitsWhenEnabled) {
    SinkCapture capture;
    log::set_log_sink(&test_sink, &capture);

    capture.messages.clear();
    ORTEAF_LOG_INFO(Core, "info message");

    log::reset_log_sink();

    ASSERT_EQ(capture.messages.size(), 1u);
    EXPECT_EQ(capture.messages[0], "info message");
}

TEST(DiagnosticsLog, TraceIsCompiledOutWhenDisabled) {
    SinkCapture capture;
    log::set_log_sink(&test_sink, &capture);

    capture.messages.clear();
    ORTEAF_LOG_TRACE(Core, "trace message");

    log::reset_log_sink();

    EXPECT_TRUE(capture.messages.empty());
}

#if GTEST_HAS_DEATH_TEST
TEST(DiagnosticsLog, AssertTriggersFatal) {
    auto trigger = [] { ORTEAF_ASSERT(false, "assert failure"); };
    EXPECT_DEATH(trigger(), "assert failure");
}
#endif

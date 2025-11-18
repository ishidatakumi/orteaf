#include "orteaf/internal/diagnostics/error/error.h"

#include <gtest/gtest.h>

namespace diag = orteaf::internal::diagnostics::error;

TEST(DiagnosticsError, ErrorCodeCategoryBasics) {
    auto& category = diag::orteafErrorCategory();
    EXPECT_STREQ("orteaf", category.name());

    std::error_code code = diag::makeErrorCode(diag::OrteafErrc::InvalidArgument);
    EXPECT_EQ(diag::OrteafErrc::InvalidArgument, static_cast<diag::OrteafErrc>(code.value()));
    EXPECT_EQ(&category, &code.category());
    EXPECT_FALSE(code.message().empty());
}

TEST(DiagnosticsError, OrteafErrorCapturesCodeAndMessage) {
    auto err = diag::makeError(diag::OrteafErrc::BackendUnavailable, "cuda backend disabled");
    EXPECT_EQ(diag::OrteafErrc::BackendUnavailable, err.errc());
    EXPECT_EQ(&diag::orteafErrorCategory(), &err.code().category());
    const auto describe = err.describe();
    EXPECT_NE(std::string::npos, describe.find("cuda backend disabled"));
}

TEST(DiagnosticsError, ThrowErrorHelperThrowsOrteafError) {
    try {
        diag::throwError(diag::OrteafErrc::OutOfMemory, "allocation failed");
        FAIL() << "Expected std::system_error to be thrown";
    } catch (const std::system_error& ex) {
        EXPECT_EQ(diag::OrteafErrc::OutOfMemory, static_cast<diag::OrteafErrc>(ex.code().value()));
        EXPECT_NE(std::string::npos, std::string(ex.what()).find("allocation failed"));
    }
}

TEST(DiagnosticsError, ResultSuccessHoldsValue) {
    auto result = diag::OrteafResult<int>::success(42);
    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result.has_error());
    EXPECT_EQ(42, result.value());
    EXPECT_EQ(42, result.value_or(-1));
}

TEST(DiagnosticsError, ResultErrorHoldsError) {
    auto result = diag::OrteafResult<int>::failure(diag::OrteafErrc::OperationFailed, "op fail");
    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.has_error());
    auto err = result.error();
    EXPECT_EQ(diag::OrteafErrc::OperationFailed, err.errc());
    EXPECT_NE(std::string::npos, err.describe().find("op fail"));
    EXPECT_THROW(result.value(), std::system_error);
}

TEST(DiagnosticsError, CaptureResultPropagatesValue) {
    auto result = diag::captureResult([] { return 99; });
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(99, result.value());
}

TEST(DiagnosticsError, CaptureResultPropagatesOrteafError) {
    auto result = diag::captureResult([]() -> int {
        diag::throwError(diag::OrteafErrc::BackendUnavailable, "backend down");
        return 0;
    });
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(diag::OrteafErrc::BackendUnavailable, result.error().errc());
}

TEST(DiagnosticsError, CaptureResultMapsStdExceptionToUnknown) {
    auto result = diag::captureResult([]() -> int {
        throw std::logic_error("logic");
    });
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(diag::OrteafErrc::Unknown, result.error().errc());
}

TEST(DiagnosticsError, CaptureResultVoidSpecialization) {
    auto result = diag::captureResult([]() { /* noop */ });
    EXPECT_TRUE(result.has_value());
    EXPECT_NO_THROW(result.value());
}

TEST(DiagnosticsError, UnwrapOrThrowReturnsOrThrows) {
    auto ok = diag::OrteafResult<int>::success(7);
    EXPECT_EQ(7, diag::unwrapOrThrow(std::move(ok)));

    auto bad = diag::OrteafResult<int>::failure(diag::OrteafErrc::InvalidState, "bad state");
    EXPECT_THROW(diag::unwrapOrThrow(std::move(bad)), std::system_error);
}

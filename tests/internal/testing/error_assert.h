#pragma once

#include "orteaf/internal/diagnostics/error/error.h"

#include <gtest/gtest.h>

#include <system_error>
#include <initializer_list>
#include <string_view>

namespace orteaf::tests {

/**
 * @brief Assert helper that verifies a callable throws std::system_error with the expected OrteafErrc.
 *
 * Usage:
 *   ExpectError(OrteafErrc::NullPointer, [] { someFunction(nullptr); });
 */
template <typename Fn>
void ExpectError(::orteaf::internal::diagnostics::error::OrteafErrc errc, Fn&& fn) {
    namespace diag = ::orteaf::internal::diagnostics::error;
    const auto expected = diag::makeErrorCode(errc);
    EXPECT_THROW(
        try {
            fn();
        } catch (const std::system_error& ex) {
            EXPECT_EQ(ex.code(), expected);
            throw;
        },
        std::system_error);
}

/**
 * @brief Assert helper that also verifies the exception message contains specific substrings.
 */
template <typename Fn>
void ExpectErrorMessage(::orteaf::internal::diagnostics::error::OrteafErrc errc,
                        std::initializer_list<std::string_view> substrings,
                        Fn&& fn) {
    namespace diag = ::orteaf::internal::diagnostics::error;
    const auto expected = diag::makeErrorCode(errc);
    EXPECT_THROW(
        try {
            fn();
        } catch (const std::system_error& ex) {
            EXPECT_EQ(ex.code(), expected);
            std::string_view what(ex.what());
            for (std::string_view needle : substrings) {
                if (!needle.empty()) {
                    EXPECT_NE(what.find(needle), std::string_view::npos);
                }
            }
            throw;
        },
        std::system_error);
}

template <typename Exception, typename Fn>
void ExpectException(Fn&& fn) {
    EXPECT_THROW(fn(), Exception);
}

}  // namespace orteaf::tests

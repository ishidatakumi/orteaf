/**
 * @file mps_string.h
 * @brief Thin bridge utilities for NSString conversions.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include <string_view>

#include "orteaf/internal/execution/mps/platform/wrapper/mps_types.h"

namespace orteaf::internal::execution::mps::platform::wrapper {

/**
 * @brief Convert std::string_view to NSString*.
 * Tries UTF-8 encoding first, then falls back to ISO Latin-1.
 */
[[nodiscard]] MpsString_t toNsString(std::string_view view);

} // namespace orteaf::internal::execution::mps::platform::wrapper

#endif  // ORTEAF_ENABLE_MPS

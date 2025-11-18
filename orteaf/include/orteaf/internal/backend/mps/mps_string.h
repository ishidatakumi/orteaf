/**
 * @file mps_string.h
 * @brief Thin bridge utilities for NSString conversions.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include <string_view>

namespace orteaf::internal::backend::mps {

struct MPSString_st; using MPSString_t = MPSString_st*;

static_assert(sizeof(MPSString_t) == sizeof(void*), "MPSString must be pointer-sized.");

/**
 * @brief Convert std::string_view to NSString*.
 * Tries UTF-8 encoding first, then falls back to ISO Latin-1.
 */
[[nodiscard]] MPSString_t toNsString(std::string_view view);

} // namespace orteaf::internal::backend::mps

#endif  // ORTEAF_ENABLE_MPS

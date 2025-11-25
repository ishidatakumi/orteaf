#pragma once

#include <cstddef>

namespace orteaf::internal::base {

/**
 * @brief Returns whether the given value is a non-zero power of two.
 */
constexpr bool isPowerOfTwo(std::size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

/**
 * @brief Calculates the smallest power of two greater than or equal to @p value.
 */
constexpr std::size_t nextPowerOfTwo(std::size_t value) {
    if (value <= 1) {
        return 1;
    }
    --value;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    if constexpr (sizeof(std::size_t) == 8) {
        value |= value >> 32;
    }
    return value + 1;
}

/**
 * @brief Aligns @p value down to the nearest multiple of @p alignment (alignment must be non-zero).
 */
constexpr std::size_t alignDown(std::size_t value, std::size_t alignment) {
    return alignment == 0 ? value : value - (value % alignment);
}

}  // namespace orteaf::internal::base

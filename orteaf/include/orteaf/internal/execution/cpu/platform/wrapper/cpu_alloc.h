#pragma once

/**
 * @file cpu_alloc.h
 * @brief CPU memory allocation functions and utilities.
 *
 * This header provides functions for allocating and deallocating CPU memory
 * with configurable alignment. All allocation and deallocation operations
 * automatically update CPU statistics when available.
 */

#include <orteaf/internal/execution/cpu/platform/wrapper/cpu_stats.h>
#include <orteaf/internal/base/math_utils.h>
#include <orteaf/internal/diagnostics/error/error.h>

#include <cstdlib>
#include <new>
#include <cstdint>
#include <cstddef>

namespace orteaf::internal::execution::cpu::platform::wrapper {

/**
 * @brief Default alignment value for CPU memory allocation.
 *
 * Based on the alignment requirements of `std::max_align_t`.
 * This value satisfies the platform's standard alignment requirements.
 */
constexpr std::size_t kCpuDefaultAlign = alignof(std::max_align_t);

/**
 * @brief Allocate memory with default CPU alignment.
 *
 * Wrapper for `allocAligned(size, kCpuDefaultAlign)`.
 * Statistics are automatically updated on allocation.
 *
 * @param size Size of memory to allocate in bytes.
 * @return Pointer to allocated memory; throws std::bad_alloc on failure.
 * @throws std::system_error If @p size is 0 (OrteafErrc::InvalidParameter).
 * @throws std::bad_alloc If memory allocation fails.
 */
inline void* alloc(std::size_t size);

/**
 * @brief Allocate memory with the specified alignment.
 *
 * Uses the following APIs depending on the platform:
 * - Windows (`_MSC_VER`): `_aligned_malloc`
 * - Others: `posix_memalign`
 *
 * If the alignment is not a power of 2, it is automatically adjusted to the next power of 2.
 * Also, alignments smaller than `std::max_align_t` are adjusted to the minimum value.
 *
 * Statistics are automatically updated on allocation.
 *
 * @param size Size of memory to allocate in bytes.
 * @param alignment Requested alignment in bytes. Must be a power of 2.
 * @return Pointer to allocated memory.
 * @throws std::system_error If @p size is 0 (OrteafErrc::InvalidParameter).
 * @throws std::bad_alloc If memory allocation fails.
 */
inline void* allocAligned(std::size_t size, std::size_t alignment) {
    if (size == 0) {
        using namespace ::orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidParameter, "cpu::allocAligned: size cannot be 0");
    }

    const std::size_t min_align = alignof(std::max_align_t);
    if (alignment < min_align) alignment = min_align;
    if (!::orteaf::internal::base::isPowerOfTwo(alignment)) {
        alignment = ::orteaf::internal::base::nextPowerOfTwo(alignment);
    }

#if defined(_MSC_VER)
    void* p = _aligned_malloc(size, alignment);
    if (!p) throw std::bad_alloc();
#else
    // aligned_alloc は size が alignment の倍数要件あり→posix_memalign優先
    void* p = nullptr;
    const int rc = ::posix_memalign(&p, alignment, size);
    if (rc != 0 || !p) throw std::bad_alloc();
#endif

    updateAlloc(size);
    return p;
}

inline void* alloc(std::size_t size) {
    return allocAligned(size, kCpuDefaultAlign);
}

/**
 * @brief Free allocated memory.
 *
 * Uses the following APIs depending on the platform:
 * - Windows (`_MSC_VER`): `_aligned_free`
 * - Others: `free`
 *
 * Does nothing if `ptr` is `nullptr`.
 * Statistics are automatically updated on deallocation.
 *
 * @param ptr Pointer to memory to free. Does nothing if `nullptr`.
 * @param size Size of memory to free in bytes. Used for statistics update.
 */
inline void dealloc(void* ptr, std::size_t size) noexcept {
    if (!ptr) return;
    updateDealloc(size);
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    ::free(ptr);
#endif
}

} // namespace orteaf::internal::execution::cpu::platform::wrapper
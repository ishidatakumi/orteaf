#pragma once

/**
 * @file cpu_alloc.h
 * @brief CPU memory allocation functions and utilities.
 *
 * This header provides functions for allocating and deallocating CPU memory
 * with configurable alignment. All allocation and deallocation operations
 * automatically update CPU statistics when available.
 */

#include "orteaf/internal/backend/cpu/cpu_stats.h"

#include <cstdlib>
#include <new>
#include <cstdint>
#include <cstddef>

namespace orteaf::internal::backend::cpu {

/**
 * @brief Default alignment value for CPU memory allocation.
 *
 * Based on the alignment requirements of `std::max_align_t`.
 * This value satisfies the platform's standard alignment requirements.
 */
constexpr std::size_t kCpuDefaultAlign = alignof(std::max_align_t);

/**
 * @brief Check if the specified value is a power of 2.
 *
 * Uses bitwise operations for efficient checking.
 *
 * @param x Value to check.
 * @return `true` if `x` is a power of 2, `false` otherwise.
 *         Returns `false` if `x` is 0.
 */
inline bool is_pow2(std::size_t x) { return x && ((x & (x-1))==0); }

/**
 * @brief Calculate the smallest power of 2 greater than or equal to the specified value.
 *
 * Uses bit manipulation for efficient calculation.
 * Used for alignment adjustment, etc.
 *
 * @param x Base value.
 * @return Smallest power of 2 greater than or equal to `x`.
 *         Returns 1 if `x` is 0 or 1.
 */
inline std::size_t next_pow2(std::size_t x){
    if (x<=1) return 1u;
    --x; x|=x>>1; x|=x>>2; x|=x>>4; x|=x>>8; x|=x>>16;
    if constexpr (sizeof(std::size_t)==8) x|=x>>32;
    return x+1;
}

/**
 * @brief Forward declaration of alloc_aligned.
 */
inline void* alloc_aligned(std::size_t size, std::size_t alignment);

/**
 * @brief Allocate memory with default CPU alignment.
 *
 * Wrapper for `alloc_aligned(size, kCpuDefaultAlign)`.
 * Statistics are automatically updated on allocation.
 *
 * @param size Size of memory to allocate in bytes.
 * @return Pointer to allocated memory; throws std::bad_alloc on failure.
 * @throws std::bad_alloc If memory allocation fails.
 */
inline void* alloc(std::size_t size) {
    return alloc_aligned(size, kCpuDefaultAlign);
}

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
 * @return Pointer to allocated memory. Returns `nullptr` if `size` is 0.
 *         Throws `std::bad_alloc` on failure.
 * @throws std::bad_alloc If memory allocation fails.
 */
inline void* alloc_aligned(std::size_t size, std::size_t alignment) {
    if (size == 0) return nullptr;

    const std::size_t min_align = alignof(std::max_align_t);
    if (alignment < min_align) alignment = min_align;
    if (!is_pow2(alignment)) alignment = next_pow2(alignment);

#if defined(_MSC_VER)
    void* p = _aligned_malloc(size, alignment);
    if (!p) throw std::bad_alloc();
#else
    // aligned_alloc は size が alignment の倍数要件あり→posix_memalign優先
    void* p = nullptr;
    const int rc = ::posix_memalign(&p, alignment, size);
    if (rc != 0 || !p) throw std::bad_alloc();
#endif

    update_alloc(size);
    return p;
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
    update_dealloc(size);
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    ::free(ptr);
#endif
}

} // namespace orteaf::internal::backend::cpu
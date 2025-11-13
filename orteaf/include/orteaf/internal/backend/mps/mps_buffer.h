/**
 * @file mps_buffer.h
 * @brief MPS/Metal buffer creation, destruction, and CPU-access helpers.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/backend/mps/mps_heap.h"

#include <cstddef>

namespace orteaf::internal::backend::mps {

struct MPSBuffer_st; using MPSBuffer_t = MPSBuffer_st*;
using MPSBufferUsage_t = unsigned long;
inline constexpr MPSBufferUsage_t kMPSDefaultBufferUsage = 0;

static_assert(sizeof(MPSBuffer_t) == sizeof(void*), "MPSBuffer must be pointer-sized.");

/**
 * @brief Create a new Metal buffer.
 * @param heap Opaque Metal heap handle
 * @param size Buffer length in bytes
 * @param usage Resource options bitmask (defaults to 0)
 * @return Opaque buffer handle, or nullptr when unavailable/disabled.
 */
MPSBuffer_t createBuffer(MPSHeap_t heap, size_t size, MPSBufferUsage_t usage = kMPSDefaultBufferUsage);

/**
 * @brief Destroy a Metal buffer; ignores nullptr.
 */
void destroyBuffer(MPSBuffer_t buffer);

/**
 * @brief Get raw CPU pointer to buffer contents (const).
 */
const void* getBufferContentsConst(MPSBuffer_t buffer);

/**
 * @brief Get raw CPU pointer to buffer contents (mutable).
 */
void* getBufferContents(MPSBuffer_t buffer);

} // namespace orteaf::internal::backend::mps

#endif  // ORTEAF_ENABLE_MPS

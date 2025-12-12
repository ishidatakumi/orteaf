/**
 * @file mps_buffer.h
 * @brief MPS/Metal buffer creation, destruction, and CPU-access helpers.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_types.h"

namespace orteaf::internal::runtime::mps::platform::wrapper {

inline constexpr MpsBufferUsage_t kMPSDefaultBufferUsage = 0;

/**
 * @brief Create a new Metal buffer.
 * @param heap Opaque Metal heap handle
 * @param size Buffer length in bytes
 * @param usage Resource options bitmask (defaults to 0)
 * @return Opaque buffer handle, or nullptr when unavailable/disabled.
 */
MpsBuffer_t createBuffer(MpsHeap_t heap, size_t size,
                         MpsBufferUsage_t usage = kMPSDefaultBufferUsage);

/**
 * @brief Create a new Metal buffer at an explicit heap offset.
 * @param heap Opaque Metal heap handle
 * @param size Buffer length in bytes
 * @param offset Byte offset into the heap; must satisfy the heap's alignment rules
 * @param usage Resource options bitmask (defaults to 0)
 * @return Opaque buffer handle, or nullptr when unavailable/disabled.
 */
MpsBuffer_t createBufferWithOffset(
    MpsHeap_t heap, size_t size, size_t offset,
    MpsBufferUsage_t usage = kMPSDefaultBufferUsage);

/**
 * @brief Destroy a Metal buffer; ignores nullptr.
 */
void destroyBuffer(MpsBuffer_t buffer);

/**
 * @brief Get raw CPU pointer to buffer contents (const).
 */
const void *getBufferContentsConst(MpsBuffer_t buffer);

/**
 * @brief Get raw CPU pointer to buffer contents (mutable).
 */
void *getBufferContents(MpsBuffer_t buffer);

} // namespace orteaf::internal::runtime::mps::platform::wrapper

#endif  // ORTEAF_ENABLE_MPS

/**
 * @file mps_command_queue.h
 * @brief MPS/Metal command queue creation and destruction helpers.
 *
 * Thin wrappers over Metal's `MTLCommandQueue`. When MPS is disabled or
 * Objective-C is unavailable, functions exist but behave as no-ops and
 * return neutral values.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/backend/mps/mps_device.h"

namespace orteaf::internal::backend::mps {

struct MPSCommandQueue_st; using MPSCommandQueue_t = MPSCommandQueue_st*;

static_assert(sizeof(MPSCommandQueue_t) == sizeof(void*), "MPSCommandQueue must be pointer-sized.");

/**
 * @brief Create a command queue for a device.
 * @param device Opaque Metal device handle
 * @return Opaque command queue handle, or nullptr when unavailable/disabled.
 */
MPSCommandQueue_t createCommandQueue(MPSDevice_t device);

/**
 * @brief Destroy a command queue.
 * @param command_queue Opaque queue handle; nullptr is ignored.
 */
void destroyCommandQueue(MPSCommandQueue_t command_queue);

} // namespace orteaf::internal::backend::mps

#endif  // ORTEAF_ENABLE_MPS
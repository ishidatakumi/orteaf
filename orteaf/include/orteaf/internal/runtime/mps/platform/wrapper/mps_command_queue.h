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

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_types.h"

namespace orteaf::internal::runtime::mps::platform::wrapper {

/**
 * @brief Create a command queue for a device.
 * @param device Opaque Metal device handle
 * @return Opaque command queue handle, or nullptr when unavailable/disabled.
 */
MpsCommandQueue_t createCommandQueue(MpsDevice_t device);

/**
 * @brief Destroy a command queue.
 * @param command_queue Opaque queue handle; nullptr is ignored.
 */
void destroyCommandQueue(MpsCommandQueue_t command_queue);

} // namespace orteaf::internal::runtime::mps::platform::wrapper

#endif  // ORTEAF_ENABLE_MPS

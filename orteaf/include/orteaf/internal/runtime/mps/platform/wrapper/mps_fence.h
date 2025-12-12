/**
 * @file mps_fence.h
 * @brief MPS/Metal fence helpers (create/destroy).
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_types.h"

namespace orteaf::internal::runtime::mps::platform::wrapper {

/** Create a fence for the given device. */
MpsFence_t createFence(MpsDevice_t device);

/** Destroy a fence; ignores nullptr. */
void destroyFence(MpsFence_t fence);

/** Encode an update of the fence on the provided compute encoder. */
void updateFence(MpsComputeCommandEncoder_t encoder, MpsFence_t fence);

/** Encode a wait for the fence on the provided compute encoder. */
void waitForFence(MpsComputeCommandEncoder_t encoder, MpsFence_t fence);

}  // namespace orteaf::internal::runtime::mps::platform::wrapper

#endif  // ORTEAF_ENABLE_MPS

/**
 * @file mps_fence.h
 * @brief MPS/Metal fence helpers (create/destroy).
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_compute_command_encorder.h"

namespace orteaf::internal::backend::mps {

struct MPSFence_st;
using MPSFence_t = MPSFence_st*;

static_assert(sizeof(MPSFence_t) == sizeof(void*), "MPSFence_t must be pointer-sized.");

/** Create a fence for the given device. */
MPSFence_t createFence(MPSDevice_t device);

/** Destroy a fence; ignores nullptr. */
void destroyFence(MPSFence_t fence);

/** Encode an update of the fence on the provided compute encoder. */
void updateFence(MPSComputeCommandEncoder_t encoder, MPSFence_t fence);

/** Encode a wait for the fence on the provided compute encoder. */
void waitForFence(MPSComputeCommandEncoder_t encoder, MPSFence_t fence);

}  // namespace orteaf::internal::backend::mps

#endif  // ORTEAF_ENABLE_MPS

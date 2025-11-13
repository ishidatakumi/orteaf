/**
 * @file mps_compute_pipeline_state.h
 * @brief MPS/Metal compute pipeline state creation and destruction.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/backend/mps/mps_function.h"
#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_error.h"

namespace orteaf::internal::backend::mps {

struct MPSComputePipelineState_st; using MPSComputePipelineState_t = MPSComputePipelineState_st*;

static_assert(sizeof(MPSComputePipelineState_t) == sizeof(void*), "MPSComputePipelineState must be pointer-sized.");

/**
 * @brief Create a compute pipeline state from a function.
 * @param device Opaque Metal device
 * @param function Opaque Metal function
 * @param error Optional error object out (bridged to NSError**)
 * @return Opaque pipeline state, or nullptr when unavailable/disabled.
 */
MPSComputePipelineState_t createComputePipelineState(MPSDevice_t device, MPSFunction_t function, MPSError_t* error = nullptr);

/**
 * @brief Destroy a pipeline state; ignores nullptr.
 */
void destroyComputePipelineState(MPSComputePipelineState_t pipeline_state);

} // namespace orteaf::internal::backend::mps

#endif  // ORTEAF_ENABLE_MPS

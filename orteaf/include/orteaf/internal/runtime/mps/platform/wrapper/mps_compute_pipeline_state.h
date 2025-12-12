/**
 * @file mps_compute_pipeline_state.h
 * @brief MPS/Metal compute pipeline state creation and destruction.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_types.h"

namespace orteaf::internal::runtime::mps::platform::wrapper {

/**
 * @brief Create a compute pipeline state from a function.
 * @param device Opaque Metal device
 * @param function Opaque Metal function
 * @param error Optional error object out (bridged to NSError**)
 * @return Opaque pipeline state, or nullptr when unavailable/disabled.
 */
MpsComputePipelineState_t createComputePipelineState(
    MpsDevice_t device, MpsFunction_t function, MpsError_t *error = nullptr);

/**
 * @brief Destroy a pipeline state; ignores nullptr.
 */
void destroyComputePipelineState(MpsComputePipelineState_t pipeline_state);

} // namespace orteaf::internal::runtime::mps::platform::wrapper

#endif  // ORTEAF_ENABLE_MPS

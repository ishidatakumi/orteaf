/**
 * @file mps_compute_command_encoder.h
 * @brief MPS/Metal compute command encoder helpers.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_types.h"

namespace orteaf::internal::runtime::mps::platform::wrapper {

/** Create a compute command encoder from a command buffer. */
MpsComputeCommandEncoder_t createComputeCommandEncoder(
    MpsCommandBuffer_t command_buffer);
/** Destroy a compute command encoder; ignores nullptr. */
void destroyComputeCommandEncoder(MpsComputeCommandEncoder_t compute_command_encoder);
/** End encoding on the compute command encoder. */
void endEncoding(MpsComputeCommandEncoder_t compute_command_encoder);

/** Bind a compute pipeline state. */
void setPipelineState(MpsComputeCommandEncoder_t compute_command_encoder,
                      MpsComputePipelineState_t pipeline_state);
/** Bind a buffer at the given index with an offset. */
void setBuffer(MpsComputeCommandEncoder_t compute_command_encoder,
               MpsBuffer_t buffer, size_t offset, size_t index);
/** Bind raw bytes at the given index. */
void setBytes(MpsComputeCommandEncoder_t compute_command_encoder,
              const void* bytes, size_t length, size_t index);
/** Dispatch threadgroups with the specified sizes. */
void setThreadgroups(MpsComputeCommandEncoder_t compute_command_encoder,
                     MpsSize_t threadgroups,
                     MpsSize_t threads_per_threadgroup);

} // namespace orteaf::internal::runtime::mps::platform::wrapper

#endif  // ORTEAF_ENABLE_MPS

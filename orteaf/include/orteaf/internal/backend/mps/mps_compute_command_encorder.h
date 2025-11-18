/**
 * @file mps_compute_command_encorder.h
 * @brief MPS/Metal compute command encoder helpers.
 */
#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/backend/mps/mps_command_buffer.h"
#include "orteaf/internal/backend/mps/mps_buffer.h"
#include "orteaf/internal/backend/mps/mps_compute_pipeline_state.h"
#include "orteaf/internal/backend/mps/mps_size.h"

#include <cstddef>

namespace orteaf::internal::backend::mps {

struct MPSComputeCommandEncoder_st; using MPSComputeCommandEncoder_t = MPSComputeCommandEncoder_st*;

static_assert(sizeof(MPSComputeCommandEncoder_t) == sizeof(void*), "MPSComputeCommandEncoder must be pointer-sized.");

/** Create a compute command encoder from a command buffer. */
MPSComputeCommandEncoder_t createComputeCommandEncoder(MPSCommandBuffer_t command_buffer);
/** Destroy a compute command encoder; ignores nullptr. */
void destroyComputeCommandEncoder(MPSComputeCommandEncoder_t compute_command_encoder);
/** End encoding on the compute command encoder. */
void endEncoding(MPSComputeCommandEncoder_t compute_command_encoder);

/** Bind a compute pipeline state. */
void setPipelineState(MPSComputeCommandEncoder_t compute_command_encoder, MPSComputePipelineState_t pipeline_state);
/** Bind a buffer at the given index with an offset. */
void setBuffer(MPSComputeCommandEncoder_t compute_command_encoder, MPSBuffer_t buffer, size_t offset, size_t index);
/** Bind raw bytes at the given index. */
void setBytes(MPSComputeCommandEncoder_t compute_command_encoder, const void* bytes, size_t length, size_t index);
/** Dispatch threadgroups with the specified sizes. */
void setThreadgroups(MPSComputeCommandEncoder_t compute_command_encoder,
                      MPSSize_t threadgroups,
                      MPSSize_t threads_per_threadgroup);

} // namespace orteaf::internal::backend::mps

#endif  // ORTEAF_ENABLE_MPS

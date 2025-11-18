/**
 * @file mps_compute_command_encorder.mm
 * @brief Implementation of MPS/Metal compute command encoder helpers.
 */
#ifndef __OBJC__
#error "mps_compute_command_encorder.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/backend/mps/mps_compute_command_encorder.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#import <Metal/Metal.h>
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::createComputeCommandEncoder
 */
MPSComputeCommandEncoder_t createComputeCommandEncoder(MPSCommandBuffer_t command_buffer) {
    if (command_buffer == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "createComputeCommandEncoder: command_buffer cannot be nullptr");
    }
    id<MTLCommandBuffer> objc_command_buffer = objcFromOpaqueNoown<id<MTLCommandBuffer>>(command_buffer);
    id<MTLComputeCommandEncoder> objc_encoder = [objc_command_buffer computeCommandEncoder];
    return (MPSComputeCommandEncoder_t)opaqueFromObjcRetained(objc_encoder);
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyComputeCommandEncoder
 */
void destroyComputeCommandEncoder(MPSComputeCommandEncoder_t compute_command_encoder) {
    if (compute_command_encoder == nullptr) return;
    opaqueReleaseRetained(compute_command_encoder);
}

/**
 * @copydoc orteaf::internal::backend::mps::setPipelineState
 */
void setPipelineState(MPSComputeCommandEncoder_t compute_command_encoder, MPSComputePipelineState_t pipeline_state) {
    if (compute_command_encoder == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "setPipelineState: compute_command_encoder cannot be nullptr");
    }
    if (pipeline_state == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "setPipelineState: pipeline_state cannot be nullptr");
    }
    id<MTLComputeCommandEncoder> objc_encoder = objcFromOpaqueNoown<id<MTLComputeCommandEncoder>>(compute_command_encoder);
    id<MTLComputePipelineState> objc_pipeline_state = objcFromOpaqueNoown<id<MTLComputePipelineState>>(pipeline_state);
    [objc_encoder setComputePipelineState:objc_pipeline_state];
}

/**
 * @copydoc orteaf::internal::backend::mps::endEncoding
 */
void endEncoding(MPSComputeCommandEncoder_t compute_command_encoder) {
    if (compute_command_encoder == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "endEncoding: compute_command_encoder cannot be nullptr");
    }
    id<MTLComputeCommandEncoder> objc_encoder = objcFromOpaqueNoown<id<MTLComputeCommandEncoder>>(compute_command_encoder);
    [objc_encoder endEncoding];
}

/**
 * @copydoc orteaf::internal::backend::mps::setBuffer
 */
void setBuffer(MPSComputeCommandEncoder_t compute_command_encoder, MPSBuffer_t buffer, size_t offset, size_t index) {
    if (compute_command_encoder == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "setBuffer: compute_command_encoder cannot be nullptr");
    }
    if (buffer == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "setBuffer: buffer cannot be nullptr");
    }
    id<MTLComputeCommandEncoder> objc_encoder = objcFromOpaqueNoown<id<MTLComputeCommandEncoder>>(compute_command_encoder);
    id<MTLBuffer> objc_buffer = objcFromOpaqueNoown<id<MTLBuffer>>(buffer);
    [objc_encoder setBuffer:objc_buffer offset:offset atIndex:index];
}

/**
 * @copydoc orteaf::internal::backend::mps::setBytes
 */
void setBytes(MPSComputeCommandEncoder_t compute_command_encoder, const void* bytes, size_t length, size_t index) {
    if (compute_command_encoder == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "setBytes: compute_command_encoder cannot be nullptr");
    }
    if (bytes == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "setBytes: bytes cannot be nullptr");
    }
    id<MTLComputeCommandEncoder> objc_encoder = objcFromOpaqueNoown<id<MTLComputeCommandEncoder>>(compute_command_encoder);
    [objc_encoder setBytes:bytes length:length atIndex:index];
}

/**
 * @copydoc orteaf::internal::backend::mps::setThreadgroups
 */
void setThreadgroups(MPSComputeCommandEncoder_t compute_command_encoder,
                      MPSSize_t threadgroups,
                      MPSSize_t threads_per_threadgroup) {
    if (compute_command_encoder == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "setThreadgroups: compute_command_encoder cannot be nullptr");
    }
    id<MTLComputeCommandEncoder> objc_encoder = objcFromOpaqueNoown<id<MTLComputeCommandEncoder>>(compute_command_encoder);
    const MTLSize objc_threadgroups = orteaf::internal::backend::mps::toMtlSize(threadgroups);
    const MTLSize objc_threads_per_threadgroup = orteaf::internal::backend::mps::toMtlSize(threads_per_threadgroup);
    [objc_encoder dispatchThreadgroups:objc_threadgroups
                   threadsPerThreadgroup:objc_threads_per_threadgroup];
}

} // namespace orteaf::internal::backend::mps

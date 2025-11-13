/**
 * @file mps_compute_command_encorder.mm
 * @brief Implementation of MPS/Metal compute command encoder helpers.
 */
#include "orteaf/internal/backend/mps/mps_compute_command_encorder.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
#import <Metal/Metal.h>
    #include "orteaf/internal/diagnostics/error/error.h"
#else
#include "orteaf/internal/diagnostics/error/error_impl.h"
#endif

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::createComputeCommandEncoder
 */
MPSComputeCommandEncoder_t createComputeCommandEncoder(MPSCommandBuffer_t command_buffer) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (command_buffer == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "createComputeCommandEncoder: command_buffer cannot be nullptr");
    }
    id<MTLCommandBuffer> objc_command_buffer = objcFromOpaqueNoown<id<MTLCommandBuffer>>(command_buffer);
    id<MTLComputeCommandEncoder> objc_encoder = [objc_command_buffer computeCommandEncoder];
    return (MPSComputeCommandEncoder_t)opaqueFromObjcRetained(objc_encoder);
#else
    (void)command_buffer;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "createComputeCommandEncoder: MPS backend is not available (MPS disabled)");
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyComputeCommandEncoder
 */
void destroyComputeCommandEncoder(MPSComputeCommandEncoder_t compute_command_encoder) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (compute_command_encoder == nullptr) return;
    opaqueReleaseRetained(compute_command_encoder);
#else
    (void)compute_command_encoder;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "destroyComputeCommandEncoder: MPS backend is not available (MPS disabled)");
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::setPipelineState
 */
void setPipelineState(MPSComputeCommandEncoder_t compute_command_encoder, MPSComputePipelineState_t pipeline_state) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
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
#else
    (void)compute_command_encoder;
    (void)pipeline_state;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setPipelineState: MPS backend is not available (MPS disabled)");
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::endEncoding
 */
void endEncoding(MPSComputeCommandEncoder_t compute_command_encoder) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (compute_command_encoder == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "endEncoding: compute_command_encoder cannot be nullptr");
    }
    id<MTLComputeCommandEncoder> objc_encoder = objcFromOpaqueNoown<id<MTLComputeCommandEncoder>>(compute_command_encoder);
    [objc_encoder endEncoding];
#else
    (void)compute_command_encoder;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "endEncoding: MPS backend is not available (MPS disabled)");
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::setBuffer
 */
void setBuffer(MPSComputeCommandEncoder_t compute_command_encoder, MPSBuffer_t buffer, size_t offset, size_t index) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
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
#else
    (void)compute_command_encoder;
    (void)buffer;
    (void)offset;
    (void)index;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setBuffer: MPS backend is not available (MPS disabled)");
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::setBytes
 */
void setBytes(MPSComputeCommandEncoder_t compute_command_encoder, const void* bytes, size_t length, size_t index) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
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
#else
    (void)compute_command_encoder;
    (void)bytes;
    (void)length;
    (void)index;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setBytes: MPS backend is not available (MPS disabled)");
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::setThreadgroups
 */
void setThreadgroups(MPSComputeCommandEncoder_t compute_command_encoder,
                      MPSSize_t threadgroups,
                      MPSSize_t threads_per_threadgroup) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (compute_command_encoder == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "setThreadgroups: compute_command_encoder cannot be nullptr");
    }
    id<MTLComputeCommandEncoder> objc_encoder = objcFromOpaqueNoown<id<MTLComputeCommandEncoder>>(compute_command_encoder);
    const MTLSize objc_threadgroups = orteaf::internal::backend::mps::toMtlSize(threadgroups);
    const MTLSize objc_threads_per_threadgroup = orteaf::internal::backend::mps::toMtlSize(threads_per_threadgroup);
    [objc_encoder dispatchThreadgroups:objc_threadgroups
                   threadsPerThreadgroup:objc_threads_per_threadgroup];
#else
    (void)compute_command_encoder;
    (void)threadgroups;
    (void)threads_per_threadgroup;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "setThreadgroups: MPS backend is not available (MPS disabled)");
#endif
}

} // namespace orteaf::internal::backend::mps

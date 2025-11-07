/**
 * @file mps_compute_command_encorder.mm
 * @brief Implementation of MPS/Metal compute command encoder helpers.
 */
#include "orteaf/internal/backend/mps/mps_compute_command_encorder.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
#import <Metal/Metal.h>
    #include "orteaf/internal/diagnostics/error/error.h"
#endif

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::create_compute_command_encoder
 */
MPSComputeCommandEncoder_t create_compute_command_encoder(MPSCommandBuffer_t command_buffer) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (command_buffer == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "create_compute_command_encoder: command_buffer cannot be nullptr");
    }
    id<MTLCommandBuffer> objc_command_buffer = objc_from_opaque_noown<id<MTLCommandBuffer>>(command_buffer);
    id<MTLComputeCommandEncoder> objc_encoder = [objc_command_buffer computeCommandEncoder];
    return (MPSComputeCommandEncoder_t)opaque_from_objc_retained(objc_encoder);
#else
    (void)command_buffer;
    return nullptr;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::destroy_compute_command_encoder
 */
void destroy_compute_command_encoder(MPSComputeCommandEncoder_t compute_command_encoder) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (!compute_command_encoder) return;
    opaque_release_retained(compute_command_encoder);
#else
    (void)compute_command_encoder;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::set_pipeline_state
 */
void set_pipeline_state(MPSComputeCommandEncoder_t compute_command_encoder, MPSPipelineState_t pipeline_state) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (compute_command_encoder == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "set_pipeline_state: compute_command_encoder cannot be nullptr");
    }
    if (pipeline_state == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "set_pipeline_state: pipeline_state cannot be nullptr");
    }
    id<MTLComputeCommandEncoder> objc_encoder = objc_from_opaque_noown<id<MTLComputeCommandEncoder>>(compute_command_encoder);
    id<MTLComputePipelineState> objc_pipeline_state = objc_from_opaque_noown<id<MTLComputePipelineState>>(pipeline_state);
    [objc_encoder setComputePipelineState:objc_pipeline_state];
#else
    (void)compute_command_encoder;
    (void)pipeline_state;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::end_encoding
 */
void end_encoding(MPSComputeCommandEncoder_t compute_command_encoder) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (compute_command_encoder == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "end_encoding: compute_command_encoder cannot be nullptr");
    }
    id<MTLComputeCommandEncoder> objc_encoder = objc_from_opaque_noown<id<MTLComputeCommandEncoder>>(compute_command_encoder);
    [objc_encoder endEncoding];
#else
    (void)compute_command_encoder;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::set_buffer
 */
void set_buffer(MPSComputeCommandEncoder_t compute_command_encoder, MPSBuffer_t buffer, size_t offset, size_t index) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (compute_command_encoder == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "set_buffer: compute_command_encoder cannot be nullptr");
    }
    if (buffer == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "set_buffer: buffer cannot be nullptr");
    }
    id<MTLComputeCommandEncoder> objc_encoder = objc_from_opaque_noown<id<MTLComputeCommandEncoder>>(compute_command_encoder);
    id<MTLBuffer> objc_buffer = objc_from_opaque_noown<id<MTLBuffer>>(buffer);
    [objc_encoder setBuffer:objc_buffer offset:offset atIndex:index];
#else
    (void)compute_command_encoder;
    (void)buffer;
    (void)offset;
    (void)index;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::set_bytes
 */
void set_bytes(MPSComputeCommandEncoder_t compute_command_encoder, const void* bytes, size_t length, size_t index) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (compute_command_encoder == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "set_bytes: compute_command_encoder cannot be nullptr");
    }
    if (bytes == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "set_bytes: bytes cannot be nullptr");
    }
    id<MTLComputeCommandEncoder> objc_encoder = objc_from_opaque_noown<id<MTLComputeCommandEncoder>>(compute_command_encoder);
    [objc_encoder setBytes:bytes length:length atIndex:index];
#else
    (void)compute_command_encoder;
    (void)bytes;
    (void)length;
    (void)index;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::set_threadgroups
 */
void set_threadgroups(MPSComputeCommandEncoder_t compute_command_encoder,
                      MPSSize_t threadgroups,
                      MPSSize_t threads_per_threadgroup) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (compute_command_encoder == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "set_threadgroups: compute_command_encoder cannot be nullptr");
    }
    id<MTLComputeCommandEncoder> objc_encoder = objc_from_opaque_noown<id<MTLComputeCommandEncoder>>(compute_command_encoder);
    const MTLSize objc_threadgroups = orteaf::internal::backend::mps::to_mtl_size(threadgroups);
    const MTLSize objc_threads_per_threadgroup = orteaf::internal::backend::mps::to_mtl_size(threads_per_threadgroup);
    [objc_encoder dispatchThreadgroups:objc_threadgroups
                   threadsPerThreadgroup:objc_threads_per_threadgroup];
#else
    (void)compute_command_encoder;
    (void)threadgroups;
    (void)threads_per_threadgroup;
#endif
}

} // namespace orteaf::internal::backend::mps

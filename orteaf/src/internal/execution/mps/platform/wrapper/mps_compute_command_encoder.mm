/**
 * @file mps_compute_command_encoder.mm
 * @brief Implementation of MPS/Metal compute command encoder helpers.
 */
#ifndef __OBJC__
#error                                                                         \
    "mps_compute_command_encoder.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/execution/mps/platform/wrapper/mps_compute_command_encoder.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_objc_bridge.h"

#include "orteaf/internal/diagnostics/error/error.h"
#import <Metal/Metal.h>

namespace orteaf::internal::execution::mps::platform::wrapper {

/**
 * @copydoc orteaf::internal::backend::mps::createComputeCommandEncoder
 */
MpsComputeCommandEncoder_t
createComputeCommandEncoder(MpsCommandBuffer_t command_buffer) {
  if (command_buffer == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer,
               "createComputeCommandEncoder: command_buffer cannot be nullptr");
  }
  id<MTLCommandBuffer> objc_command_buffer =
      objcFromOpaqueNoown<id<MTLCommandBuffer>>(command_buffer);
  id<MTLComputeCommandEncoder> objc_encoder =
      [objc_command_buffer computeCommandEncoder];
  return (MpsComputeCommandEncoder_t)opaqueFromObjcRetained(objc_encoder);
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyComputeCommandEncoder
 */
void destroyComputeCommandEncoder(
    MpsComputeCommandEncoder_t compute_command_encoder) {
  if (compute_command_encoder == nullptr)
    return;
  opaqueReleaseRetained(compute_command_encoder);
}

/**
 * @copydoc orteaf::internal::backend::mps::setPipelineState
 */
void setPipelineState(MpsComputeCommandEncoder_t compute_command_encoder,
                      MpsComputePipelineState_t pipeline_state) {
  if (compute_command_encoder == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer,
               "setPipelineState: compute_command_encoder cannot be nullptr");
  }
  if (pipeline_state == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer,
               "setPipelineState: pipeline_state cannot be nullptr");
  }
  id<MTLComputeCommandEncoder> objc_encoder =
      objcFromOpaqueNoown<id<MTLComputeCommandEncoder>>(
          compute_command_encoder);
  id<MTLComputePipelineState> objc_pipeline_state =
      objcFromOpaqueNoown<id<MTLComputePipelineState>>(pipeline_state);
  [objc_encoder setComputePipelineState:objc_pipeline_state];
}

/**
 * @copydoc orteaf::internal::backend::mps::endEncoding
 */
void endEncoding(MpsComputeCommandEncoder_t compute_command_encoder) {
  if (compute_command_encoder == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer,
               "endEncoding: compute_command_encoder cannot be nullptr");
  }
  id<MTLComputeCommandEncoder> objc_encoder =
      objcFromOpaqueNoown<id<MTLComputeCommandEncoder>>(
          compute_command_encoder);
  [objc_encoder endEncoding];
}

/**
 * @copydoc orteaf::internal::backend::mps::setBuffer
 */
void setBuffer(MpsComputeCommandEncoder_t compute_command_encoder,
               MpsBuffer_t buffer, size_t offset, size_t index) {
  if (compute_command_encoder == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer,
               "setBuffer: compute_command_encoder cannot be nullptr");
  }
  if (buffer == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer, "setBuffer: buffer cannot be nullptr");
  }
  id<MTLComputeCommandEncoder> objc_encoder =
      objcFromOpaqueNoown<id<MTLComputeCommandEncoder>>(
          compute_command_encoder);
  id<MTLBuffer> objc_buffer = objcFromOpaqueNoown<id<MTLBuffer>>(buffer);
  [objc_encoder setBuffer:objc_buffer offset:offset atIndex:index];
}

/**
 * @copydoc orteaf::internal::backend::mps::setBytes
 */
void setBytes(MpsComputeCommandEncoder_t compute_command_encoder,
              const void *bytes, size_t length, size_t index) {
  if (compute_command_encoder == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer,
               "setBytes: compute_command_encoder cannot be nullptr");
  }
  if (bytes == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer, "setBytes: bytes cannot be nullptr");
  }
  id<MTLComputeCommandEncoder> objc_encoder =
      objcFromOpaqueNoown<id<MTLComputeCommandEncoder>>(
          compute_command_encoder);
  [objc_encoder setBytes:bytes length:length atIndex:index];
}

/**
 * @copydoc orteaf::internal::backend::mps::setThreadgroups
 */
void setThreadgroups(MpsComputeCommandEncoder_t compute_command_encoder,
                     MPSSize_t threadgroups,
                     MPSSize_t threads_per_threadgroup) {
  if (compute_command_encoder == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer,
               "setThreadgroups: compute_command_encoder cannot be nullptr");
  }
  id<MTLComputeCommandEncoder> objc_encoder =
      objcFromOpaqueNoown<id<MTLComputeCommandEncoder>>(
          compute_command_encoder);
  const MTLSize objc_threadgroups =
      orteaf::internal::execution::mps::platform::wrapper::toMtlSize(
          threadgroups);
  const MTLSize objc_threads_per_threadgroup =
      orteaf::internal::execution::mps::platform::wrapper::toMtlSize(
          threads_per_threadgroup);
  [objc_encoder dispatchThreadgroups:objc_threadgroups
               threadsPerThreadgroup:objc_threads_per_threadgroup];
}

} // namespace orteaf::internal::execution::mps::platform::wrapper

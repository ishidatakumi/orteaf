#pragma once

#include <orteaf/internal/runtime/mps/platform/wrapper/mps_command_buffer.h>
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_compute_command_encoder.h>
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_fence.h>

namespace orteaf::internal::runtime::mps::platform {

struct MpsFastOps {
  // Fast-path wrapper for command buffer creation.
  static inline ::orteaf::internal::runtime::mps::platform::wrapper::
      MPSCommandBuffer_t
      createCommandBuffer(
          ::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandQueue_t
              command_queue) {
    return ::orteaf::internal::runtime::mps::platform::wrapper::
        createCommandBuffer(command_queue);
  }

  // Fast-path wrapper for compute encoder creation and configuration
  // primitives.
  static inline ::orteaf::internal::runtime::mps::platform::wrapper::
      MPSComputeCommandEncoder_t
      createComputeCommandEncoder(
          ::orteaf::internal::runtime::mps::platform::wrapper::
              MPSCommandBuffer_t command_buffer) {
    return ::orteaf::internal::runtime::mps::platform::wrapper::
        createComputeCommandEncoder(command_buffer);
  }

  static inline void
  setPipelineState(::orteaf::internal::runtime::mps::platform::wrapper::
                       MPSComputeCommandEncoder_t encoder,
                   ::orteaf::internal::runtime::mps::platform::wrapper::
                       MPSComputePipelineState_t pipeline) {
    ::orteaf::internal::runtime::mps::platform::wrapper::setPipelineState(
        encoder, pipeline);
  }

  static inline void setBuffer(
      ::orteaf::internal::runtime::mps::platform::wrapper::
          MPSComputeCommandEncoder_t encoder,
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSBuffer_t buffer,
      std::size_t offset, std::size_t index) {
    ::orteaf::internal::runtime::mps::platform::wrapper::setBuffer(
        encoder, buffer, offset, index);
  }

  static inline void setBytes(::orteaf::internal::runtime::mps::platform::
                                  wrapper::MPSComputeCommandEncoder_t encoder,
                              const void *bytes, std::size_t length,
                              std::size_t index) {
    ::orteaf::internal::runtime::mps::platform::wrapper::setBytes(
        encoder, bytes, length, index);
  }

  static inline void
  setThreadgroups(::orteaf::internal::runtime::mps::platform::wrapper::
                      MPSComputeCommandEncoder_t encoder,
                  ::orteaf::internal::runtime::mps::platform::wrapper::MPSSize_t
                      threadgroups,
                  ::orteaf::internal::runtime::mps::platform::wrapper::MPSSize_t
                      threads_per_threadgroup) {
    ::orteaf::internal::runtime::mps::platform::wrapper::setThreadgroups(
        encoder, threadgroups, threads_per_threadgroup);
  }

  static inline void
  endEncoding(::orteaf::internal::runtime::mps::platform::wrapper::
                  MPSComputeCommandEncoder_t encoder) {
    ::orteaf::internal::runtime::mps::platform::wrapper::endEncoding(encoder);
  }

  static inline void
  commit(::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandBuffer_t
             command_buffer) {
    ::orteaf::internal::runtime::mps::platform::wrapper::commit(command_buffer);
  }

  static inline void updateFence(
      ::orteaf::internal::runtime::mps::platform::wrapper::
          MPSComputeCommandEncoder_t encoder,
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSFence_t fence) {
    ::orteaf::internal::runtime::mps::platform::wrapper::updateFence(encoder,
                                                                     fence);
  }

  static inline void waitForFence(
      ::orteaf::internal::runtime::mps::platform::wrapper::
          MPSComputeCommandEncoder_t encoder,
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSFence_t fence) {
    ::orteaf::internal::runtime::mps::platform::wrapper::waitForFence(encoder,
                                                                      fence);
  }
};

} // namespace orteaf::internal::runtime::mps::platform

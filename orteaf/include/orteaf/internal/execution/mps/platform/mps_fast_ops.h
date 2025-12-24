#pragma once

#include <orteaf/internal/execution/mps/platform/wrapper/mps_command_buffer.h>
#include <orteaf/internal/execution/mps/platform/wrapper/mps_compute_command_encoder.h>
#include <orteaf/internal/execution/mps/platform/wrapper/mps_fence.h>

namespace orteaf::internal::execution::mps::platform {

struct MpsFastOps {
  // Fast-path wrapper for command buffer creation.
  static inline ::orteaf::internal::execution::mps::platform::wrapper::
      MpsCommandBuffer_t
      createCommandBuffer(
          ::orteaf::internal::execution::mps::platform::wrapper::MpsCommandQueue_t
              command_queue) {
    return ::orteaf::internal::execution::mps::platform::wrapper::
        createCommandBuffer(command_queue);
  }

  // Fast-path wrapper for compute encoder creation and configuration
  // primitives.
  static inline ::orteaf::internal::execution::mps::platform::wrapper::
      MpsComputeCommandEncoder_t
      createComputeCommandEncoder(
          ::orteaf::internal::execution::mps::platform::wrapper::
              MpsCommandBuffer_t command_buffer) {
    return ::orteaf::internal::execution::mps::platform::wrapper::
        createComputeCommandEncoder(command_buffer);
  }

  static inline void
  setPipelineState(::orteaf::internal::execution::mps::platform::wrapper::
                       MpsComputeCommandEncoder_t encoder,
                   ::orteaf::internal::execution::mps::platform::wrapper::
                       MpsComputePipelineState_t pipeline) {
    ::orteaf::internal::execution::mps::platform::wrapper::setPipelineState(
        encoder, pipeline);
  }

  static inline void setBuffer(
      ::orteaf::internal::execution::mps::platform::wrapper::
          MpsComputeCommandEncoder_t encoder,
      ::orteaf::internal::execution::mps::platform::wrapper::MpsBuffer_t buffer,
      std::size_t offset, std::size_t index) {
    ::orteaf::internal::execution::mps::platform::wrapper::setBuffer(
        encoder, buffer, offset, index);
  }

  static inline void setBytes(::orteaf::internal::execution::mps::platform::
                                  wrapper::MpsComputeCommandEncoder_t encoder,
                              const void *bytes, std::size_t length,
                              std::size_t index) {
    ::orteaf::internal::execution::mps::platform::wrapper::setBytes(
        encoder, bytes, length, index);
  }

  static inline void
  setThreadgroups(::orteaf::internal::execution::mps::platform::wrapper::
                      MpsComputeCommandEncoder_t encoder,
                  ::orteaf::internal::execution::mps::platform::wrapper::MPSSize_t
                      threadgroups,
                  ::orteaf::internal::execution::mps::platform::wrapper::MPSSize_t
                      threads_per_threadgroup) {
    ::orteaf::internal::execution::mps::platform::wrapper::setThreadgroups(
        encoder, threadgroups, threads_per_threadgroup);
  }

  static inline void
  endEncoding(::orteaf::internal::execution::mps::platform::wrapper::
                  MpsComputeCommandEncoder_t encoder) {
    ::orteaf::internal::execution::mps::platform::wrapper::endEncoding(encoder);
  }

  static inline void
  commit(::orteaf::internal::execution::mps::platform::wrapper::MpsCommandBuffer_t
             command_buffer) {
    ::orteaf::internal::execution::mps::platform::wrapper::commit(command_buffer);
  }

  static inline void updateFence(
      ::orteaf::internal::execution::mps::platform::wrapper::
          MpsComputeCommandEncoder_t encoder,
      ::orteaf::internal::execution::mps::platform::wrapper::MpsFence_t fence) {
    ::orteaf::internal::execution::mps::platform::wrapper::updateFence(encoder,
                                                                     fence);
  }

  static inline void waitForFence(
      ::orteaf::internal::execution::mps::platform::wrapper::
          MpsComputeCommandEncoder_t encoder,
      ::orteaf::internal::execution::mps::platform::wrapper::MpsFence_t fence) {
    ::orteaf::internal::execution::mps::platform::wrapper::waitForFence(encoder,
                                                                      fence);
  }

  static inline bool isCompleted(
      ::orteaf::internal::execution::mps::platform::wrapper::MpsCommandBuffer_t
          command_buffer) {
    return ::orteaf::internal::execution::mps::platform::wrapper::isCompleted(
        command_buffer);
  }
};

} // namespace orteaf::internal::execution::mps::platform

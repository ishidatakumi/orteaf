#pragma once

#if ORTEAF_ENABLE_MPS

#include <orteaf/internal/base/handle.h>
#include <orteaf/internal/execution/mps/platform/wrapper/mps_fence.h>

#include <orteaf/internal/execution/mps/platform/mps_fast_ops.h>
#include <orteaf/internal/execution/mps/platform/wrapper/mps_command_buffer.h>

namespace orteaf::internal::execution::mps::manager {
struct FencePayloadPoolTraits;
class MpsFenceLifetimeManager;
}

namespace orteaf::internal::execution::mps::resource {

class MpsFenceHazard {
public:
  using FenceType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsFence_t;
  using CommandBufferType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsCommandBuffer_t;
  using CommandQueueHandle = ::orteaf::internal::base::CommandQueueHandle;

  MpsFenceHazard() = default;

  FenceType fence() const noexcept { return fence_; }
  CommandBufferType commandBuffer() const noexcept { return command_buffer_; }
  CommandQueueHandle commandQueueHandle() const noexcept {
    return command_queue_handle_;
  }

  bool hasFence() const noexcept { return fence_ != nullptr; }
  bool hasCommandBuffer() const noexcept { return command_buffer_ != nullptr; }

  bool setFence(FenceType fence) noexcept {
    if (fence_ != nullptr || command_buffer_ != nullptr) {
      return false;
    }
    fence_ = fence;
    return true;
  }

  bool setCommandBuffer(CommandBufferType command_buffer) noexcept {
    if (fence_ == nullptr || command_buffer_ != nullptr) {
      return false;
    }
    command_buffer_ = command_buffer;
    return true;
  }

  bool setCommandQueueHandle(CommandQueueHandle handle) noexcept {
    if (command_buffer_ != nullptr) {
      return false;
    }
    command_queue_handle_ = handle;
    return true;
  }

  template <typename MpsOps =
                ::orteaf::internal::execution::mps::platform::MpsFastOps>
  bool isReady() {
    if (command_buffer_ == nullptr) {
      return true;
    }
    if (MpsOps::isCompleted(command_buffer_)) {
      command_buffer_ = nullptr;
      return true;
    }
    return false;
  }

  bool isCompleted() const noexcept { return command_buffer_ == nullptr; }

private:
  void markCompletedUnsafe() noexcept { command_buffer_ = nullptr; }

  void reset() noexcept {
    fence_ = nullptr;
    command_buffer_ = nullptr;
    command_queue_handle_ = {};
  }

  friend struct ::orteaf::internal::execution::mps::manager::
      FencePayloadPoolTraits;
  friend class ::orteaf::internal::execution::mps::manager::
      MpsFenceLifetimeManager;

  FenceType fence_{nullptr};
  CommandBufferType command_buffer_{nullptr};
  CommandQueueHandle command_queue_handle_{};
};

} // namespace orteaf::internal::execution::mps::resource

#endif // ORTEAF_ENABLE_MPS

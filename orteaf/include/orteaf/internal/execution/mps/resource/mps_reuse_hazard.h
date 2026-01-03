#pragma once

#if ORTEAF_ENABLE_MPS

#include <orteaf/internal/base/handle.h>
#include <orteaf/internal/execution/mps/platform/wrapper/mps_command_buffer.h>

#include <orteaf/internal/execution/mps/platform/mps_fast_ops.h>

namespace orteaf::internal::execution::mps::resource {

class MpsReuseHazard {
public:
  using CommandBufferType =
      ::orteaf::internal::execution::mps::platform::wrapper::MpsCommandBuffer_t;
  using CommandQueueHandle = ::orteaf::internal::base::CommandQueueHandle;

  MpsReuseHazard() = default;

  CommandBufferType commandBuffer() const noexcept { return command_buffer_; }
  CommandQueueHandle commandQueueHandle() const noexcept {
    return command_queue_handle_;
  }

  bool hasCommandBuffer() const noexcept { return command_buffer_ != nullptr; }

  template <typename MpsOps =
                ::orteaf::internal::execution::mps::platform::MpsFastOps>
  bool isCompleted() {
    if (command_buffer_ == nullptr) {
      return true;
    }
    if (MpsOps::isCompleted(command_buffer_)) {
      command_buffer_ = nullptr;
      return true;
    }
    return false;
  }

  bool setCommandBuffer(CommandBufferType command_buffer) noexcept {
    if (command_buffer_ != nullptr) {
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

private:
  void reset() noexcept {
    command_buffer_ = nullptr;
    command_queue_handle_ = {};
  }

  CommandBufferType command_buffer_{nullptr};
  CommandQueueHandle command_queue_handle_{};
};

} // namespace orteaf::internal::execution::mps::resource

#endif // ORTEAF_ENABLE_MPS

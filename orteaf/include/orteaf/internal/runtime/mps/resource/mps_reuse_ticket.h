#pragma once

#if ORTEAF_ENABLE_MPS

#include <orteaf/internal/base/handle.h>
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_event.h>
#include <orteaf/internal/runtime/mps/resource/mps_fence_ticket.h>

namespace orteaf::internal::runtime::mps::resource {

class MpsReuseTicket {
public:
  MpsReuseTicket() noexcept = default;
  MpsReuseTicket(
      ::orteaf::internal::base::CommandQueueHandle handle,
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsCommandBuffer_t
          command_buffer) noexcept
      : command_queue_handle_(handle), command_buffer_(command_buffer) {}

  MpsReuseTicket(const MpsFenceTicket &fence_ticket) noexcept
      : command_queue_handle_(fence_ticket.commandQueueHandle()),
        command_buffer_(fence_ticket.commandBuffer()) {}
  MpsReuseTicket(const MpsReuseTicket &) = delete;
  MpsReuseTicket &operator=(const MpsReuseTicket &) = delete;
  MpsReuseTicket(MpsReuseTicket &&other) noexcept { moveFrom(other); }
  MpsReuseTicket &operator=(MpsReuseTicket &&other) noexcept {
    if (this != &other) {
      reset();
      moveFrom(other);
    }
    return *this;
  }
  ~MpsReuseTicket() = default;

  bool valid() const noexcept { return command_buffer_ != nullptr; }

  ::orteaf::internal::base::CommandQueueHandle
  commandQueueHandle() const noexcept {
    return command_queue_handle_;
  }
  ::orteaf::internal::runtime::mps::platform::wrapper::MpsCommandBuffer_t
  commandBuffer() const noexcept {
    return command_buffer_;
  }

  MpsReuseTicket &setCommandQueueHandle(
      ::orteaf::internal::base::CommandQueueHandle handle) noexcept {
    command_queue_handle_ = handle;
    return *this;
  }

  MpsReuseTicket &setCommandBuffer(
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsCommandBuffer_t
          command_buffer) noexcept {
    command_buffer_ = command_buffer;
    return *this;
  }

  void reset() noexcept {
    command_queue_handle_ = {};
    command_buffer_ = nullptr;
  }

private:
  void moveFrom(MpsReuseTicket &other) noexcept {
    command_queue_handle_ = other.command_queue_handle_;
    command_buffer_ = other.command_buffer_;
    other.command_queue_handle_ = {};
    other.command_buffer_ = nullptr;
  }

  ::orteaf::internal::base::CommandQueueHandle command_queue_handle_{};
  ::orteaf::internal::runtime::mps::platform::wrapper::MpsCommandBuffer_t
      command_buffer_{nullptr};
};

} // namespace orteaf::internal::runtime::mps::resource

#endif // ORTEAF_ENABLE_MPS

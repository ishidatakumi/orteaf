#pragma once

#if ORTEAF_ENABLE_MPS

#include <optional>
#include <utility>

#include <orteaf/internal/base/handle.h>
#include <orteaf/internal/runtime/mps/manager/mps_fence_pool.h>
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_event.h>

namespace orteaf::internal::runtime::mps::resource {
class MpsFenceTicket {
public:
  using MpsFenceLease =
      ::orteaf::internal::runtime::mps::manager::MpsFencePool::FenceLease;

  MpsFenceTicket() noexcept = default;
  MpsFenceTicket(
      ::orteaf::internal::base::CommandQueueHandle handle,
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandBuffer_t
          command_buffer,
      MpsFenceLease &&fence_handle) noexcept
      : command_queue_handle_(handle), command_buffer_(command_buffer),
        fence_handle_(std::move(fence_handle)) {}

  MpsFenceTicket(const MpsFenceTicket &) = delete;
  MpsFenceTicket &operator=(const MpsFenceTicket &) = delete;
  MpsFenceTicket(MpsFenceTicket &&other) noexcept { moveFrom(other); }
  MpsFenceTicket &operator=(MpsFenceTicket &&other) noexcept {
    if (this != &other) {
      reset();
      moveFrom(other);
    }
    return *this;
  }
  ~MpsFenceTicket() = default;

  bool valid() const noexcept { return command_buffer_ != nullptr; }

  ::orteaf::internal::base::CommandQueueHandle
  commandQueueHandle() const noexcept {
    return command_queue_handle_;
  }
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandBuffer_t
  commandBuffer() const noexcept {
    return command_buffer_;
  }
  bool hasFence() const noexcept { return fence_handle_.has_value(); }
  const MpsFenceLease &fenceHandle() const noexcept {
    return fence_handle_.value();
  }

  MpsFenceTicket &setCommandQueueHandle(
      ::orteaf::internal::base::CommandQueueHandle handle) noexcept {
    command_queue_handle_ = handle;
    return *this;
  }

  MpsFenceTicket &setCommandBuffer(
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandBuffer_t
          command_buffer) noexcept {
    command_buffer_ = command_buffer;
    return *this;
  }

  MpsFenceTicket &setFenceHandle(MpsFenceLease &&fence_handle) noexcept {
    fence_handle_.emplace(std::move(fence_handle));
    return *this;
  }

  void reset() noexcept {
    if (fence_handle_.has_value()) {
      fence_handle_->release();
      fence_handle_.reset();
    }
    command_queue_handle_ = {};
    command_buffer_ = nullptr;
  }

private:
  void moveFrom(MpsFenceTicket &other) noexcept {
    command_queue_handle_ = other.command_queue_handle_;
    command_buffer_ = other.command_buffer_;
    fence_handle_ = std::move(other.fence_handle_);
    other.command_queue_handle_ = {};
    other.command_buffer_ = nullptr;
    other.fence_handle_.reset();
  }

  ::orteaf::internal::base::CommandQueueHandle command_queue_handle_{};
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandBuffer_t
      command_buffer_{nullptr};
  std::optional<MpsFenceLease> fence_handle_{};
};

} // namespace orteaf::internal::runtime::mps::resource

#endif // ORTEAF_ENABLE_MPS

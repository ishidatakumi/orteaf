#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <utility>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/execution/mps/manager/mps_fence_manager.h"
#include "orteaf/internal/execution/mps/platform/mps_fast_ops.h"

namespace orteaf::internal::execution::mps::manager {

class MpsFenceLifetimeManager {
public:
  using FenceManager = ::orteaf::internal::execution::mps::manager::MpsFenceManager;
  using FenceLease = FenceManager::FenceLease;
  using CommandQueueHandle = ::orteaf::internal::base::CommandQueueHandle;
  using CommandBufferType =
      ::orteaf::internal::execution::mps::platform::wrapper::
          MpsCommandBuffer_t;

  MpsFenceLifetimeManager() = default;
  MpsFenceLifetimeManager(const MpsFenceLifetimeManager &) = delete;
  MpsFenceLifetimeManager &operator=(const MpsFenceLifetimeManager &) = delete;
  MpsFenceLifetimeManager(MpsFenceLifetimeManager &&) noexcept = default;
  MpsFenceLifetimeManager &
  operator=(MpsFenceLifetimeManager &&) noexcept = default;
  ~MpsFenceLifetimeManager() = default;

  bool setFenceManager(FenceManager *manager) noexcept {
    if (!empty() && manager != fence_manager_) {
      return false;
    }
    fence_manager_ = manager;
    return true;
  }

  bool setCommandQueueHandle(CommandQueueHandle handle) noexcept {
    if (!empty() && handle != queue_handle_) {
      return false;
    }
    queue_handle_ = handle;
    return true;
  }

  FenceLease acquire(CommandBufferType command_buffer) {
    if (fence_manager_ == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS fence lifetime manager requires a fence manager");
    }
    if (!queue_handle_.isValid()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "MPS fence lifetime manager requires a valid command queue handle");
    }
    if (command_buffer == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "MPS fence lifetime manager requires a command buffer");
    }
    auto lease = fence_manager_->acquire();
    auto *payload = lease.payloadPtr();
    if (payload == nullptr) {
      lease.release();
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS fence lease has no payload");
    }
    if (!payload->setCommandQueueHandle(queue_handle_)) {
      lease.release();
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS fence hazard failed to bind command queue handle");
    }
    if (!payload->setCommandBuffer(command_buffer)) {
      lease.release();
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS fence hazard failed to bind command buffer");
    }
    hazards_.pushBack(lease);
    return lease;
  }

  template <typename FastOps =
                ::orteaf::internal::execution::mps::platform::MpsFastOps>
  std::size_t releaseReady() {
    if (fence_manager_ == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS fence lifetime manager requires a fence manager");
    }
    if (!queue_handle_.isValid()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "MPS fence lifetime manager requires a valid command queue handle");
    }
    if (head_ >= hazards_.size()) {
      hazards_.clear();
      head_ = 0;
      return 0;
    }

    std::size_t ready_end = 0;
    for (std::size_t i = hazards_.size(); i > head_; --i) {
      auto &lease = hazards_[i - 1];
      auto *payload = lease.payloadPtr();
      if (payload == nullptr) {
        ready_end = i;
        break;
      }
      if (payload->isReady<FastOps>()) {
        ready_end = i;
        break;
      }
    }

    if (ready_end == 0) {
      return 0;
    }

    const std::size_t released = ready_end - head_;
    for (std::size_t i = head_; i < ready_end; ++i) {
      hazards_[i].release();
    }
    head_ = ready_end;
    compactIfNeeded();
    return released;
  }

  void clear() noexcept {
    for (std::size_t i = head_; i < hazards_.size(); ++i) {
      hazards_[i].release();
    }
    hazards_.clear();
    head_ = 0;
  }

  std::size_t size() const noexcept {
    return (head_ >= hazards_.size()) ? 0 : (hazards_.size() - head_);
  }

  bool empty() const noexcept { return size() == 0; }

#if ORTEAF_ENABLE_TEST
  std::size_t storageSizeForTest() const noexcept { return hazards_.size(); }
  std::size_t headIndexForTest() const noexcept { return head_; }
#endif

private:
  void compactIfNeeded() {
    if (head_ == 0) {
      return;
    }
    if (head_ >= hazards_.size()) {
      hazards_.clear();
      head_ = 0;
      return;
    }
    if (head_ < (hazards_.size() / 2)) {
      return;
    }

    const std::size_t new_size = hazards_.size() - head_;
    for (std::size_t i = 0; i < new_size; ++i) {
      hazards_[i] = std::move(hazards_[head_ + i]);
    }
    hazards_.resize(new_size);
    head_ = 0;
  }

  FenceManager *fence_manager_{nullptr};
  CommandQueueHandle queue_handle_{CommandQueueHandle::invalid()};
  ::orteaf::internal::base::HeapVector<FenceLease> hazards_{};
  std::size_t head_{0};
};

} // namespace orteaf::internal::execution::mps::manager

#endif // ORTEAF_ENABLE_MPS

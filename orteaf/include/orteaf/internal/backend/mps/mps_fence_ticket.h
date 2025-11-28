#pragma once

#if ORTEAF_ENABLE_MPS

#include <optional>
#include <utility>

#include "orteaf/internal/backend/mps/wrapper/mps_event.h"
#include "orteaf/internal/base/strong_id.h"
#include "orteaf/internal/runtime/manager/mps/mps_fence_pool.h"

namespace orteaf::internal::backend::mps {

class MpsFenceTicket {
public:
    using MpsFenceHandle = ::orteaf::internal::runtime::mps::MpsFencePool::Handle;

    MpsFenceTicket() noexcept = default;
    MpsFenceTicket(base::CommandQueueId id,
                   ::orteaf::internal::backend::mps::MPSCommandBuffer_t command_buffer,
                   MpsFenceHandle&& fence_handle) noexcept
        : command_queue_id_(id),
          command_buffer_(command_buffer),
          fence_handle_(std::move(fence_handle)) {}

    MpsFenceTicket(const MpsFenceTicket&) = delete;
    MpsFenceTicket& operator=(const MpsFenceTicket&) = delete;
    MpsFenceTicket(MpsFenceTicket&& other) noexcept { moveFrom(other); }
    MpsFenceTicket& operator=(MpsFenceTicket&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(other);
        }
        return *this;
    }
    ~MpsFenceTicket() = default;

    bool valid() const noexcept { return command_buffer_ != nullptr; }

    base::CommandQueueId commandQueueId() const noexcept { return command_queue_id_; }
    ::orteaf::internal::backend::mps::MPSCommandBuffer_t commandBuffer() const noexcept {
        return command_buffer_;
    }
    bool hasFence() const noexcept { return fence_handle_.has_value(); }
    const MpsFenceHandle& fenceHandle() const noexcept { return fence_handle_.value(); }

    MpsFenceTicket& setCommandQueueId(base::CommandQueueId id) noexcept {
        command_queue_id_ = id;
        return *this;
    }

    MpsFenceTicket& setCommandBuffer(
        ::orteaf::internal::backend::mps::MPSCommandBuffer_t command_buffer) noexcept {
        command_buffer_ = command_buffer;
        return *this;
    }

    MpsFenceTicket& setFenceHandle(MpsFenceHandle&& fence_handle) noexcept {
        fence_handle_.emplace(std::move(fence_handle));
        return *this;
    }

    void reset() noexcept {
        if (fence_handle_.has_value()) {
            fence_handle_->release();
            fence_handle_.reset();
        }
        command_queue_id_ = {};
        command_buffer_ = nullptr;
    }

private:
    void moveFrom(MpsFenceTicket& other) noexcept {
        command_queue_id_ = other.command_queue_id_;
        command_buffer_ = other.command_buffer_;
        fence_handle_ = std::move(other.fence_handle_);
        other.command_queue_id_ = {};
        other.command_buffer_ = nullptr;
        other.fence_handle_.reset();
    }

    base::CommandQueueId command_queue_id_{};
    ::orteaf::internal::backend::mps::MPSCommandBuffer_t command_buffer_{nullptr};
    std::optional<MpsFenceHandle> fence_handle_{};
};

} // namespace orteaf::internal::backend::mps

#endif // ORTEAF_ENABLE_MPS

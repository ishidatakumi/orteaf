#pragma once

#if ORTEAF_ENABLE_MPS

#include <utility>

#include "orteaf/internal/backend/mps/wrapper/mps_event.h"
#include "orteaf/internal/base/strong_id.h"

namespace orteaf::internal::backend::mps {

class MpsReuseTicket {
public:
    MpsReuseTicket() noexcept = default;
    MpsReuseTicket(base::CommandQueueId id,
                   ::orteaf::internal::backend::mps::MPSCommandBuffer_t command_buffer) noexcept
        : command_queue_id_(id), command_buffer_(command_buffer) {}

    MpsReuseTicket(const MpsReuseTicket&) = delete;
    MpsReuseTicket& operator=(const MpsReuseTicket&) = delete;
    MpsReuseTicket(MpsReuseTicket&& other) noexcept { moveFrom(other); }
    MpsReuseTicket& operator=(MpsReuseTicket&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(other);
        }
        return *this;
    }
    ~MpsReuseTicket() = default;

    bool valid() const noexcept { return command_buffer_ != nullptr; }

    base::CommandQueueId commandQueueId() const noexcept { return command_queue_id_; }
    ::orteaf::internal::backend::mps::MPSCommandBuffer_t commandBuffer() const noexcept {
        return command_buffer_;
    }

    MpsReuseTicket& setCommandQueueId(base::CommandQueueId id) noexcept {
        command_queue_id_ = id;
        return *this;
    }

    MpsReuseTicket& setCommandBuffer(
        ::orteaf::internal::backend::mps::MPSCommandBuffer_t command_buffer) noexcept {
        command_buffer_ = command_buffer;
        return *this;
    }

    void reset() noexcept {
        command_queue_id_ = {};
        command_buffer_ = nullptr;
    }

private:
    void moveFrom(MpsReuseTicket& other) noexcept {
        command_queue_id_ = other.command_queue_id_;
        command_buffer_ = other.command_buffer_;
        other.command_queue_id_ = {};
        other.command_buffer_ = nullptr;
    }

    base::CommandQueueId command_queue_id_{};
    ::orteaf::internal::backend::mps::MPSCommandBuffer_t command_buffer_{nullptr};
};

} // namespace orteaf::internal::backend::mps

#endif // ORTEAF_ENABLE_MPS

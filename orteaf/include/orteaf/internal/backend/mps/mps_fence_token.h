#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <utility>

#include "orteaf/internal/backend/mps/mps_fence_ticket.h"
#include "orteaf/internal/base/small_vector.h"

namespace orteaf::internal::backend::mps {

class MpsFenceToken {
public:
    using Ticket = MpsFenceTicket;
    static constexpr std::size_t kInlineCapacity = 4;

    MpsFenceToken() = default;
    MpsFenceToken(const MpsFenceToken&) = delete;
    MpsFenceToken& operator=(const MpsFenceToken&) = delete;
    MpsFenceToken(MpsFenceToken&&) noexcept = default;
    MpsFenceToken& operator=(MpsFenceToken&&) noexcept = default;
    ~MpsFenceToken() = default;

    bool empty() const noexcept { return tickets_.empty(); }
    std::size_t size() const noexcept { return tickets_.size(); }

    void addTicket(Ticket&& ticket) { tickets_.pushBack(std::move(ticket)); }

    void clear() noexcept { tickets_.clear(); }

    const Ticket& operator[](std::size_t index) const noexcept { return tickets_[index]; }
    Ticket& operator[](std::size_t index) noexcept { return tickets_[index]; }

    const Ticket* begin() const noexcept { return tickets_.begin(); }
    const Ticket* end() const noexcept { return tickets_.end(); }
    Ticket* begin() noexcept { return tickets_.begin(); }
    Ticket* end() noexcept { return tickets_.end(); }

private:
    ::orteaf::internal::base::SmallVector<Ticket, kInlineCapacity> tickets_{};
};

} // namespace orteaf::internal::backend::mps

#endif // ORTEAF_ENABLE_MPS

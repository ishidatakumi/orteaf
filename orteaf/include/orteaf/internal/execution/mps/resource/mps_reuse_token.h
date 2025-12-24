#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <utility>

#include <orteaf/internal/base/small_vector.h>
#include <orteaf/internal/execution/mps/resource/mps_fence_token.h>
#include <orteaf/internal/execution/mps/resource/mps_reuse_ticket.h>

namespace orteaf::internal::execution::mps::resource {

class MpsReuseToken {
public:
  using Ticket = MpsReuseTicket;
  static constexpr std::size_t kInlineCapacity = 4;

  static MpsReuseToken fromFenceToken(MpsFenceToken &&token) {
    MpsReuseToken reuse_token;
    for (const auto &ticket : token) {
      reuse_token.addTicket(Ticket(ticket));
    }
    token.clear();
    return reuse_token;
  }

  MpsReuseToken() = default;

  explicit MpsReuseToken(MpsFenceToken &&token)
      : MpsReuseToken(fromFenceToken(std::move(token))) {}

  MpsReuseToken(const MpsReuseToken &) = delete;
  MpsReuseToken &operator=(const MpsReuseToken &) = delete;
  MpsReuseToken(MpsReuseToken &&) noexcept = default;
  MpsReuseToken &operator=(MpsReuseToken &&) noexcept = default;
  ~MpsReuseToken() = default;

  bool empty() const noexcept { return tickets_.empty(); }
  std::size_t size() const noexcept { return tickets_.size(); }

  void addTicket(Ticket &&ticket) { tickets_.pushBack(std::move(ticket)); }

  void clear() noexcept { tickets_.clear(); }

  const Ticket &operator[](std::size_t index) const noexcept {
    return tickets_[index];
  }
  Ticket &operator[](std::size_t index) noexcept { return tickets_[index]; }

  const Ticket *begin() const noexcept { return tickets_.begin(); }
  const Ticket *end() const noexcept { return tickets_.end(); }
  Ticket *begin() noexcept { return tickets_.begin(); }
  Ticket *end() noexcept { return tickets_.end(); }

private:
  ::orteaf::internal::base::SmallVector<Ticket, kInlineCapacity> tickets_{};
};

} // namespace orteaf::internal::execution::mps::resource

#endif // ORTEAF_ENABLE_MPS

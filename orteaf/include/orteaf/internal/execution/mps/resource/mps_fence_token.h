#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <utility>

#include <orteaf/internal/base/handle.h>
#include <orteaf/internal/base/small_vector.h>
#include <orteaf/internal/execution/mps/manager/mps_fence_manager.h>

namespace orteaf::internal::execution::mps::resource {

class MpsFenceToken {
public:
  using FenceLease =
      ::orteaf::internal::execution::mps::manager::MpsFenceManager::FenceLease;

  static constexpr std::size_t kInlineCapacity = 4;

  MpsFenceToken() = default;
  MpsFenceToken(const MpsFenceToken &) = delete;
  MpsFenceToken &operator=(const MpsFenceToken &) = delete;
  MpsFenceToken(MpsFenceToken &&) noexcept = default;
  MpsFenceToken &operator=(MpsFenceToken &&) noexcept = default;
  ~MpsFenceToken() = default;

  bool empty() const noexcept { return leases_.empty(); }
  std::size_t size() const noexcept { return leases_.size(); }

  void addLease(FenceLease &&lease) { leases_.pushBack(std::move(lease)); }

  // Add a lease, replacing any existing lease with the same command queue id.
  FenceLease &addOrReplaceLease(FenceLease &&lease) {
    auto *payload = lease.payloadPtr();
    if (payload != nullptr) {
      const auto queue_handle = payload->commandQueueHandle();
      for (std::size_t i = 0; i < leases_.size(); ++i) {
        auto *existing_payload = leases_[i].payloadPtr();
        if (existing_payload != nullptr &&
            existing_payload->commandQueueHandle() == queue_handle) {
          leases_[i] = std::move(lease);
          return leases_[i];
        }
      }
    }
    leases_.pushBack(std::move(lease));
    return leases_.back();
  }

  void clear() noexcept { leases_.clear(); }

  const FenceLease &operator[](std::size_t index) const noexcept {
    return leases_[index];
  }
  FenceLease &operator[](std::size_t index) noexcept { return leases_[index]; }

  const FenceLease *begin() const noexcept { return leases_.begin(); }
  const FenceLease *end() const noexcept { return leases_.end(); }
  FenceLease *begin() noexcept { return leases_.begin(); }
  FenceLease *end() noexcept { return leases_.end(); }

private:
  ::orteaf::internal::base::SmallVector<FenceLease, kInlineCapacity> leases_{};
};

} // namespace orteaf::internal::execution::mps::resource

#endif // ORTEAF_ENABLE_MPS

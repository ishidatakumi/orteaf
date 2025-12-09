#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/shared_lease.h"
#include "orteaf/internal/runtime/base/base_manager.h"
#include "orteaf/internal/runtime/base/resource_pool.h"
#include "orteaf/internal/runtime/mps/platform/mps_slow_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_event.h"

#include <atomic>

namespace orteaf::internal::runtime::mps::manager {

struct EventPoolState {
  std::atomic<std::size_t> ref_count{0};
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSEvent_t event{
      nullptr};
  uint32_t generation{0};
  bool alive{false};
  bool in_use{false};

  EventPoolState() = default;
  EventPoolState(const EventPoolState &) = delete;
  EventPoolState &operator=(const EventPoolState &) = delete;
  EventPoolState(EventPoolState &&other) noexcept
      : ref_count(other.ref_count.load(std::memory_order_relaxed)),
        event(other.event), generation(other.generation), alive(other.alive),
        in_use(other.in_use) {
    other.event = nullptr;
    other.alive = false;
    other.in_use = false;
  }
  EventPoolState &operator=(EventPoolState &&other) noexcept {
    if (this != &other) {
      ref_count.store(other.ref_count.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
      event = other.event;
      generation = other.generation;
      alive = other.alive;
      in_use = other.in_use;
      other.event = nullptr;
      other.alive = false;
      other.in_use = false;
    }
    return *this;
  }
};

struct EventPoolTraits {
  using StateType = EventPoolState;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t;
  using OpsType = ::orteaf::internal::runtime::mps::platform::MpsSlowOps;

  static constexpr const char *Name = "MPS event pool";
};

class MpsEventManager
    : public ::orteaf::internal::runtime::base::BaseManager<MpsEventManager,
                                                            EventPoolTraits> {
public:
  using Base = ::orteaf::internal::runtime::base::BaseManager<MpsEventManager,
                                                              EventPoolTraits>;
  using SlowOps = Base::Ops;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t;
  using EventHandle = ::orteaf::internal::base::EventHandle;
  using EventLease = ::orteaf::internal::base::SharedLease<
      EventHandle,
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSEvent_t,
      MpsEventManager>;

  MpsEventManager() = default;
  MpsEventManager(const MpsEventManager &) = delete;
  MpsEventManager &operator=(const MpsEventManager &) = delete;
  MpsEventManager(MpsEventManager &&) = default;
  MpsEventManager &operator=(MpsEventManager &&) = default;
  ~MpsEventManager() = default;

  void initialize(DeviceType device, SlowOps *slow_ops, std::size_t capacity);

  void shutdown();

  EventLease acquire();
  EventLease acquire(EventHandle handle);

  void release(EventLease &lease) noexcept {
    release(lease.handle());
    lease.invalidate();
  }

#if ORTEAF_ENABLE_TEST
  struct DebugState {
    bool alive{false};
    std::uint32_t generation{0};
    std::size_t growth_chunk_size{0};
  };

  DebugState debugState(::orteaf::internal::base::EventHandle handle) const;
#endif

private:
  void release(EventHandle &handle);
};

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

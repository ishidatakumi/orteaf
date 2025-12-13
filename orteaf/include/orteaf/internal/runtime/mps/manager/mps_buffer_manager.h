#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <string>
#include <utility>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/shared_lease.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/allocator/buffer.h"
#include "orteaf/internal/runtime/allocator/policies/chunk_locator/direct_chunk_locator.h"
#include "orteaf/internal/runtime/allocator/policies/fast_free/fast_free_policies.h"
#include "orteaf/internal/runtime/allocator/policies/freelist/host_stack_freelist_policy.h"
#include "orteaf/internal/runtime/allocator/policies/large_alloc/direct_resource_large_alloc.h"
#include "orteaf/internal/runtime/allocator/policies/reuse/deferred_reuse_policy.h"
#include "orteaf/internal/runtime/allocator/policies/threading/threading_policies.h"
#include "orteaf/internal/runtime/allocator/pool/segregate_pool.h"
#include "orteaf/internal/runtime/base/shared_onetime_manager.h"
#include "orteaf/internal/runtime/mps/manager/mps_library_manager.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/runtime/mps/resource/mps_buffer_view.h"

namespace orteaf::internal::runtime::mps::manager {

using ::orteaf::internal::backend::Backend;

// ============================================================================
// Pool type alias template
// ============================================================================
template <typename ResourceT>
using MpsBufferPoolT =
    ::orteaf::internal::runtime::allocator::pool::SegregatePool<
        ResourceT,
        ::orteaf::internal::runtime::allocator::policies::FastFreePolicy,
        ::orteaf::internal::runtime::allocator::policies::NoLockThreadingPolicy,
        ::orteaf::internal::runtime::allocator::policies::
            DirectResourceLargeAllocPolicy<ResourceT>,
        ::orteaf::internal::runtime::allocator::policies::
            DirectChunkLocatorPolicy<ResourceT>,
        ::orteaf::internal::runtime::allocator::policies::DeferredReusePolicy<
            ResourceT>,
        ::orteaf::internal::runtime::allocator::policies::
            HostStackFreelistPolicy<ResourceT>>;

// ============================================================================
// Resource wrapper for SharedOneTimeState
// ============================================================================
struct MpsBufferResource {
  ::orteaf::internal::runtime::allocator::Buffer buffer{};
};

// ============================================================================
// Traits for SharedOneTimeManager
// ============================================================================
template <typename ResourceT> struct MpsBufferManagerTraitsT {
  using OpsType = MpsBufferPoolT<ResourceT>;
  using StateType =
      ::orteaf::internal::runtime::base::SharedOneTimeState<MpsBufferResource>;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t;
  using HandleType = ::orteaf::internal::base::BufferHandle;

  static constexpr const char *Name = "MPS buffer manager";
};

// Forward declaration
template <typename ResourceT> class MpsBufferManagerT;

// ============================================================================
// MpsBufferManagerT - Templated buffer manager using SharedOneTimeManager
// ============================================================================
template <typename ResourceT>
class MpsBufferManagerT
    : public ::orteaf::internal::runtime::base::SharedOneTimeManager<
          MpsBufferManagerT<ResourceT>, MpsBufferManagerTraitsT<ResourceT>> {
public:
  using Traits = MpsBufferManagerTraitsT<ResourceT>;
  using Base = ::orteaf::internal::runtime::base::SharedOneTimeManager<
      MpsBufferManagerT<ResourceT>, Traits>;
  using Buffer = ::orteaf::internal::runtime::allocator::Buffer;
  using BufferHandle = typename Traits::HandleType;
  using BufferLease =
      ::orteaf::internal::base::SharedLease<BufferHandle, Buffer *,
                                            MpsBufferManagerT>;
  using DeviceType = typename Traits::DeviceType;
  using Pool = MpsBufferPoolT<ResourceT>;
  using Resource = ResourceT;
  using State = typename Traits::StateType;

  // =========================================================================
  // User-tunable configuration (Pool/Resource options with defaults)
  // =========================================================================
  struct Config {
    std::size_t chunk_size{16 * 1024 * 1024};
    std::size_t min_block_size{64};
    std::size_t max_block_size{16 * 1024 * 1024};
    ::orteaf::internal::runtime::mps::platform::wrapper::MPSBufferUsage_t usage{
        ::orteaf::internal::runtime::mps::platform::wrapper::
            kMPSDefaultBufferUsage};
  };

  MpsBufferManagerT() = default;
  MpsBufferManagerT(const MpsBufferManagerT &) = delete;
  MpsBufferManagerT &operator=(const MpsBufferManagerT &) = delete;

  MpsBufferManagerT(MpsBufferManagerT &&other) noexcept
      : Base(std::move(other)), pool_(std::move(other.pool_)),
        device_(other.device_), device_handle_(other.device_handle_),
        heap_(other.heap_) {
    other.device_ = nullptr;
    other.heap_ = nullptr;
  }

  MpsBufferManagerT &operator=(MpsBufferManagerT &&other) noexcept {
    if (this != &other) {
      Base::operator=(std::move(other));
      pool_ = std::move(other.pool_);
      device_ = other.device_;
      device_handle_ = other.device_handle_;
      heap_ = other.heap_;
      other.device_ = nullptr;
      other.heap_ = nullptr;
    }
    return *this;
  }

  ~MpsBufferManagerT() = default;

  // =========================================================================
  // Lifecycle - Dependencies as args, config as param
  // HeapType is deduced from Resource::Config::heap type
  // =========================================================================
  using HeapType = decltype(std::declval<typename Resource::Config>().heap);

  void initialize(DeviceType device,
                  ::orteaf::internal::base::DeviceHandle device_handle,
                  HeapType heap, MpsLibraryManager *library_manager,
                  const Config &config, std::size_t capacity) {
    shutdown();

    if (device == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(Traits::Name) + " requires a valid device");
    }
    if (heap == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(Traits::Name) + " requires a valid heap");
    }

    device_ = device;
    device_handle_ = device_handle;
    heap_ = heap;

    // Build ResourceConfig from dependencies
    typename Resource::Config res_cfg{};
    res_cfg.device = device;
    res_cfg.device_handle = device_handle;
    res_cfg.heap = heap;
    res_cfg.usage = config.usage;
    res_cfg.library_manager = library_manager;

    // Initialize pool with resource
    Resource backend_resource{};
    backend_resource.initialize(res_cfg);
    pool_.~Pool();
    new (&pool_) Pool(std::move(backend_resource));

    // Build PoolConfig from user config
    typename Pool::Config pool_cfg{};
    pool_cfg.chunk_size = config.chunk_size;
    pool_cfg.min_block_size = config.min_block_size;
    pool_cfg.max_block_size = config.max_block_size;
    pool_cfg.fast_free.resource = pool_.resource();
    pool_cfg.threading.resource = pool_.resource();
    pool_cfg.large_alloc.resource = pool_.resource();
    pool_cfg.chunk_locator.resource = pool_.resource();
    pool_cfg.reuse.resource = pool_.resource();
    pool_cfg.freelist.resource = pool_.resource();
    pool_.initialize(pool_cfg);

    Base::ops_ = &pool_;
    Base::states_.clear();
    Base::free_list_.clear();
    if (capacity > 0) {
      Base::growPool(capacity);
    }
    Base::initialized_ = true;
  }

  void shutdown() {
    if (!Base::initialized_) {
      return;
    }
    for (std::size_t i = 0; i < Base::states_.size(); ++i) {
      State &state = Base::states_[i];
      if (state.alive) {
        deallocateBuffer(state.resource.buffer);
        state.resource.buffer = Buffer{};
        state.alive = false;
        state.ref_count.store(0, std::memory_order_relaxed);
      }
    }
    Base::states_.clear();
    Base::free_list_.clear();

    pool_.~Pool();
    new (&pool_) Pool{};

    device_ = nullptr;
    heap_ = nullptr;
    Base::ops_ = nullptr;
    Base::initialized_ = false;
  }

  // =========================================================================
  // Acquire (allocate new buffer)
  // =========================================================================
  BufferLease acquire(std::size_t size, std::size_t alignment) {
    Base::ensureInitialized();
    if (size == 0) {
      return {};
    }

    const std::size_t index = Base::allocateSlot();
    State &state = Base::states_[index];

    state.resource.buffer = allocateBuffer(size, alignment);
    if (!state.resource.buffer.valid()) {
      Base::free_list_.pushBack(index);
      return {};
    }

    Base::markSlotAlive(index);

    const auto handle = BufferHandle{
        static_cast<typename BufferHandle::index_type>(index),
        static_cast<typename BufferHandle::generation_type>(state.generation)};

    return BufferLease{this, handle, &state.resource.buffer};
  }

  // =========================================================================
  // Acquire (share existing buffer by handle)
  // =========================================================================
  BufferLease acquire(BufferHandle handle) {
    Base::ensureInitialized();
    const std::size_t index = static_cast<std::size_t>(handle.index);

    if (index >= Base::states_.size()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(Traits::Name) + " handle out of range");
    }

    State &state = Base::states_[index];
    if (!state.alive) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(Traits::Name) + " handle is inactive");
    }

    if (!Base::isGenerationValid(
            index, static_cast<std::uint32_t>(handle.generation))) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(Traits::Name) + " handle is stale");
    }

    Base::incrementRefCount(index);
    return BufferLease{this, handle, &state.resource.buffer};
  }

  // =========================================================================
  // Release
  // =========================================================================
  void release(BufferHandle handle) {
    if (!Base::initialized_) {
      return;
    }
    const std::size_t index = static_cast<std::size_t>(handle.index);
    if (index >= Base::states_.size()) {
      return;
    }

    State &state = Base::states_[index];
    if (!state.alive) {
      return;
    }
    if (!Base::isGenerationValid(
            index, static_cast<std::uint32_t>(handle.generation))) {
      return;
    }

    const auto prev = state.ref_count.fetch_sub(1, std::memory_order_acq_rel);
    if (prev == 1) {
      deallocateBuffer(state.resource.buffer);
      Base::releaseSlotAndDestroy(index);
    }
  }

  void release(BufferLease &lease) noexcept {
    release(lease.handle());
    lease.invalidate();
  }

  Pool *pool() { return &pool_; }
  const Pool *pool() const { return &pool_; }

private:
  Buffer allocateBuffer(std::size_t size, std::size_t alignment) {
    if (size == 0) {
      return {};
    }
    typename Pool::LaunchParams params{};
    auto res = pool_.allocate(size, alignment, params);
    if (!res.valid()) {
      return {};
    }
    return Buffer{std::move(res), size, alignment};
  }

  void deallocateBuffer(Buffer &buffer) {
    if (!buffer.valid()) {
      return;
    }
    auto &res = buffer.asResource<Backend::Mps>();
    if (!res.valid()) {
      return;
    }
    typename Pool::LaunchParams params{};
    pool_.deallocate(std::move(res), buffer.size(), buffer.alignment(), params);
  }

  // Runtime state
  Pool pool_{};
  DeviceType device_{nullptr};
  ::orteaf::internal::base::DeviceHandle device_handle_{};
  HeapType heap_{nullptr};
};

} // namespace orteaf::internal::runtime::mps::manager

// ============================================================================
// Default type alias (after namespace to avoid circular dependency)
// Include mps_resource.h only when you need MpsBufferManager alias
// ============================================================================
#include "orteaf/internal/runtime/allocator/resource/mps/mps_resource.h"

namespace orteaf::internal::runtime::mps::manager {
using MpsResource =
    ::orteaf::internal::runtime::allocator::resource::mps::MpsResource;
using MpsBufferPool = MpsBufferPoolT<MpsResource>;
using MpsBufferManagerTraits = MpsBufferManagerTraitsT<MpsResource>;
using MpsBufferManager = MpsBufferManagerT<MpsResource>;
} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

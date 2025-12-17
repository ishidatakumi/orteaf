#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <string>
#include <utility>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/allocator/buffer.h"
#include "orteaf/internal/runtime/allocator/policies/chunk_locator/direct_chunk_locator.h"
#include "orteaf/internal/runtime/allocator/policies/fast_free/fast_free_policies.h"
#include "orteaf/internal/runtime/allocator/policies/freelist/host_stack_freelist_policy.h"
#include "orteaf/internal/runtime/allocator/policies/large_alloc/direct_resource_large_alloc.h"
#include "orteaf/internal/runtime/allocator/policies/reuse/deferred_reuse_policy.h"
#include "orteaf/internal/runtime/allocator/policies/threading/threading_policies.h"
#include "orteaf/internal/runtime/allocator/pool/segregate_pool.h"
#include "orteaf/internal/runtime/base/lease/control_block/shared.h"
#include "orteaf/internal/runtime/base/lease/shared_lease.h"
#include "orteaf/internal/runtime/base/lease/slot.h"
#include "orteaf/internal/runtime/base/manager/base_manager_core.h"
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
// Resource wrapper for BaseManagerCore slot
// ============================================================================
struct MpsBufferResource {
  ::orteaf::internal::runtime::allocator::Buffer buffer{};
};

// ============================================================================
// BaseManagerCore Types
// ============================================================================
using BufferSlot =
    ::orteaf::internal::runtime::base::GenerationalSlot<MpsBufferResource>;
using BufferControlBlock =
    ::orteaf::internal::runtime::base::SharedControlBlock<BufferSlot>;

// ============================================================================
// Traits for BaseManagerCore - templated on ResourceT for pool type
// ============================================================================
template <typename ResourceT> struct MpsBufferManagerTraitsT {
  using ControlBlock = BufferControlBlock;
  using Handle = ::orteaf::internal::base::BufferHandle;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t;
  static constexpr const char *Name = "MpsBufferManager";
};

// Forward declaration
template <typename ResourceT> class MpsBufferManagerT;

// ============================================================================
// MpsBufferManagerT - Templated buffer manager using BaseManagerCore
// ============================================================================
template <typename ResourceT>
class MpsBufferManagerT
    : protected ::orteaf::internal::runtime::base::BaseManagerCore<
          MpsBufferManagerTraitsT<ResourceT>> {
public:
  using Traits = MpsBufferManagerTraitsT<ResourceT>;
  using Base = ::orteaf::internal::runtime::base::BaseManagerCore<Traits>;
  using Buffer = ::orteaf::internal::runtime::allocator::Buffer;
  using BufferHandle = typename Traits::Handle;
  using BufferLease =
      ::orteaf::internal::runtime::base::SharedLease<BufferHandle, Buffer *,
                                                     MpsBufferManagerT>;
  using DeviceType = typename Traits::DeviceType;
  using Pool = MpsBufferPoolT<ResourceT>;
  using LaunchParams = typename Pool::LaunchParams;
  using Resource = ResourceT;
  using ControlBlock = typename Base::ControlBlock;

  // =========================================================================
  // User-tunable configuration (Pool/Resource options with defaults)
  // =========================================================================
  struct Config {
    std::size_t chunk_size{16 * 1024 * 1024};
    std::size_t min_block_size{64};
    std::size_t max_block_size{16 * 1024 * 1024};
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsBufferUsage_t usage{
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

    Base::setupPool(capacity);
  }

  // =========================================================================
  // Shutdown (default params version)
  // =========================================================================
  void shutdown() { shutdown(default_params_); }

  // =========================================================================
  // Shutdown (explicit params version)
  // =========================================================================
  void shutdown(LaunchParams &params) {
    Base::teardownPool([this, &params](ControlBlock &cb, BufferHandle) {
      if (cb.isAlive()) {
        deallocateBuffer(cb.payload().buffer, params);
      }
    });

    pool_.~Pool();
    new (&pool_) Pool{};

    device_ = nullptr;
    heap_ = nullptr;
  }

  // =========================================================================
  // Acquire (allocate new buffer - default params version)
  // =========================================================================
  BufferLease acquire(std::size_t size, std::size_t alignment) {
    return acquire(size, alignment, default_params_);
  }

  // =========================================================================
  // Acquire (allocate new buffer - explicit params version)
  // =========================================================================
  BufferLease acquire(std::size_t size, std::size_t alignment,
                      LaunchParams &params) {
    Base::ensureInitialized();
    if (size == 0) {
      return {};
    }

    auto handle = Base::acquireFresh(
        [this, size, alignment, &params](MpsBufferResource &resource) {
          resource.buffer = allocateBuffer(size, alignment, params);
          return resource.buffer.valid();
        });

    if (handle == BufferHandle::invalid()) {
      return {};
    }

    return BufferLease{this, handle,
                       &Base::getControlBlock(handle).payload().buffer};
  }

  // =========================================================================
  // Acquire (share existing buffer by handle)
  // =========================================================================
  BufferLease acquire(BufferHandle handle) {
    Base::ensureInitialized();
    auto &cb = Base::acquireExisting(handle);
    return BufferLease{this, handle, &cb.payload().buffer};
  }

  // =========================================================================
  // Release (default params version)
  // =========================================================================
  void release(BufferHandle handle) { release(handle, default_params_); }

  void release(BufferLease &lease) noexcept {
    release(lease.handle(), default_params_);
    lease.invalidate();
  }

  // =========================================================================
  // Release (explicit params version)
  // =========================================================================
  void release(BufferHandle handle, LaunchParams &params) {
    if (!Base::isInitialized()) {
      return;
    }
    if (!Base::isValidHandle(handle)) {
      return;
    }

    Base::releaseAndDestroy(handle,
                            [this, &params](MpsBufferResource &resource) {
                              deallocateBuffer(resource.buffer, params);
                              resource.buffer = Buffer{};
                            });
  }

  void release(BufferLease &lease, LaunchParams &params) noexcept {
    release(lease.handle(), params);
    lease.invalidate();
  }

  Pool *pool() { return &pool_; }
  const Pool *pool() const { return &pool_; }

  // =========================================================================
  // Growth chunk size (for pool expansion)
  // =========================================================================
  using Base::growthChunkSize;
  using Base::setGrowthChunkSize;

  // Expose base methods
  using Base::capacity;
  using Base::isInitialized;

#if ORTEAF_ENABLE_TEST
  using Base::controlBlockForTest;
  using Base::freeListSizeForTest;
  using Base::isInitializedForTest;
#endif

private:
  Buffer allocateBuffer(std::size_t size, std::size_t alignment,
                        LaunchParams &params) {
    if (size == 0) {
      return {};
    }
    auto res = pool_.allocate(size, alignment, params);
    if (!res.valid()) {
      return {};
    }
    return Buffer{std::move(res), size, alignment};
  }

  void deallocateBuffer(Buffer &buffer, LaunchParams &params) {
    if (!buffer.valid()) {
      return;
    }
    auto &res = buffer.asResource<Backend::Mps>();
    if (!res.valid()) {
      return;
    }
    pool_.deallocate(std::move(res), buffer.size(), buffer.alignment(), params);
  }

  // Runtime state
  Pool pool_{};
  DeviceType device_{nullptr};
  ::orteaf::internal::base::DeviceHandle device_handle_{};
  HeapType heap_{nullptr};
  LaunchParams default_params_{};
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

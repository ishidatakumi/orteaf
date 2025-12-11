#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/runtime/allocator/buffer.h"
#include "orteaf/internal/runtime/allocator/policies/chunk_locator/direct_chunk_locator.h"
#include "orteaf/internal/runtime/allocator/policies/fast_free/fast_free_policies.h"
#include "orteaf/internal/runtime/allocator/policies/freelist/host_stack_freelist_policy.h"
#include "orteaf/internal/runtime/allocator/policies/large_alloc/direct_resource_large_alloc.h"
#include "orteaf/internal/runtime/allocator/policies/reuse/deferred_reuse_policy.h"
#include "orteaf/internal/runtime/allocator/policies/threading/threading_policies.h"
#include "orteaf/internal/runtime/allocator/pool/segregate_pool.h"
#include "orteaf/internal/runtime/base/buffer_manager.h"
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
            DirectResourceLargeAllocPolicy<ResourceT, Backend::Mps>,
        ::orteaf::internal::runtime::allocator::policies::
            DirectChunkLocatorPolicy<ResourceT, Backend::Mps>,
        ::orteaf::internal::runtime::allocator::policies::DeferredReusePolicy<
            ResourceT, Backend::Mps>,
        ::orteaf::internal::runtime::allocator::policies::
            HostStackFreelistPolicy<ResourceT, Backend::Mps>,
        Backend::Mps>;

// ============================================================================
// Traits template (simplified - OpsType is just Pool*)
// ============================================================================
template <typename ResourceT> struct MpsBufferManagerTraitsT {
  using BufferType = ::orteaf::internal::runtime::allocator::Buffer;
  using StateType = ::orteaf::internal::runtime::base::BufferState<BufferType>;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t;
  using OpsType = MpsBufferPoolT<ResourceT>; // Simplified: just the pool
  using HandleType = ::orteaf::internal::base::BufferHandle;

  static constexpr const char *Name = "MPS buffer manager";

  static BufferType allocate(OpsType *pool, std::size_t size,
                             std::size_t alignment) {
    if (pool == nullptr || size == 0) {
      return {};
    }
    typename OpsType::LaunchParams params{};
    auto res = pool->allocate(size, alignment, params);
    if (!res.valid()) {
      return {};
    }
    return BufferType{res, size, alignment};
  }

  static void deallocate(OpsType *pool, BufferType &buffer) {
    if (pool == nullptr || !buffer.valid()) {
      return;
    }
    auto res = buffer.asResource<Backend::Mps>();
    if (!res.valid()) {
      return;
    }
    typename OpsType::LaunchParams params{};
    pool->deallocate(res, buffer.size(), buffer.alignment(), params);
  }
};

// Forward declaration
template <typename ResourceT> class MpsBufferManagerT;

// ============================================================================
// MpsBufferManagerT - Templated buffer manager (simplified)
// ============================================================================
template <typename ResourceT>
class MpsBufferManagerT
    : public ::orteaf::internal::runtime::base::BufferManager<
          MpsBufferManagerT<ResourceT>, MpsBufferManagerTraitsT<ResourceT>> {
public:
  using Traits = MpsBufferManagerTraitsT<ResourceT>;
  using Base = ::orteaf::internal::runtime::base::BufferManager<
      MpsBufferManagerT<ResourceT>, Traits>;
  using Buffer = ::orteaf::internal::runtime::allocator::Buffer;
  using BufferHandle = typename Base::BufferHandle;
  using BufferLease = typename Base::BufferLease;
  using DeviceType = typename Traits::DeviceType;
  using Pool = MpsBufferPoolT<ResourceT>;
  using Resource = ResourceT;

  struct PoolConfig {
    std::size_t chunk_size{16 * 1024 * 1024};
    std::size_t min_block_size{64};
    std::size_t max_block_size{16 * 1024 * 1024};
  };

  MpsBufferManagerT() = default;
  MpsBufferManagerT(const MpsBufferManagerT &) = delete;
  MpsBufferManagerT &operator=(const MpsBufferManagerT &) = delete;

  MpsBufferManagerT(MpsBufferManagerT &&other) noexcept
      : Base(std::move(other)), pool_config_(std::move(other.pool_config_)),
        resource_config_(std::move(other.resource_config_)),
        backend_resource_(std::move(other.backend_resource_)),
        pool_(std::move(other.pool_)) {}

  MpsBufferManagerT &operator=(MpsBufferManagerT &&other) noexcept {
    if (this != &other) {
      Base::operator=(std::move(other));
      pool_config_ = std::move(other.pool_config_);
      resource_config_ = std::move(other.resource_config_);
      backend_resource_ = std::move(other.backend_resource_);
      pool_ = std::move(other.pool_);
    }
    return *this;
  }

  ~MpsBufferManagerT() = default;

  // =========================================================================
  // Configuration setters (call before initialize)
  // =========================================================================
  void setPoolConfig(const PoolConfig &config) { pool_config_ = config; }
  const PoolConfig &poolConfig() const { return pool_config_; }

  void setResourceConfig(const typename Resource::Config &config) {
    resource_config_ = config;
  }
  const typename Resource::Config &resourceConfig() const {
    return resource_config_;
  }

  // =========================================================================
  // Lifecycle
  // =========================================================================
  void initialize(DeviceType device, std::size_t capacity) {
    shutdown();

    backend_resource_.initialize(resource_config_);
    pool_.~Pool();
    new (&pool_) Pool(std::move(backend_resource_));

    typename Pool::Config pool_cfg{};
    pool_cfg.chunk_size = pool_config_.chunk_size;
    pool_cfg.min_block_size = pool_config_.min_block_size;
    pool_cfg.max_block_size = pool_config_.max_block_size;
    pool_cfg.fast_free.resource = pool_.resource();
    pool_cfg.threading.resource = pool_.resource();
    pool_cfg.large_alloc.resource = pool_.resource();
    pool_cfg.chunk_locator.resource = pool_.resource();
    pool_cfg.reuse.resource = pool_.resource();
    pool_cfg.freelist.resource = pool_.resource();
    pool_.initialize(pool_cfg);

    Base::initialize(device, &pool_, capacity);
  }

  void shutdown() {
    if (!Base::isInitialized()) {
      return;
    }
    Base::shutdown();
    pool_.~Pool();
    new (&pool_) Pool{};
    backend_resource_.~Resource();
    new (&backend_resource_) Resource{};
  }

  Pool *pool() { return &pool_; }
  const Pool *pool() const { return &pool_; }

private:
  friend Base;

  static Buffer allocate(Pool *pool, std::size_t size, std::size_t alignment) {
    return Traits::allocate(pool, size, alignment);
  }

  static void deallocate(Pool *pool, Buffer &buffer) {
    Traits::deallocate(pool, buffer);
  }

  // Configuration (set before initialize)
  PoolConfig pool_config_{};
  typename Resource::Config resource_config_{};

  // Runtime state
  Resource backend_resource_{};
  Pool pool_{};
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

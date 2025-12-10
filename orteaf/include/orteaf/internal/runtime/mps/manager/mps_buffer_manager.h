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
#include "orteaf/internal/runtime/allocator/resource/mps/mps_resource.h"
#include "orteaf/internal/runtime/base/backend_traits.h"
#include "orteaf/internal/runtime/base/buffer_manager.h"
#include "orteaf/internal/runtime/mps/resource/mps_buffer_view.h"

namespace orteaf::internal::runtime::mps::manager {

using ::orteaf::internal::backend::Backend;
using MpsResource =
    ::orteaf::internal::runtime::allocator::resource::mps::MpsResource;

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

// Default pool type
using MpsBufferPool = MpsBufferPoolT<MpsResource>;

// ============================================================================
// Ops context template
// ============================================================================
template <typename ResourceT> struct MpsBufferOpsT {
  MpsBufferPoolT<ResourceT> *pool{nullptr};
  typename MpsBufferPoolT<ResourceT>::LaunchParams launch_params{};
};

using MpsBufferOps = MpsBufferOpsT<MpsResource>;

// ============================================================================
// Traits template
// ============================================================================
template <typename ResourceT> struct MpsBufferManagerTraitsT {
  using BufferType = ::orteaf::internal::runtime::allocator::Buffer;
  using StateType = ::orteaf::internal::runtime::base::BufferState<BufferType>;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t;
  using OpsType = MpsBufferOpsT<ResourceT>;
  using HandleType = ::orteaf::internal::base::BufferHandle;

  static constexpr const char *Name = "MPS buffer manager";

  static BufferType allocate(OpsType *ops, std::size_t size,
                             std::size_t alignment);
  static void deallocate(OpsType *ops, BufferType &buffer);
};

using MpsBufferManagerTraits = MpsBufferManagerTraitsT<MpsResource>;

// Forward declaration
template <typename ResourceT> class MpsBufferManagerT;

// ============================================================================
// MpsBufferManagerT - Templated buffer manager
// ============================================================================
template <typename ResourceT = MpsResource>
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
  using Ops = typename Traits::OpsType;
  using Pool = MpsBufferPoolT<ResourceT>;
  using Resource = ResourceT;

  struct Config {
    typename Pool::Config pool{};
    typename Resource::Config resource{};
    std::size_t capacity{128};
  };

  void initialize(DeviceType device, Ops *ops, const Config &config) {
    shutdown();

    backend_resource_.initialize(config.resource);
    pool_.~Pool();
    new (&pool_) Pool(std::move(backend_resource_));

    typename Pool::Config pool_cfg = config.pool;
    pool_cfg.fast_free.resource = pool_.resource();
    pool_cfg.threading.resource = pool_.resource();
    pool_cfg.large_alloc.resource = pool_.resource();
    pool_cfg.chunk_locator.resource = pool_.resource();
    pool_cfg.reuse.resource = pool_.resource();
    pool_cfg.freelist.resource = pool_.resource();
    pool_.initialize(pool_cfg);

    ops_context_.pool = &pool_;
    ops_context_.launch_params = typename Pool::LaunchParams{};

    Base::initialize(device, &ops_context_, config.capacity);
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

  struct TraitsAdapter {
    static Buffer allocate(Ops *ops_ctx, std::size_t size,
                           std::size_t alignment) {
      if (ops_ctx == nullptr || ops_ctx->pool == nullptr) {
        return {};
      }
      auto res =
          ops_ctx->pool->allocate(size, alignment, ops_ctx->launch_params);
      if (!res.valid()) {
        return {};
      }
      return Buffer{res, size, alignment};
    }

    static void deallocate(Ops *ops_ctx, Buffer &buffer) {
      if (ops_ctx == nullptr || ops_ctx->pool == nullptr) {
        return;
      }
      auto res = buffer.asResource<Backend::Mps>();
      if (!res.valid()) {
        return;
      }
      ops_ctx->pool->deallocate(res, buffer.size(), buffer.alignment(),
                                ops_ctx->launch_params);
    }
  };

private:
  friend Base;

  static Buffer allocate(Ops *ops, std::size_t size, std::size_t alignment) {
    return TraitsAdapter::allocate(ops, size, alignment);
  }

  static void deallocate(Ops *ops, Buffer &buffer) {
    TraitsAdapter::deallocate(ops, buffer);
  }

  Ops ops_context_{};
  Resource backend_resource_{};
  Pool pool_{};
};

// ============================================================================
// Default type alias
// ============================================================================
using MpsBufferManager = MpsBufferManagerT<MpsResource>;

// ============================================================================
// Traits implementation
// ============================================================================
template <typename ResourceT>
inline typename MpsBufferManagerTraitsT<ResourceT>::BufferType
MpsBufferManagerTraitsT<ResourceT>::allocate(OpsType *ops, std::size_t size,
                                             std::size_t alignment) {
  return MpsBufferManagerT<ResourceT>::TraitsAdapter::allocate(ops, size,
                                                               alignment);
}

template <typename ResourceT>
inline void MpsBufferManagerTraitsT<ResourceT>::deallocate(OpsType *ops,
                                                           BufferType &buffer) {
  MpsBufferManagerT<ResourceT>::TraitsAdapter::deallocate(ops, buffer);
}

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

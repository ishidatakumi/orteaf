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
#include "orteaf/internal/runtime/base/lease/strong_lease.h"
#include "orteaf/internal/runtime/base/lease/weak_lease.h"
#include "orteaf/internal/runtime/base/manager/base_pool_manager_core.h"
#include "orteaf/internal/runtime/base/pool/slot_pool.h"
#include "orteaf/internal/runtime/mps/manager/mps_library_manager.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_device.h"
#include "orteaf/internal/runtime/mps/resource/mps_buffer_view.h"

namespace orteaf::internal::runtime::mps::manager {

using ::orteaf::internal::backend::Backend;

// ============================================================================
// SegregatePool type alias template (for GPU memory allocation)
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

// Forward declaration
template <typename ResourceT> class MpsBufferManagerT;

// ============================================================================
// BufferPayloadPoolTraits - Defines Payload/Handle/Request/Context for SlotPool
// ============================================================================
template <typename ResourceT> struct BufferPayloadPoolTraitsT {
  using Payload = ::orteaf::internal::runtime::allocator::Buffer;
  using Handle = ::orteaf::internal::base::BufferHandle;
  using SegregatePool = MpsBufferPoolT<ResourceT>;
  using LaunchParams = typename SegregatePool::LaunchParams;

  struct Request {
    std::size_t size{0};
    std::size_t alignment{0};
  };

  struct Context {
    SegregatePool *segregate_pool{nullptr};
    LaunchParams *launch_params{nullptr};
  };

  static bool create(Payload &payload, const Request &request,
                     const Context &context) {
    if (context.segregate_pool == nullptr || context.launch_params == nullptr) {
      return false;
    }
    if (request.size == 0) {
      // Zero-size is valid but results in invalid buffer
      payload = Payload{};
      return true;
    }
    auto res = context.segregate_pool->allocate(request.size, request.alignment,
                                                *context.launch_params);
    if (!res.valid()) {
      return false;
    }
    payload = Payload{std::move(res), request.size, request.alignment};
    return true;
  }

  static void destroy(Payload &payload, const Request &,
                      const Context &context) {
    if (!payload.valid()) {
      return;
    }
    if (context.segregate_pool == nullptr || context.launch_params == nullptr) {
      payload = Payload{};
      return;
    }
    auto &res = payload.template asResource<Backend::Mps>();
    if (res.valid()) {
      context.segregate_pool->deallocate(std::move(res), payload.size(),
                                         payload.alignment(),
                                         *context.launch_params);
    }
    payload = Payload{};
  }
};

// ============================================================================
// PayloadPool type alias
// ============================================================================
template <typename ResourceT>
using BufferPayloadPoolT = ::orteaf::internal::runtime::base::pool::SlotPool<
    BufferPayloadPoolTraitsT<ResourceT>>;

// ============================================================================
// ControlBlock type using SharedControlBlock
// ============================================================================
template <typename ResourceT>
using BufferControlBlockT =
    ::orteaf::internal::runtime::base::SharedControlBlock<
        ::orteaf::internal::base::BufferHandle,
        ::orteaf::internal::runtime::allocator::Buffer,
        BufferPayloadPoolT<ResourceT>>;

// ============================================================================
// Traits for BasePoolManagerCore
// ============================================================================
template <typename ResourceT> struct MpsBufferManagerTraitsT {
  using PayloadPool = BufferPayloadPoolT<ResourceT>;
  using ControlBlock = BufferControlBlockT<ResourceT>;
  struct ControlBlockTag {};
  using PayloadHandle = ::orteaf::internal::base::BufferHandle;
  static constexpr const char *Name = "MpsBufferManager";
};

// ============================================================================
// MpsBufferManagerT - Templated buffer manager using BasePoolManagerCore
// ============================================================================
template <typename ResourceT> class MpsBufferManagerT {
public:
  using Traits = MpsBufferManagerTraitsT<ResourceT>;
  using Core = ::orteaf::internal::runtime::base::BasePoolManagerCore<Traits>;
  using Buffer = ::orteaf::internal::runtime::allocator::Buffer;
  using BufferHandle = ::orteaf::internal::base::BufferHandle;
  using SegregatePool = MpsBufferPoolT<ResourceT>;
  using LaunchParams = typename SegregatePool::LaunchParams;
  using Resource = ResourceT;
  using DeviceType =
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t;

  using ControlBlock = typename Core::ControlBlock;
  using ControlBlockHandle = typename Core::ControlBlockHandle;
  using ControlBlockPool = typename Core::ControlBlockPool;
  using PayloadPool = typename Core::PayloadPool;

  // Lease types
  using StrongBufferLease = ::orteaf::internal::runtime::base::StrongLease<
      ControlBlockHandle, ControlBlock, ControlBlockPool, MpsBufferManagerT>;
  using WeakBufferLease = ::orteaf::internal::runtime::base::WeakLease<
      ControlBlockHandle, ControlBlock, ControlBlockPool, MpsBufferManagerT>;
  // Legacy alias for compatibility
  using BufferLease = StrongBufferLease;

private:
  friend StrongBufferLease;
  friend WeakBufferLease;

public:
  // HeapType deduced from Resource::Config
  using HeapType = decltype(std::declval<typename Resource::Config>().heap);

  // =========================================================================
  // Config - All dependencies and settings in one struct
  // =========================================================================
  struct Config {
    // Dependencies
    DeviceType device{nullptr};
    ::orteaf::internal::base::DeviceHandle device_handle{};
    HeapType heap{nullptr};
    MpsLibraryManager *library_manager{nullptr};
    // SegregatePool config
    std::size_t chunk_size{16 * 1024 * 1024};
    std::size_t min_block_size{64};
    std::size_t max_block_size{16 * 1024 * 1024};
    ::orteaf::internal::runtime::mps::platform::wrapper::MpsBufferUsage_t usage{
        ::orteaf::internal::runtime::mps::platform::wrapper::
            kMPSDefaultBufferUsage};
    // BasePoolManagerCore config
    std::size_t payload_capacity{0};
    std::size_t control_block_capacity{0};
    std::size_t payload_block_size{1};
    std::size_t control_block_block_size{1};
    std::size_t payload_growth_chunk_size{1};
    std::size_t control_block_growth_chunk_size{1};
  };

  // =========================================================================
  // Lifecycle
  // =========================================================================
  MpsBufferManagerT() = default;
  MpsBufferManagerT(const MpsBufferManagerT &) = delete;
  MpsBufferManagerT &operator=(const MpsBufferManagerT &) = delete;
  MpsBufferManagerT(MpsBufferManagerT &&) = default;
  MpsBufferManagerT &operator=(MpsBufferManagerT &&) = default;
  ~MpsBufferManagerT() = default;

  void configure(const Config &config) {
    shutdown();

    if (config.device == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(Traits::Name) + " requires a valid device");
    }
    if (config.heap == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(Traits::Name) + " requires a valid heap");
    }

    device_ = config.device;
    device_handle_ = config.device_handle;
    heap_ = config.heap;

    // Initialize SegregatePool
    typename Resource::Config res_cfg{};
    res_cfg.device = config.device;
    res_cfg.device_handle = config.device_handle;
    res_cfg.heap = config.heap;
    res_cfg.usage = config.usage;
    res_cfg.library_manager = config.library_manager;

    Resource backend_resource{};
    backend_resource.initialize(res_cfg);
    segregate_pool_.~SegregatePool();
    new (&segregate_pool_) SegregatePool(std::move(backend_resource));

    typename SegregatePool::Config pool_cfg{};
    pool_cfg.chunk_size = config.chunk_size;
    pool_cfg.min_block_size = config.min_block_size;
    pool_cfg.max_block_size = config.max_block_size;
    pool_cfg.fast_free.resource = segregate_pool_.resource();
    pool_cfg.threading.resource = segregate_pool_.resource();
    pool_cfg.large_alloc.resource = segregate_pool_.resource();
    pool_cfg.chunk_locator.resource = segregate_pool_.resource();
    pool_cfg.reuse.resource = segregate_pool_.resource();
    pool_cfg.freelist.resource = segregate_pool_.resource();
    segregate_pool_.initialize(pool_cfg);

    // Configure BasePoolManagerCore
    typename Core::Config core_cfg{};
    core_cfg.control_block_capacity = config.control_block_capacity;
    core_cfg.control_block_block_size = config.control_block_block_size;
    core_cfg.growth_chunk_size = config.control_block_growth_chunk_size;
    core_.configure(core_cfg);

    // Configure PayloadPool
    typename PayloadPool::Config payload_cfg{};
    payload_cfg.size = config.payload_capacity;
    payload_cfg.block_size = config.payload_block_size;
    core_.payloadPool().configure(payload_cfg);

    payload_growth_chunk_size_ = config.payload_growth_chunk_size;
    core_.setInitialized(true);
  }

  void shutdown() {
    if (!core_.isInitialized()) {
      return;
    }

    core_.checkCanShutdownOrThrow();

    // Destroy all payloads
    auto context = makePayloadContext();
    typename BufferPayloadPoolTraitsT<ResourceT>::Request request{};
    core_.payloadPool().shutdown(request, context);

    // Shutdown ControlBlock pool
    core_.shutdownControlBlockPool();

    // Shutdown SegregatePool
    segregate_pool_.~SegregatePool();
    new (&segregate_pool_) SegregatePool{};

    device_ = nullptr;
    heap_ = nullptr;
    core_.setInitialized(false);
  }

  // =========================================================================
  // Acquire (allocate new buffer)
  // =========================================================================
  StrongBufferLease acquire(std::size_t size, std::size_t alignment) {
    return acquire(size, alignment, default_params_);
  }

  StrongBufferLease acquire(std::size_t size, std::size_t alignment,
                            LaunchParams &params) {
    core_.ensureInitialized();
    if (size == 0) {
      return {};
    }

    // Get or grow PayloadPool slot
    typename BufferPayloadPoolTraitsT<ResourceT>::Request request{size,
                                                                  alignment};
    auto context = makePayloadContext(&params);
    auto payload_ref = core_.reserveUncreatedPayloadOrGrow(
        payload_growth_chunk_size_, request, context);
    if (!payload_ref.valid()) {
      return {};
    }

    // Create the buffer using emplace
    if (!core_.payloadPool().emplace(payload_ref.handle, request, context)) {
      core_.payloadPool().release(payload_ref.handle);
      return {};
    }

    // Acquire ControlBlock
    auto cb_ref = core_.acquireControlBlock();
    auto *cb = cb_ref.payload_ptr;

    // Bind payload to ControlBlock
    if (!cb->tryBindPayload(payload_ref.handle, payload_ref.payload_ptr,
                            &core_.payloadPool())) {
      // Rollback
      core_.payloadPool().destroy(payload_ref.handle, request, context);
      core_.payloadPool().release(payload_ref.handle);
      core_.releaseControlBlock(cb_ref.handle);
      return {};
    }

    return StrongBufferLease{cb, core_.controlBlockPoolForLease(),
                             cb_ref.handle};
  }

  // =========================================================================
  // Acquire (share existing buffer by handle)
  // =========================================================================
  StrongBufferLease acquire(BufferHandle handle) {
    core_.ensureInitialized();
    if (!core_.isAlive(handle)) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(Traits::Name) + " handle is not alive");
    }

    // Find ControlBlock for this payload handle
    // For now, create a new ControlBlock for the same payload
    auto *payload_ptr = core_.payloadPool().get(handle);
    if (payload_ptr == nullptr) {
      return {};
    }

    auto cb_ref = core_.acquireControlBlock();
    auto *cb = cb_ref.payload_ptr;

    if (!cb->tryBindPayload(handle, payload_ptr, &core_.payloadPool())) {
      core_.releaseControlBlock(cb_ref.handle);
      return {};
    }

    return StrongBufferLease{cb, core_.controlBlockPoolForLease(),
                             cb_ref.handle};
  }

  // =========================================================================
  // Release
  // =========================================================================
  void release(StrongBufferLease &lease) noexcept { lease.release(); }

  void release(WeakBufferLease &lease) noexcept { lease.release(); }

  // =========================================================================
  // Accessors
  // =========================================================================
  SegregatePool *pool() { return &segregate_pool_; }
  const SegregatePool *pool() const { return &segregate_pool_; }

#if ORTEAF_ENABLE_TEST
  bool isInitializedForTest() const noexcept { return core_.isInitialized(); }
  std::size_t payloadPoolSizeForTest() const noexcept {
    return core_.payloadPool().size();
  }
  std::size_t payloadPoolCapacityForTest() const noexcept {
    return core_.payloadPool().capacity();
  }
  std::size_t payloadPoolAvailableForTest() const noexcept {
    return core_.payloadPool().available();
  }
  std::size_t controlBlockPoolSizeForTest() const noexcept {
    return core_.controlBlockPoolSizeForTest();
  }
  std::size_t controlBlockPoolCapacityForTest() const noexcept {
    return core_.controlBlockPoolCapacityForTest();
  }
  bool isAliveForTest(BufferHandle handle) const noexcept {
    return core_.isAlive(handle);
  }
  std::size_t payloadGrowthChunkSizeForTest() const noexcept {
    return payload_growth_chunk_size_;
  }
  std::size_t controlBlockGrowthChunkSizeForTest() const noexcept {
    return core_.growthChunkSize();
  }
  const ControlBlock *controlBlockForTest(ControlBlockHandle handle) const {
    return core_.controlBlockPoolForLease()->get(handle);
  }
#endif

private:
  typename BufferPayloadPoolTraitsT<ResourceT>::Context
  makePayloadContext(LaunchParams *params = nullptr) noexcept {
    typename BufferPayloadPoolTraitsT<ResourceT>::Context ctx{};
    ctx.segregate_pool = &segregate_pool_;
    ctx.launch_params = params ? params : &default_params_;
    return ctx;
  }

  // Runtime state
  SegregatePool segregate_pool_{};
  DeviceType device_{nullptr};
  ::orteaf::internal::base::DeviceHandle device_handle_{};
  HeapType heap_{nullptr};
  LaunchParams default_params_{};
  std::size_t payload_growth_chunk_size_{1};
  Core core_{};
};

} // namespace orteaf::internal::runtime::mps::manager

// ============================================================================
// Default type alias (after namespace to avoid circular dependency)
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

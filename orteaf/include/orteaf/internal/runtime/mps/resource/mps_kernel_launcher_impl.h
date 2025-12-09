#pragma once

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/runtime/mps/ops/mps_common_ops.h" // Fix include
#include "orteaf/internal/runtime/mps/platform/mps_fast_ops.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_compute_command_encoder.h"
#include "orteaf/internal/runtime/mps/resource/mps_fence_ticket.h"
#include "orteaf/internal/runtime/mps/resource/mps_fence_token.h"
#include <array>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "orteaf/internal/runtime/mps/manager/mps_compute_pipeline_state_manager.h"
#include "orteaf/internal/runtime/mps/manager/mps_library_manager.h"

namespace orteaf::internal::runtime::mps::resource {

/**
 * @brief MPS/Metal compute pipeline launcher with per-device pipeline caching.
 *
 * Key points:
 * - Keys (library/function) are fixed at construction.
 * - Pipelines are cached per device handle. `initialize(device)` must be called
 *   before use on that device; subsequent calls for another device keep a
 * separate cache.
 * - Convenience helpers wrap command buffer / encoder creation, pipeline
 * binding, argument binding (buffers/bytes), threadgroup dispatch, end/commit,
 * and optional fence tracking.
 * - Fence helpers can encode an update on an encoder, return a ticket, and
 * optionally track it in a `MpsFenceToken` keyed by command queue handle.
 */
template <std::size_t N> class MpsKernelLauncherImpl {
public:
  using PipelineLease = manager::MpsComputePipelineStateManager::PipelineLease;
  using Key = std::pair<manager::LibraryKey, manager::FunctionKey>;
  struct KeyLiteral {
    const char *library;
    const char *function;
  };

  MpsKernelLauncherImpl() = default;

  explicit MpsKernelLauncherImpl(std::initializer_list<KeyLiteral> keys) {
    for (const auto &k : keys) {
      addKey(k.library, k.function);
    }
  }

  MpsKernelLauncherImpl(const MpsKernelLauncherImpl &) = delete;
  MpsKernelLauncherImpl &operator=(const MpsKernelLauncherImpl &) = delete;
  MpsKernelLauncherImpl(MpsKernelLauncherImpl &&) = default;
  MpsKernelLauncherImpl &operator=(MpsKernelLauncherImpl &&) = default;
  ~MpsKernelLauncherImpl() = default;

  /**
   * @brief Check whether pipelines are initialized for a given device.
   */
  bool
  initialized(::orteaf::internal::base::DeviceHandle device) const noexcept {
    const auto idx = findEntryIndex(device);
    return idx != kInvalidIndex && device_pipelines_[idx].initialized;
  }

  /**
   * @brief Build per-device pipeline cache for all registered keys.
   * Reinitializing a device clears and rebuilds its cache.
   */
  template <typename PrivateOps =
                ::orteaf::internal::runtime::mps::ops::MpsPrivateOps>
  void initialize(::orteaf::internal::base::DeviceHandle device) {
    auto entry_idx = findEntryIndex(device);
    if (entry_idx == kInvalidIndex) {
      device_pipelines_.pushBack(DevicePipelines{device});
      entry_idx = device_pipelines_.size() - 1;
    }
    auto &entry = device_pipelines_[entry_idx];
    entry.pipelines.clear();
    entry.pipelines.reserve(size_);
    for (std::size_t i = 0; i < size_; ++i) {
      const auto &key = keys_[i];
      entry.pipelines.pushBack(
          PrivateOps::acquirePipeline(device, key.first, key.second));
    }
    entry.initialized = true;
  }

#if ORTEAF_ENABLE_TEST
  const std::array<Key, N> &keysForTest() const noexcept { return keys_; }
  std::size_t sizeForTest() const noexcept { return size_; }
  PipelineLease &
  pipelineLeaseForTest(::orteaf::internal::base::DeviceHandle device,
                       std::size_t index) {
    auto idx = findEntryIndex(device);
    return device_pipelines_[idx].pipelines[index];
  }
  std::size_t pipelineCountForTest(
      ::orteaf::internal::base::DeviceHandle device) const noexcept {
    auto idx = findEntryIndex(device);
    return (idx == kInvalidIndex) ? 0 : device_pipelines_[idx].pipelines.size();
  }
#endif

  // Convenience: create a command buffer from a command queue without exposing
  // backend wrapper details to launcher users.
  template <
      typename FastOps = ::orteaf::internal::runtime::mps::platform::MpsFastOps>
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandBuffer_t
  createCommandBuffer(
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandQueue_t
          command_queue) const {
    return FastOps::createCommandBuffer(command_queue);
  }

  // Convenience: create a compute encoder and bind the pipeline in one step.
  template <
      typename FastOps = ::orteaf::internal::runtime::mps::platform::MpsFastOps>
  ::orteaf::internal::runtime::mps::platform::wrapper::
      MPSComputeCommandEncoder_t
      createComputeEncoder(::orteaf::internal::runtime::mps::platform::wrapper::
                               MPSCommandBuffer_t command_buffer,
                           ::orteaf::internal::base::DeviceHandle device,
                           std::size_t pipeline_index) const {
    const auto entry_idx = findEntryIndex(device);
    if (entry_idx == kInvalidIndex)
      return nullptr;
    const auto &entry = device_pipelines_[entry_idx];
    if (!entry.initialized || pipeline_index >= entry.pipelines.size()) {
      return nullptr;
    }
    auto *encoder = FastOps::createComputeCommandEncoder(command_buffer);
    FastOps::setPipelineState(encoder,
                              entry.pipelines[pipeline_index].pointer());
    return encoder;
  }

  template <
      typename FastOps = ::orteaf::internal::runtime::mps::platform::MpsFastOps>
  ::orteaf::internal::runtime::mps::platform::wrapper::
      MPSComputeCommandEncoder_t
      createComputeEncoder(::orteaf::internal::runtime::mps::platform::wrapper::
                               MPSCommandBuffer_t command_buffer,
                           ::orteaf::internal::base::DeviceHandle device,
                           std::string_view library,
                           std::string_view function) const {
    const std::size_t idx = findKeyIndex(library, function);
    const auto entry_idx = findEntryIndex(device);
    if (entry_idx == kInvalidIndex)
      return nullptr;
    const auto &entry = device_pipelines_[entry_idx];
    if (!entry.initialized || idx >= entry.pipelines.size()) {
      return nullptr;
    }
    auto *encoder = FastOps::createComputeCommandEncoder(command_buffer);
    FastOps::setPipelineState(encoder, entry.pipelines[idx].pointer());
    return encoder;
  }

  /** @brief Bind a buffer argument to the compute encoder. */
  template <
      typename FastOps = ::orteaf::internal::runtime::mps::platform::MpsFastOps>
  void setBuffer(
      ::orteaf::internal::runtime::mps::platform::wrapper::
          MPSComputeCommandEncoder_t encoder,
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSBuffer_t buffer,
      std::size_t offset, std::size_t index) const {
    FastOps::setBuffer(encoder, buffer, offset, index);
  }

  /** @brief Bind raw bytes to the compute encoder. */
  template <
      typename FastOps = ::orteaf::internal::runtime::mps::platform::MpsFastOps>
  void setBytes(::orteaf::internal::runtime::mps::platform::wrapper::
                    MPSComputeCommandEncoder_t encoder,
                const void *bytes, std::size_t length,
                std::size_t index) const {
    FastOps::setBytes(encoder, bytes, length, index);
  }

  /** @brief Set threadgroup and threads-per-threadgroup sizes. */
  template <
      typename FastOps = ::orteaf::internal::runtime::mps::platform::MpsFastOps>
  void dispatchThreadgroups(
      ::orteaf::internal::runtime::mps::platform::wrapper::
          MPSComputeCommandEncoder_t encoder,
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSSize_t
          threadgroups,
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSSize_t
          threads_per_threadgroup) const {
    FastOps::setThreadgroups(encoder, threadgroups, threads_per_threadgroup);
  }

  /** @brief End encoding on the compute encoder. */
  template <
      typename FastOps = ::orteaf::internal::runtime::mps::platform::MpsFastOps>
  void endEncoding(::orteaf::internal::runtime::mps::platform::wrapper::
                       MPSComputeCommandEncoder_t encoder) const {
    FastOps::endEncoding(encoder);
  }

  /** @brief Commit the command buffer for execution. */
  template <
      typename FastOps = ::orteaf::internal::runtime::mps::platform::MpsFastOps>
  void
  commit(::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandBuffer_t
             command_buffer) const {
    FastOps::commit(command_buffer);
  }

  /** @brief Encode a fence update on the encoder using an existing fence lease.
   */
  template <
      typename FastOps = ::orteaf::internal::runtime::mps::platform::MpsFastOps>
  void updateFence(
      ::orteaf::internal::runtime::mps::platform::wrapper::
          MPSComputeCommandEncoder_t encoder,
      ::orteaf::internal::runtime::mps::manager::MpsFenceManager::FenceLease
          &fence) const {
    FastOps::updateFence(encoder, fence.pointer());
  }

  /** @brief Encode a fence wait on the encoder using an existing fence lease.
   */
  template <
      typename FastOps = ::orteaf::internal::runtime::mps::platform::MpsFastOps>
  void waitForFence(::orteaf::internal::runtime::mps::platform::wrapper::
                        MPSComputeCommandEncoder_t encoder,
                    const ::orteaf::internal::runtime::mps::manager::
                        MpsFenceManager::FenceLease &fence) const {
    FastOps::waitForFence(encoder, fence.pointer());
  }

  /** @brief Encode waits for all fences stored in a fence token. */
  template <
      typename FastOps = ::orteaf::internal::runtime::mps::platform::MpsFastOps>
  void
  waitForFence(::orteaf::internal::runtime::mps::platform::wrapper::
                   MPSComputeCommandEncoder_t encoder,
               const ::orteaf::internal::runtime::mps::resource::MpsFenceToken
                   &token) const {
    for (const auto &ticket : token) {
      if (ticket.hasFence()) {
        FastOps::waitForFence(encoder, ticket.fenceHandle().pointer());
      }
    }
  }

  /** @brief Acquire a fence from the pool, encode an update, and return a
   * ticket. */
  template <
      typename FastOps = ::orteaf::internal::runtime::mps::platform::MpsFastOps,
      typename PrivateOps =
          ::orteaf::internal::runtime::mps::ops::MpsPrivateOps>
  ::orteaf::internal::runtime::mps::resource::MpsFenceTicket updateFence(
      ::orteaf::internal::base::DeviceHandle device,
      const ::orteaf::internal::runtime::mps::manager::MpsCommandQueueManager::
          CommandQueueLease &queue_lease,
      ::orteaf::internal::runtime::mps::platform::wrapper::
          MPSComputeCommandEncoder_t encoder,
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandBuffer_t
          command_buffer) const {
    auto fence_lease = PrivateOps::acquireFence(device);
    FastOps::updateFence(encoder, fence_lease.pointer());
    return ::orteaf::internal::runtime::mps::resource::MpsFenceTicket(
        queue_lease.handle(), command_buffer, std::move(fence_lease));
  }

  /** @brief Acquire/update a fence and track/replace the ticket in a fence
   * token. */
  template <
      typename FastOps = ::orteaf::internal::runtime::mps::platform::MpsFastOps,
      typename PrivateOps =
          ::orteaf::internal::runtime::mps::ops::MpsPrivateOps>
  void updateFenceAndTrack(
      ::orteaf::internal::base::DeviceHandle device,
      const ::orteaf::internal::runtime::mps::manager::MpsCommandQueueManager::
          CommandQueueLease &queue_lease,
      ::orteaf::internal::runtime::mps::platform::wrapper::
          MPSComputeCommandEncoder_t encoder,
      ::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandBuffer_t
          command_buffer,
      ::orteaf::internal::runtime::mps::resource::MpsFenceToken &token) const {
    auto ticket = updateFence<FastOps, PrivateOps>(device, queue_lease, encoder,
                                                   command_buffer);
    token.addOrReplaceTicket(std::move(ticket));
  }

  /**
   * @brief One-shot execute by pipeline index: create CB/encoder, bind
   * pipeline, call Binder to bind args, dispatch, optional fence update, end
   * and commit. Returns the command buffer or nullptr.
   */
  template <
      typename FastOps = ::orteaf::internal::runtime::mps::platform::MpsFastOps,
      typename PrivateOps =
          ::orteaf::internal::runtime::mps::ops::MpsPrivateOps,
      typename Binder>
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandBuffer_t
  dispatchOneShot(const ::orteaf::internal::runtime::mps::manager::
                      MpsCommandQueueManager::CommandQueueLease &queue_lease,
                  ::orteaf::internal::base::DeviceHandle device,
                  std::size_t pipeline_index,
                  ::orteaf::internal::runtime::mps::platform::wrapper::MPSSize_t
                      threadgroups,
                  ::orteaf::internal::runtime::mps::platform::wrapper::MPSSize_t
                      threads_per_threadgroup,
                  Binder &&binder,
                  ::orteaf::internal::runtime::mps::resource::MpsFenceToken
                      *fence_token = nullptr) const {
    const auto entry_idx = findEntryIndex(device);
    if (entry_idx == kInvalidIndex)
      return nullptr;
    const auto &entry = device_pipelines_[entry_idx];
    if (!entry.initialized || pipeline_index >= entry.pipelines.size())
      return nullptr;
    auto *command_buffer = FastOps::createCommandBuffer(queue_lease.pointer());
    auto *encoder = FastOps::createComputeCommandEncoder(command_buffer);
    FastOps::setPipelineState(encoder,
                              entry.pipelines[pipeline_index].pointer());
    static_cast<Binder &&>(binder)(encoder);
    FastOps::setThreadgroups(encoder, threadgroups, threads_per_threadgroup);
    if (fence_token && queue_lease.handle().isValid()) {
      updateFenceAndTrack<FastOps, PrivateOps>(device, queue_lease, encoder,
                                               command_buffer, *fence_token);
    }
    FastOps::endEncoding(encoder);
    FastOps::commit(command_buffer);
    return command_buffer;
  }

  /**
   * @brief One-shot execute by pipeline name: create CB/encoder, bind pipeline,
   * call Binder to bind args, dispatch, optional fence update, end and commit.
   * Returns the command buffer or nullptr.
   */
  template <
      typename FastOps = ::orteaf::internal::runtime::mps::platform::MpsFastOps,
      typename PrivateOps =
          ::orteaf::internal::runtime::mps::ops::MpsPrivateOps,
      typename Binder>
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSCommandBuffer_t
  dispatchOneShot(const ::orteaf::internal::runtime::mps::manager::
                      MpsCommandQueueManager::CommandQueueLease &queue_lease,
                  ::orteaf::internal::base::DeviceHandle device,
                  std::string_view library, std::string_view function,
                  ::orteaf::internal::runtime::mps::platform::wrapper::MPSSize_t
                      threadgroups,
                  ::orteaf::internal::runtime::mps::platform::wrapper::MPSSize_t
                      threads_per_threadgroup,
                  Binder &&binder,
                  ::orteaf::internal::runtime::mps::resource::MpsFenceToken
                      *fence_token = nullptr) const {
    const std::size_t idx = findKeyIndex(library, function);
    const auto entry_idx = findEntryIndex(device);
    if (entry_idx == kInvalidIndex)
      return nullptr;
    const auto &entry = device_pipelines_[entry_idx];
    if (!entry.initialized || idx >= entry.pipelines.size()) {
      return nullptr;
    }
    return dispatchOneShot<FastOps, PrivateOps>(
        queue_lease, device, idx, threadgroups, threads_per_threadgroup,
        static_cast<Binder &&>(binder), fence_token);
  }

private:
  // Append a key constructed from raw identifiers. Marks the launcher as not
  // initialized.
  void addKey(std::string library_identifier, std::string function_identifier) {
    addKeyInternal(manager::LibraryKey::Named(std::move(library_identifier)),
                   manager::FunctionKey::Named(std::move(function_identifier)));
  }

  void addKeyInternal(const manager::LibraryKey &lib,
                      const manager::FunctionKey &func) {
    if (isDuplicate(lib, func) || size_ >= N) {
      return;
    }
    keys_[size_++] = Key{lib, func};
    for (auto &entry : device_pipelines_) {
      entry.initialized = false;
      entry.pipelines.clear();
    }
  }

  std::size_t findKeyIndex(std::string_view library,
                           std::string_view function) const noexcept {
    for (std::size_t i = 0; i < size_; ++i) {
      const auto &key = keys_[i];
      if (key.first.identifier == library &&
          key.second.identifier == function) {
        return i;
      }
    }
    return std::numeric_limits<std::size_t>::max();
  }

  std::size_t
  findEntryIndex(::orteaf::internal::base::DeviceHandle device) const noexcept {
    for (std::size_t i = 0; i < device_pipelines_.size(); ++i) {
      if (device_pipelines_[i].device == device) {
        return i;
      }
    }
    return kInvalidIndex;
  }

  bool isDuplicate(const manager::LibraryKey &lib,
                   const manager::FunctionKey &func) const {
    for (std::size_t i = 0; i < size_; ++i) {
      const auto &key = keys_[i];
      if (key.first == lib && key.second == func) {
        return true;
      }
    }
    return false;
  }

  struct DevicePipelines {
    ::orteaf::internal::base::DeviceHandle device{};
    ::orteaf::internal::base::HeapVector<PipelineLease> pipelines{};
    bool initialized{false};
  };

  ::orteaf::internal::base::HeapVector<DevicePipelines> device_pipelines_{};
  std::array<Key, N> keys_{};
  std::size_t size_{0};
  static constexpr std::size_t kInvalidIndex =
      std::numeric_limits<std::size_t>::max();
};

} // namespace orteaf::internal::runtime::mps::resource

#endif // ORTEAF_ENABLE_MPS

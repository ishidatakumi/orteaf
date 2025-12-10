#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/allocator/buffer.h"
#include "orteaf/internal/runtime/base/base_manager.h"

namespace orteaf::internal::runtime::base {

/**
 * @brief 汎用的なバッファプール状態。
 *
 * Buffer は type-erased な Resource を持ち、alive フラグは持たず、
 * in_use と ref_count で管理する。
 */
template <typename Buffer> struct BufferState {
  std::atomic<std::size_t> ref_count{0};
  Buffer buffer{};
  std::uint32_t generation{0};
  bool in_use{false};
};

/**
 * @brief Buffer 向けの汎用マネージャ。
 *
 * Traits 要件:
 *  - using BufferType     = ::orteaf::internal::runtime::allocator::Buffer;
 *    （あるいはそれに準じる型消去リソース）
 *  - using StateType      = BufferState<BufferType>;
 *  - using DeviceType     = backend に依存するデバイス型;
 *  - using OpsType        = allocate/deallocate を提供する遅延実行 Ops;
 *  - using HandleType     = ::orteaf::internal::base::BufferHandle;
 *  - static constexpr const char* Name;
 *  - static BufferType allocate(OpsType*, BufferHandle handle,
 *                               std::size_t size, std::size_t alignment);
 *  - static void deallocate(OpsType*, BufferType&);
 */
template <typename Derived, typename Traits>
class BufferManager : public BaseManager<Derived, Traits> {
public:
  using Base = BaseManager<Derived, Traits>;
  using BufferType = typename Traits::BufferType;
  using State = typename Traits::StateType;
  using Device = typename Traits::DeviceType;
  using Ops = typename Traits::OpsType;
  using BufferHandle = typename Traits::HandleType;

  using Base::device_;
  using Base::free_list_;
  using Base::growth_chunk_size_;
  using Base::initialized_;
  using Base::ops_;
  using Base::states_;

  void initialize(Device device, Ops *ops, std::size_t capacity) {
    shutdown();
    if (device == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(Traits::Name) + " requires a valid device");
    }
    if (ops == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(Traits::Name) + " requires valid ops");
    }
    if (capacity > BufferHandle::invalid_index()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          "Requested " + std::string(Traits::Name) +
              " capacity exceeds supported limit");
    }
    device_ = device;
    ops_ = ops;
    states_.clear();
    free_list_.clear();
    if (capacity > 0) {
      Base::growPool(capacity);
    }
    initialized_ = true;
  }

  void shutdown() {
    if (!initialized_) {
      return;
    }
    for (std::size_t i = 0; i < states_.size(); ++i) {
      State &state = states_[i];
      if (state.in_use) {
        Traits::deallocate(ops_, state.buffer);
      }
      state.buffer = BufferType{};
      state.in_use = false;
      state.ref_count.store(0, std::memory_order_relaxed);
    }
    states_.clear();
    free_list_.clear();
    device_ = nullptr;
    ops_ = nullptr;
    initialized_ = false;
  }

  /**
   * @brief 新規にバッファを確保して返す。
   */
  BufferType require(std::size_t size, std::size_t alignment) {
    Base::ensureInitialized();
    if (size == 0) {
      return BufferType{};
    }
    const std::size_t index = Base::allocateSlot();
    State &state = states_[index];

    const auto handle =
        BufferHandle{static_cast<typename BufferHandle::index_type>(index),
                     static_cast<typename BufferHandle::generation_type>(
                         state.generation)};

    state.buffer = Traits::allocate(ops_, size, alignment);
    if (!state.buffer.valid()) {
      // release slot back to free list
      free_list_.pushBack(index);
      return BufferType{};
    }

    state.in_use = true;
    state.ref_count.store(1, std::memory_order_relaxed);
    return state.buffer;
  }

  /**
   * @brief 既存ハンドルを参照し ref_count を増やす。
   */
  BufferType require(BufferHandle handle) {
    Base::ensureInitialized();
    const std::size_t index = static_cast<std::size_t>(handle.index);
    if (index >= states_.size()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
          std::string(Traits::Name) + " handle out of range");
    }
    State &state = states_[index];
    if (!state.in_use) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(Traits::Name) + " handle is inactive");
    }
    if (static_cast<std::size_t>(state.generation) !=
        static_cast<std::size_t>(handle.generation)) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          std::string(Traits::Name) + " handle is stale");
    }
    state.ref_count.fetch_add(1, std::memory_order_relaxed);
    return state.buffer;
  }

  /**
   * @brief 参照を解放し、ref_count が 0 になれば deallocate する。
   */
  void release(BufferHandle handle) {
    if (!initialized_) {
      return;
    }
    const std::size_t index = static_cast<std::size_t>(handle.index);
    if (index >= states_.size()) {
      return;
    }
    State &state = states_[index];
    if (!state.in_use) {
      return;
    }
    if (static_cast<std::size_t>(state.generation) !=
        static_cast<std::size_t>(handle.generation)) {
      return;
    }
    const auto prev =
        state.ref_count.fetch_sub(1, std::memory_order_acq_rel);
    if (prev == 1) {
      Traits::deallocate(ops_, state.buffer);
      state.buffer = BufferType{};
      state.in_use = false;
      ++state.generation;
      free_list_.pushBack(index);
    }
  }
};

} // namespace orteaf::internal::runtime::base

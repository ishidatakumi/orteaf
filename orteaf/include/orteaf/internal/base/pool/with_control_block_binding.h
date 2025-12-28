#pragma once

#include <cstddef>

#include "orteaf/internal/base/heap_vector.h"

namespace orteaf::internal::base::pool {

/**
 * @brief ControlBlockバインディング機能を追加するMixinクラス
 *
 * 既存のPoolクラス（SlotPool、FixedSlotStore）を継承し、
 * Payload-ControlBlock間の1対1追跡機能を追加する。
 *
 * 使用例:
 * @code
 *   using BoundSlotPool = WithControlBlockBinding<SlotPool<Traits>, CBHandle>;
 *   using BoundFixedSlotStore = WithControlBlockBinding<FixedSlotStore<Traits>,
 * CBHandle>;
 * @endcode
 *
 * @tparam BasePool 継承元のPoolクラス（SlotPoolまたはFixedSlotStore）
 * @tparam ControlBlockHandleT ControlBlock識別用のHandle型
 */
template <typename BasePool, typename ControlBlockHandleT>
class WithControlBlockBinding : public BasePool {
public:
  // ===========================================================================
  // Type Aliases
  // ===========================================================================

  using Base = BasePool;
  using ControlBlockHandle = ControlBlockHandleT;
  using Handle = typename Base::Handle;
  // SlotRef removed - use Handle and get(handle) instead

  // ===========================================================================
  // Lifecycle
  // ===========================================================================

  WithControlBlockBinding() = default;
  WithControlBlockBinding(const WithControlBlockBinding &) = delete;
  WithControlBlockBinding &operator=(const WithControlBlockBinding &) = delete;
  WithControlBlockBinding(WithControlBlockBinding &&) = default;
  WithControlBlockBinding &operator=(WithControlBlockBinding &&) = default;
  ~WithControlBlockBinding() = default;

  // ===========================================================================
  // Configuration (Override to sync bound_control_blocks_)
  // ===========================================================================

  /**
   * @brief Poolをリサイズし、バインディング配列も拡張
   */
  std::size_t resize(std::size_t new_size) {
    const std::size_t old_size = Base::resize(new_size);
    syncBoundControlBlocks();
    return old_size;
  }

  /**
   * @brief Poolをクリアし、バインディング配列もクリア
   */
  template <typename... Args> void clear(Args &&...args) {
    Base::clear(std::forward<Args>(args)...);
    bound_control_blocks_.clear();
  }

  // ===========================================================================
  // ControlBlock Binding Operations
  // ===========================================================================

  /**
   * @brief 指定されたPayloadにバインドされたCBがあるかチェック
   *
   * @param handle Payload Handle
   * @return バインドされたCBが存在すればtrue
   */
  bool hasBoundControlBlock(Handle handle) const noexcept {
    if (!Base::isValid(handle)) {
      return false;
    }
    const auto idx = static_cast<std::size_t>(handle.index);
    if (idx >= bound_control_blocks_.size()) {
      return false;
    }
    return bound_control_blocks_[idx].isValid();
  }

  /**
   * @brief バインドされたControlBlock Handleを取得
   *
   * @param handle Payload Handle
   * @return バインドされたCB Handle (存在しなければinvalid)
   */
  ControlBlockHandle getBoundControlBlock(Handle handle) const noexcept {
    if (!Base::isValid(handle)) {
      return ControlBlockHandle::invalid();
    }
    const auto idx = static_cast<std::size_t>(handle.index);
    if (idx >= bound_control_blocks_.size()) {
      return ControlBlockHandle::invalid();
    }
    return bound_control_blocks_[idx];
  }

  /**
   * @brief ControlBlockをPayloadにバインド
   *
   * @param payload_handle Payload Handle
   * @param cb_handle バインドするControlBlock Handle
   */
  void bindControlBlock(Handle payload_handle,
                        ControlBlockHandle cb_handle) noexcept {
    if (!Base::isValid(payload_handle)) {
      return;
    }
    const auto idx = static_cast<std::size_t>(payload_handle.index);
    if (idx >= bound_control_blocks_.size()) {
      return;
    }
    bound_control_blocks_[idx] = cb_handle;
  }

  /**
   * @brief PayloadからControlBlockをアンバインド
   *
   * @param payload_handle Payload Handle
   */
  void unbindControlBlock(Handle payload_handle) noexcept {
    if (!Base::isValid(payload_handle)) {
      return;
    }
    const auto idx = static_cast<std::size_t>(payload_handle.index);
    if (idx >= bound_control_blocks_.size()) {
      return;
    }
    bound_control_blocks_[idx] = ControlBlockHandle::invalid();
  }

  // ===========================================================================
  // Release Overrides (unbind on release)
  // ===========================================================================

  /**
   * @brief Release a payload slot and unbind its control block.
   *
   * Release always clears the bound control block for the payload slot.
   */
  bool release(Handle payload_handle) noexcept {
    unbindControlBlock(payload_handle);
    return Base::release(payload_handle);
  }

  /**
   * @brief Release a payload slot with request/context and unbind its control block.
   *
   * Release always clears the bound control block for the payload slot.
   */
  bool release(Handle payload_handle, const typename Base::Request &request,
               const typename Base::Context &context) noexcept {
    unbindControlBlock(payload_handle);
    return Base::release(payload_handle, request, context);
  }

#if ORTEAF_ENABLE_TEST
  // ===========================================================================
  // Test Support
  // ===========================================================================

  std::size_t boundControlBlocksSize() const noexcept {
    return bound_control_blocks_.size();
  }
#endif

private:
  /**
   * @brief バインディング配列をPool sizeに同期
   */
  void syncBoundControlBlocks() {
    const std::size_t pool_size = Base::size();
    if (bound_control_blocks_.size() < pool_size) {
      const std::size_t old_size = bound_control_blocks_.size();
      bound_control_blocks_.resize(pool_size);
      // 新規スロットはinvalidで初期化
      for (std::size_t i = old_size; i < pool_size; ++i) {
        bound_control_blocks_[i] = ControlBlockHandle::invalid();
      }
    }
  }

  // ===========================================================================
  // Members
  // ===========================================================================

  ::orteaf::internal::base::HeapVector<ControlBlockHandle>
      bound_control_blocks_{};
};

} // namespace orteaf::internal::base::pool

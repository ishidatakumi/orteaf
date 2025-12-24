#pragma once

#include <cstddef>

#include <string>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/diagnostics/error/error_macros.h"
#include "orteaf/internal/diagnostics/log/log.h"
#include "orteaf/internal/execution/allocator/buffer_resource.h"
#include "orteaf/internal/execution/allocator/policies/policy_config.h"

namespace orteaf::internal::execution::allocator::policies {

template <typename Resource>
class DirectResourceLargeAllocPolicy {
public:
  using BufferViewHandle = ::orteaf::internal::base::BufferViewHandle;
  using BufferView = Resource::BufferView;
  using BufferBlock = Resource::BufferBlock;

  DirectResourceLargeAllocPolicy() = default;
  DirectResourceLargeAllocPolicy(const DirectResourceLargeAllocPolicy &) =
      delete;
  DirectResourceLargeAllocPolicy &
  operator=(const DirectResourceLargeAllocPolicy &) = delete;
  DirectResourceLargeAllocPolicy(DirectResourceLargeAllocPolicy &&) = default;
  DirectResourceLargeAllocPolicy &
  operator=(DirectResourceLargeAllocPolicy &&) = default;
  ~DirectResourceLargeAllocPolicy() = default;

  struct Config : PolicyConfig<Resource> {};

  void initialize(const Config &config) {
    ORTEAF_THROW_IF_NULL(
        config.resource,
        "DirectResourceLargeAllocPolicy requires non-null Resource*");
    resource_ = config.resource;
  }

  BufferBlock allocate(std::size_t size, std::size_t alignment) {
    ORTEAF_THROW_IF(resource_ == nullptr, InvalidState,
                    "DirectResourceLargeAllocPolicy is not initialized");

    if (size == 0) {
      return {};
    }

    BufferView buffer = resource_->allocate(size, alignment);
    if (buffer.empty()) {
      return {};
    }

    const std::size_t index = reserveSlot();
    Entry entry{};
    entry.view = buffer;
    entry.in_use = true;
#if ORTEAF_CORE_DEBUG_ENABLED
    entry.size = size;
    entry.alignment = alignment;
#endif
    entries_[index] = entry;
    return BufferBlock(encodeId(index), buffer);
  }

  void deallocate(BufferViewHandle handle, std::size_t size,
                  std::size_t alignment) {

    if (!isLargeAlloc(handle)) {
      return;
    }

    const std::size_t index = indexFromId(handle);
    if (index >= entries_.size()) {
      return;
    }

    Entry &entry = entries_[index];
    if (!entry.in_use) {
      return;
    }

    resource_->deallocate(entry.view, size, alignment);
#if ORTEAF_CORE_DEBUG_ENABLED
    ORTEAF_LOG_DEBUG_IF(Core,
                        entry.size != size || entry.alignment != alignment,
                        "LargeAlloc deallocate mismatch: recorded size=" +
                            std::to_string(entry.size) +
                            " align=" + std::to_string(entry.alignment) +
                            " called size=" + std::to_string(size) +
                            " align=" + std::to_string(alignment));
#endif
    entry = Entry{};
    free_list_.pushBack(static_cast<std::size_t>(index));
  }

  bool isLargeAlloc(BufferViewHandle handle) const {
    // 上位ビットでLarge/Chunkを判定
    return (static_cast<BufferViewHandle::underlying_type>(handle) &
            kLargeMask) != 0;
  }

  bool isAlive(BufferViewHandle handle) const {
    if (!isLargeAlloc(handle)) {
      return false;
    }

    const std::size_t index = indexFromId(handle);
    return index < entries_.size() && entries_[index].in_use &&
           entries_[index].view;
  }

  std::size_t size() const { return entries_.size() - free_list_.size(); }

private:
  static constexpr BufferViewHandle::underlying_type kLargeMask =
      BufferViewHandle::underlying_type{1u} << 31;
  static constexpr BufferViewHandle::underlying_type kIndexMask = ~kLargeMask;

  struct Entry {
    BufferView view{};
    bool in_use{false};
#if ORTEAF_CORE_DEBUG_ENABLED
    std::size_t size{};
    std::size_t alignment{};
#endif
  };

  BufferViewHandle encodeId(std::size_t index) const {
    // Large用のビットを立てて衝突を避ける
    return BufferViewHandle{
        static_cast<BufferViewHandle::underlying_type>(index) | kLargeMask};
  }

  std::size_t indexFromId(BufferViewHandle handle) const {
    // Large判定ビットを落としてインデックスに戻す
    return static_cast<std::size_t>(
        static_cast<BufferViewHandle::underlying_type>(handle) & kIndexMask);
  }

  std::size_t reserveSlot() {
    if (!free_list_.empty()) {
      const auto index = free_list_.back();
      free_list_.resize(free_list_.size() - 1);
      return index;
    }
    entries_.emplaceBack();
    return entries_.size() - 1;
  }

  Resource *resource_{nullptr};
  ::orteaf::internal::base::HeapVector<Entry> entries_;
  ::orteaf::internal::base::HeapVector<std::size_t> free_list_;
};

} // namespace orteaf::internal::execution::allocator::policies

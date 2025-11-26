#pragma once

#include <cstddef>
#include <string>
#include <mutex>

#include "orteaf/internal/backend/backend.h"
#include "orteaf/internal/backend/backend_traits.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/base/strong_id.h"
#include "orteaf/internal/diagnostics/log/log.h"
#include "orteaf/internal/runtime/allocator/memory_block.h"

namespace orteaf::internal::runtime::allocator::policies {

template <typename Resource, ::orteaf::internal::backend::Backend B>
class DirectResourceLargeAllocPolicy {
public:
    using BufferId = ::orteaf::internal::base::BufferId;
    using BufferView = typename ::orteaf::internal::backend::BackendTraits<B>::BufferView;
    using MemoryBlock = ::orteaf::internal::runtime::allocator::MemoryBlock<B>;

    void initialize() {}

    MemoryBlock allocate(std::size_t size, std::size_t alignment) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size == 0) {
            return {};
        }

        BufferView buffer = Resource::allocate(size, alignment);
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
        return MemoryBlock(encodeId(index), buffer);
    }

    void deallocate(BufferId id, std::size_t size, std::size_t alignment) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!isLargeAlloc(id)) {
            return;
        }

        const std::size_t index = indexFromId(id);
        if (index >= entries_.size()) {
            return;
        }

        Entry& entry = entries_[index];
        if (!entry.in_use) {
            return;
        }

        Resource::deallocate(entry.view, size, alignment);
#if ORTEAF_CORE_DEBUG_ENABLED
        ORTEAF_LOG_DEBUG_IF(Core,
                            entry.size != size || entry.alignment != alignment,
                            "LargeAlloc deallocate mismatch: recorded size=" + std::to_string(entry.size) +
                                " align=" + std::to_string(entry.alignment) + " called size=" +
                                std::to_string(size) + " align=" + std::to_string(alignment));
#endif
        entry = Entry{};
        free_list_.pushBack(static_cast<std::size_t>(index));
    }

    bool isLargeAlloc(BufferId id) const {
        // 上位ビットでLarge/Chunkを判定
        return (static_cast<BufferId::underlying_type>(id) & kLargeMask) != 0;
    }

    bool isAlive(BufferId id) const {
        if (!isLargeAlloc(id)) {
            return false;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        const std::size_t index = indexFromId(id);
        return index < entries_.size() && entries_[index].in_use && entries_[index].view;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size() - free_list_.size();
    }

private:
    static constexpr BufferId::underlying_type kLargeMask = BufferId::underlying_type{1u} << 31;
    static constexpr BufferId::underlying_type kIndexMask = ~kLargeMask;

    struct Entry {
        BufferView view{};
        bool in_use{false};
#if ORTEAF_CORE_DEBUG_ENABLED
        std::size_t size{};
        std::size_t alignment{};
#endif
    };

    BufferId encodeId(std::size_t index) const {
        // Large用のビットを立てて衝突を避ける
        return BufferId{static_cast<BufferId::underlying_type>(index) | kLargeMask};
    }

    std::size_t indexFromId(BufferId id) const {
        // Large判定ビットを落としてインデックスに戻す
        return static_cast<std::size_t>(static_cast<BufferId::underlying_type>(id) & kIndexMask);
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

    mutable std::mutex mutex_;
    base::HeapVector<Entry> entries_;
    base::HeapVector<std::size_t> free_list_;
};

}  // namespace orteaf::internal::runtime::allocator::policies

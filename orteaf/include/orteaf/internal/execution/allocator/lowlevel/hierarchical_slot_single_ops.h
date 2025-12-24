#pragma once

#include "orteaf/internal/execution/allocator/lowlevel/hierarchical_slot_storage.h"

namespace orteaf::internal::execution::allocator::policies {

/**
 * @brief 単一スロット操作（FastSingle用）
 */
template <class HeapOps, ::orteaf::internal::backend::Backend B>
class HierarchicalSlotSingleOps {
public:
    using Storage = HierarchicalSlotStorage<HeapOps, B>;
    using BufferView = typename Storage::BufferView;
    using HeapRegion = typename Storage::HeapRegion;
    using Slot = typename Storage::Slot;
    using Layer = typename Storage::Layer;
    using State = typename Storage::State;

    explicit HierarchicalSlotSingleOps(Storage& storage) : storage_(storage) {}

    // ========================================================================
    // Single slot allocation
    // ========================================================================

    BufferView allocate(std::size_t size) {
        std::lock_guard<std::mutex> lock(storage_.mutex());

        uint32_t target = Storage::pickLayer(storage_.config().levels, size);
        ORTEAF_THROW_IF(target == Storage::kInvalidLayer, OutOfMemory, "No suitable layer");

        ensureSlotAvailable(target);
        uint32_t slot_idx = acquireSlot(target);
        return mapSlot(target, slot_idx);
    }

    void deallocate(BufferView view) {
        if (!view) return;
        std::lock_guard<std::mutex> lock(storage_.mutex());

        auto [layer_idx, slot_idx] = findSlot(view);
        if (layer_idx == Storage::kInvalidLayer) return;

        Slot& slot = storage_.layers()[layer_idx].slots[slot_idx];
        if (slot.state != State::InUse) return;

        unmapSlot(layer_idx, slot_idx);
        releaseSlot(layer_idx, slot_idx);
        tryMergeUpward(layer_idx, slot_idx);
    }

    // ========================================================================
    // Internal operations (exposed for DenseOps)
    // ========================================================================

    void ensureSlotAvailable(uint32_t target_layer) {
        auto& layers = storage_.layers();
        if (Storage::hasFreeSlot(layers[target_layer])) {
            return;
        }

        int parent_layer = findParentWithFreeSlot(target_layer);

        if (parent_layer < 0) {
            std::size_t expand = storage_.config().expand_bytes;
            if (expand == 0) {
                expand = layers[0].slot_size;
            }
            storage_.addRegion(expand);
            parent_layer = 0;
        }

        for (int i = parent_layer; i < static_cast<int>(target_layer); ++i) {
            uint32_t slot_idx = Storage::popFreeSlot(layers[i]);
            Storage::splitSlot(layers[i], layers[i + 1], slot_idx);
        }
    }

    uint32_t acquireSlot(uint32_t layer_index) {
        Layer& layer = storage_.layers()[layer_index];
        uint32_t slot_idx = Storage::popFreeSlot(layer);
        Storage::markSlotInUse(layer.slots[slot_idx]);
        return slot_idx;
    }

    // Dense用: 既知のインデックスをそのまま確保する
    void acquireSpecificSlot(uint32_t layer_index, uint32_t slot_index) {
        Layer& layer = storage_.layers()[layer_index];
        ORTEAF_THROW_IF(slot_index >= layer.slots.size(), OutOfMemory, "Slot index out of range");
        Slot& slot = layer.slots[slot_index];
        ORTEAF_THROW_IF(slot.state != State::Free, OutOfMemory, "Slot not free");
        Storage::markSlotInUse(slot);
        // free_list からは呼び出し元で調整済み（or 未登録）想定
    }

    BufferView mapSlot(uint32_t layer_index, uint32_t slot_index) {
        Slot& slot = storage_.layers()[layer_index].slots[slot_index];
        return storage_.heapOps()->map(slot.region);
    }

    void unmapSlot(uint32_t layer_index, uint32_t slot_index) {
        Layer& layer = storage_.layers()[layer_index];
        Slot& slot = layer.slots[slot_index];
        storage_.heapOps()->unmap(slot.region, layer.slot_size);
    }

    void releaseSlot(uint32_t layer_index, uint32_t slot_index) {
        Layer& layer = storage_.layers()[layer_index];
        Storage::markSlotFree(layer.slots[slot_index]);
        layer.free_list.pushBack(slot_index);
    }

    std::pair<uint32_t, uint32_t> findSlot(BufferView view) const {
        const auto& layers = storage_.layers();
        for (uint32_t layer_idx = 0; layer_idx < layers.size(); ++layer_idx) {
            const auto& layer = layers[layer_idx];
            for (uint32_t slot_idx = 0; slot_idx < layer.slots.size(); ++slot_idx) {
                const Slot& slot = layer.slots[slot_idx];
                if (slot.state == State::InUse && slot.region.data() == view.data()) {
                    return {layer_idx, slot_idx};
                }
            }
        }
        return {Storage::kInvalidLayer, 0};
    }

    void tryMergeUpward(uint32_t layer_idx, uint32_t slot_idx) {
        auto& layers = storage_.layers();
        Slot& slot = layers[layer_idx].slots[slot_idx];
        if (slot.parent_slot == Storage::kNoParent || layer_idx == 0) return;

        uint32_t parent_layer_idx = layer_idx - 1;
        uint32_t parent_slot_idx = slot.parent_slot;

        Layer& parent_layer = layers[parent_layer_idx];
        Layer& child_layer = layers[layer_idx];
        Slot& parent = parent_layer.slots[parent_slot_idx];

        if (parent.state != State::Split) return;

        const std::size_t count = parent_layer.slot_size / child_layer.slot_size;
        if (!Storage::allSiblingsFree(child_layer, parent.child_begin, count)) return;

        // 子をfree_listから除去
        ::orteaf::internal::base::HeapVector<uint32_t> new_free_list;
        for (std::size_t i = 0; i < child_layer.free_list.size(); ++i) {
            const auto idx = child_layer.free_list[i];
            if (idx < parent.child_begin || idx >= parent.child_begin + count) {
                new_free_list.pushBack(idx);
            }
        }
        child_layer.free_list = std::move(new_free_list);

        // span_free_listにchild_beginを追加（再利用用）
        child_layer.span_free_list.pushBack(parent.child_begin);

        // 親をFreeに戻す
        Storage::markSlotFree(parent);
        parent.child_begin = 0;
        parent_layer.free_list.pushBack(parent_slot_idx);

        // 再帰的に上へ
        tryMergeUpward(parent_layer_idx, parent_slot_idx);
    }

    int findParentWithFreeSlot(uint32_t target_layer) const noexcept {
        const auto& layers = storage_.layers();
        for (int i = static_cast<int>(target_layer) - 1; i >= 0; --i) {
            if (Storage::hasFreeSlot(layers[i])) {
                return i;
            }
        }
        return -1;
    }

private:
    Storage& storage_;
};

}  // namespace orteaf::internal::execution::allocator::policies

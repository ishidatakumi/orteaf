#pragma once

#include "orteaf/internal/runtime/allocator/lowlevel/hierarchical_slot_single_ops.h"

namespace orteaf::internal::runtime::allocator::policies {

/**
 * @brief 複数スロット操作（Dense用）
 */
template <class HeapOps, ::orteaf::internal::backend::Backend B>
class HierarchicalSlotDenseOps {
public:
    using Storage = HierarchicalSlotStorage<HeapOps, B>;
    using SingleOps = HierarchicalSlotSingleOps<HeapOps, B>;
    using BufferView = typename Storage::BufferView;
    using HeapRegion = typename Storage::HeapRegion;
    using Slot = typename Storage::Slot;
    using Layer = typename Storage::Layer;
    using State = typename Storage::State;

    struct AllocationPlan {
        bool found{false};
        uint32_t start_layer{Storage::kInvalidLayer};
        uint32_t start_slot{0};
    };

    explicit HierarchicalSlotDenseOps(Storage& storage, SingleOps& single_ops)
        : storage_(storage), single_ops_(single_ops) {}

    // ========================================================================
    // Dense allocation
    // ========================================================================

    BufferView allocateDense(std::size_t size) {
        std::lock_guard<std::mutex> lock(storage_.mutex());

        std::vector<uint32_t> rs = storage_.computeRequestSlots(size);

        // 高速パス：末尾から連続確保
        AllocationPlan plan = tryFindTrailPlan(rs);

        if (!plan.found) {
            // 中間探索
            plan = tryFindMiddlePlan(rs);
        }

        if (!plan.found) {
            // expand して再試行
            expandForRequest(rs);
            plan = tryFindTrailPlan(rs);
        }

        ORTEAF_THROW_IF(!plan.found, OutOfMemory, "Cannot allocate dense region");

        return executeAllocationPlan(plan, rs, size);
    }

#if ORTEAF_ENABLE_TEST
    // テスト用ラッパ
    AllocationPlan debugTryFindTrailPlan(const std::vector<uint32_t>& rs) {
        return tryFindTrailPlan(rs);
    }

    AllocationPlan debugTryFindMiddlePlan(const std::vector<uint32_t>& rs) {
        return tryFindMiddlePlan(rs);
    }
#endif

    void deallocateDense(BufferView view, std::size_t size) {
        if (!view) return;
        std::lock_guard<std::mutex> lock(storage_.mutex());

        std::vector<uint32_t> rs = storage_.computeRequestSlots(size);
        auto& layers = storage_.layers();

        // viewのアドレスから開始位置を特定
        void* base_addr = view.data();
        std::size_t offset = 0;

        for (uint32_t layer_idx = 0; layer_idx < rs.size(); ++layer_idx) {
            Layer& layer = layers[layer_idx];

            for (uint32_t i = 0; i < rs[layer_idx]; ++i) {
                void* expected_addr = static_cast<char*>(base_addr) + offset;

                // 該当スロットを探す
                for (uint32_t slot_idx = 0; slot_idx < layer.slots.size(); ++slot_idx) {
                    Slot& slot = layer.slots[slot_idx];
                    if (slot.state == State::InUse && slot.region.data() == expected_addr) {
                        single_ops_.unmapSlot(layer_idx, slot_idx);
                        single_ops_.releaseSlot(layer_idx, slot_idx);
                        single_ops_.tryMergeUpward(layer_idx, slot_idx);
                        break;
                    }
                }

                offset += layer.slot_size;
            }
        }
    }

private:
    // ========================================================================
    // Trail search (recursive)
    // ========================================================================

    AllocationPlan tryFindTrailPlan(const std::vector<uint32_t>& rs) {
        AllocationPlan plan;
        plan.found = false;

        if (rs.empty()) return plan;

        auto& layers = storage_.layers();
        if (layers.empty() || layers[0].slots.empty()) return plan;

        // levels[0]の先端からスタート
        uint32_t need = rs[0];

        bool result = tryFindTrailRecursive(rs, 0, 0, need, false, plan, static_cast<uint32_t>(layers[0].slots.size()));
        plan.found = result;

        return plan;
    }

    /**
     * @brief 再帰的に末尾から探索
     * @param rs 必要スロット数配列
     * @param layer_idx 現在のレイヤー
     * @param start_idx このレイヤーでの開始インデックス
     * @param need このレイヤーで必要なスロット数
     * @param is_found すでに見つかっているか（trueなら開始位置探索モード）
     * @param plan 結果
     * @return 成功したか
     */
    bool tryFindTrailRecursive(
        const std::vector<uint32_t>& rs,
        uint32_t layer_idx,
        uint32_t start_idx,
        uint32_t need,
        bool is_found,
        AllocationPlan& plan,
        uint32_t upper_bound  // このレイヤで進んでよい上限スロット（子範囲の上限を越えないため、exclusive）
    ) {
        auto& layers = storage_.layers();
        Layer& layer = layers[layer_idx];

        if (start_idx >= layer.slots.size() || upper_bound > layer.slots.size()) {
            return false;
        }

        if (is_found) {
            // 開始位置探索モード：できるだけ進む（先頭側を優先）
            uint32_t idx = start_idx;
            while (idx + 1 < upper_bound && layer.slots[static_cast<size_t>(idx + 1)].state == State::Free) {
                ++idx;
            }

            // 隣接するSplitがあれば子に潜る
            if (idx + 1 < upper_bound && layer.slots[static_cast<size_t>(idx + 1)].state == State::Split) {
                Slot& split_slot = layer.slots[static_cast<size_t>(idx + 1)];
                if (layer_idx + 1 < layers.size()) {
                    uint32_t sibling_count = static_cast<uint32_t>(layer.slot_size / layers[layer_idx + 1].slot_size);
                    if (layer_idx + 1 >= rs.size()) return false;
                    uint32_t child_begin = split_slot.child_begin;
                    return tryFindTrailRecursive(rs, layer_idx + 1, child_begin, rs[layer_idx + 1], true, plan, child_begin + sibling_count);
                }
            } else if (idx + 1 < upper_bound) {
                // ここが開始位置
                plan.start_layer = layer_idx;
                plan.start_slot = idx;
                return true;
            }

        } else {
            // 確認モード：needを満たせるか確認
            uint32_t count = 0;
            uint32_t idx = start_idx;

            while (idx + 1 < upper_bound && layer.slots[static_cast<size_t>(idx + 1)].state == State::Free) {
                ++count;
                ++idx;
            }

            if (count == need) {
                // 十分なFreeあり。隣接Splitがあれば子に潜る
                if (idx + 1 < upper_bound && layer.slots[static_cast<size_t>(idx + 1)].state == State::Split) {
                    if (layer_idx + 1 < layers.size()) {
                        Slot& split_slot = layer.slots[static_cast<size_t>(idx + 1)];
                        uint32_t sibling_count = static_cast<uint32_t>(layer.slot_size / layers[layer_idx + 1].slot_size);
                        if (layer_idx + 1 >= rs.size()) return false;
                        if (sibling_count < rs[layer_idx + 1]) return false;
                        uint32_t child_begin = split_slot.child_begin;
                        return tryFindTrailRecursive(rs, layer_idx + 1, child_begin, rs[layer_idx + 1], false, plan, child_begin + sibling_count);
                    }
                } else if (idx + 1 < upper_bound) {
                    for (std::size_t req_idx = layer_idx; req_idx < rs.size(); ++req_idx) {
                        if (rs[req_idx] != 0) {
                            return false;
                        }
                    }
                    return true;
                }
            } else if (count > need) {
                if (idx + 1 < upper_bound && layer.slots[static_cast<size_t>(idx + 1)].state == State::Split) {
                    if (layer_idx + 1 < layers.size()) {
                        Slot& split_slot = layer.slots[static_cast<size_t>(idx + 1)];
                        uint32_t sibling_count = static_cast<uint32_t>(layer.slot_size / layers[layer_idx + 1].slot_size);
                        if (layer_idx + 1 >= rs.size()) return false;
                        uint32_t child_begin = split_slot.child_begin;
                        return tryFindTrailRecursive(rs, layer_idx + 1, child_begin, rs[layer_idx + 1], true, plan, child_begin + sibling_count);
                    }
                } else if (idx + 1 < upper_bound) {
                    plan.start_layer = layer_idx;
                    plan.start_slot = idx;
                    return true;
                }
            }
            // 足りない
            return false;
        }
    }

    // ========================================================================
    // Middle search
    // ========================================================================

    AllocationPlan tryFindMiddlePlan(const std::vector<uint32_t>& rs) {
        AllocationPlan plan;
        plan.found = false;

        auto& layers = storage_.layers();
        if (layers.empty() || rs.empty()) return plan;

        Layer& root = layers[0];
        uint32_t need = rs[0];

        // 連続Free区間を探す
        uint32_t consecutive_start = 0;
        uint32_t consecutive_count = 0;

        for (uint32_t i = 0; i < root.slots.size(); ++i) {
            if (root.slots[i].state == State::Free) {
                if (consecutive_count == 0) {
                    consecutive_start = i;
                }
                ++consecutive_count;

                if (consecutive_count >= need) {
                    // 十分な連続領域発見
                    plan.found = true;
                    plan.start_layer = 0;
                    plan.start_slot = consecutive_start;
                    return plan;
                }
            } else {
                consecutive_count = 0;
            }
        }

        return plan;
    }

    // ========================================================================
    // Execution
    // ========================================================================

    void expandForRequest(const std::vector<uint32_t>& rs) {
        const auto& levels = storage_.config().levels;
        std::size_t total_needed = 0;
        for (uint32_t i = 0; i < rs.size(); ++i) {
            total_needed += rs[i] * levels[i];
        }

        // levels[0]の倍数に切り上げ
        std::size_t expand = ((total_needed + levels[0] - 1) / levels[0]) * levels[0];
        storage_.addRegion(expand);
    }

    BufferView executeAllocationPlan(const AllocationPlan& plan, const std::vector<uint32_t>& rs, std::size_t size) {
        auto& layers = storage_.layers();
        if (plan.start_layer >= layers.size()) {
            ORTEAF_THROW(OutOfMemory, "Invalid allocation plan");
        }

        void* base_addr = nullptr;
        std::size_t total_allocated = 0;

        // start_layer から順に決定的に確保する
        uint32_t layer_idx = plan.start_layer;
        uint32_t slot_idx = plan.start_slot;

        // start_layer の連続スロットを確保
        for (uint32_t i = 0; i < rs[layer_idx]; ++i) {
            if (slot_idx + i >= layers[layer_idx].slots.size()) {
                ORTEAF_THROW(OutOfMemory, "Plan exceeds layer slots");
            }
            single_ops_.acquireSpecificSlot(layer_idx, slot_idx + i);
            BufferView view = single_ops_.mapSlot(layer_idx, slot_idx + i);
            if (base_addr == nullptr) {
                base_addr = view.data();
            }
            total_allocated += layers[layer_idx].slot_size;
        }

        // 下位レイヤに降りる
        for (uint32_t l = layer_idx + 1; l < rs.size(); ++l) {
            if (rs[l] == 0) continue;
            // 親は plan.start_slot から連続している前提で、その子ブロックを連続取得
            Slot& parent = layers[l - 1].slots[slot_idx];
            uint32_t children = static_cast<uint32_t>(layers[l - 1].slot_size / layers[l].slot_size);
            uint32_t child_start = parent.child_begin;

            if (child_start + rs[l] > child_start + children) {
                ORTEAF_THROW(OutOfMemory, "Child plan exceeds layer slots");
            }

            for (uint32_t i = 0; i < rs[l]; ++i) {
                uint32_t child_idx = child_start + i;
                single_ops_.acquireSpecificSlot(l, child_idx);
                BufferView view = single_ops_.mapSlot(l, child_idx);
                if (base_addr == nullptr) {
                    base_addr = view.data();
                }
                total_allocated += layers[l].slot_size;
            }
        }

        return BufferView{base_addr, 0, size};
    }

    Storage& storage_;
    SingleOps& single_ops_;
};

}  // namespace orteaf::internal::runtime::allocator::policies

#pragma once

#include "orteaf/internal/execution/allocator/lowlevel/hierarchical_slot_single_ops.h"

#include <optional>

namespace orteaf::internal::execution::allocator::policies {

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

    enum class Direction { Forward, Backward };

    struct AllocationPlan {
        bool found{false};
        uint32_t end_layer{Storage::kInvalidLayer};
        uint32_t end_slot{0};
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

    static int step(Direction dir) noexcept { return dir == Direction::Forward ? 1 : -1; }
    static bool inBounds(int32_t idx, int32_t lower, int32_t upper, Direction dir) noexcept {
        return dir == Direction::Forward ? idx < upper : idx >= lower;
    }

    // 新ロジック（方向指定版）。旧実装との切り替え用に別名で持つ。
    bool tryFindTrailRecursiveDir(
        const std::vector<uint32_t>& rs,
        uint32_t layer_idx,
        uint32_t start_idx,
        uint32_t need,
        bool is_found,
        AllocationPlan& plan,
        Direction dir,
        uint32_t lower_bound,
        uint32_t upper_bound
    ) {
        auto& layers = storage_.layers();
        Layer& layer = layers[layer_idx];

        (void)is_found;  // モードは旧実装互換のため残すが新ロジックでは未使用

        if (start_idx >= layer.slots.size() || lower_bound >= upper_bound || upper_bound > layer.slots.size()) {
            return false;
        }

        auto finalize_true = [&](uint32_t layer_no, uint32_t slot_no) {
            plan.end_layer = layer_no;
            plan.end_slot = slot_no;
            return true;
        };

        auto finalize_false = [&](uint32_t layer_no, uint32_t slot_no) {
            plan.end_layer = layer_no;
            plan.end_slot = slot_no;
            return false;
        };

        const bool has_child_request = [&]() {
            for (std::size_t i = layer_idx + 1; i < rs.size(); ++i) {
                if (rs[i] != 0) return true;
            }
            return false;
        }();

        auto descend_to_child = [&](uint32_t slot_index, bool check_siblings) -> bool {
            Slot& split_slot = layer.slots[slot_index];
            if (layer_idx + 1 >= layers.size() || layer_idx + 1 >= rs.size()) return false;
            uint32_t sibling_count = static_cast<uint32_t>(layer.slot_size / layers[layer_idx + 1].slot_size);
            if (check_siblings && sibling_count < rs[layer_idx + 1]) return false;
            uint32_t child_begin = split_slot.child_begin;
            uint32_t child_upper = child_begin + sibling_count;
            return tryFindTrailRecursiveDir(
                rs,
                layer_idx + 1,
                dir == Direction::Forward ? child_begin : child_upper - 1,
                rs[layer_idx + 1],
                false,
                plan,
                dir,
                child_begin,
                child_upper);
        };

        int32_t lower = static_cast<int32_t>(lower_bound);
        int32_t upper = static_cast<int32_t>(upper_bound);

        int32_t idx = static_cast<int32_t>(start_idx);

        if (!inBounds(idx, lower, upper, dir)) return false;

        // 連続Freeの長さを測る（start_idx を含む run のみ見る）
        uint32_t free_count = 0;
        int32_t run_start = idx;
        int32_t run_end = idx;
        for (run_end = idx;
             inBounds(run_end, lower, upper, dir) &&
             layer.slots[static_cast<size_t>(run_end)].state == State::Free;
             run_end += step(dir)) {
            ++free_count;
        }

        int32_t boundary = run_end;

        const uint32_t end_slot = (dir == Direction::Forward)
            ? static_cast<uint32_t>(run_end - 1)
            : static_cast<uint32_t>(run_start);

        if (free_count < need) return finalize_false(layer_idx, end_slot);

        const bool boundary_is_split = inBounds(boundary, lower, upper, dir) &&
            layer.slots[static_cast<size_t>(boundary)].state == State::Split;

        if (dir == Direction::Forward) {
            if (free_count > need) {
                if (boundary_is_split) {
                    descend_to_child(static_cast<uint32_t>(boundary), false);
                    return true;
                }

                return finalize_true(layer_idx, end_slot);
            }

            if (free_count == need) {
                if (!has_child_request) {
                    if (boundary_is_split) {
                        descend_to_child(static_cast<uint32_t>(boundary), true);
                        return true;
                    }

                    return finalize_true(layer_idx, end_slot);
                }

                if (boundary_is_split) {
                    bool child_ok = descend_to_child(static_cast<uint32_t>(boundary), true);
                    return child_ok;
                }

                return finalize_false(layer_idx, end_slot);
            }

            return finalize_false(layer_idx, end_slot);
        }

        // Backward
        if (free_count > need) {
            return finalize_true(layer_idx, end_slot);
        }

        if (free_count == need) {
            if (!has_child_request) {
                return finalize_true(layer_idx, end_slot);
            }

            if (boundary_is_split) {
                return descend_to_child(static_cast<uint32_t>(boundary), true);
            }

            return false;
        }

        return false;
    }

    AllocationPlan tryFindTrailPlan(const std::vector<uint32_t>& rs) {
        AllocationPlan plan;
        plan.found = false;

        if (rs.empty()) return plan;

        auto& layers = storage_.layers();
        if (layers.empty() || layers[0].slots.empty()) return plan;

        // levels[0]を先頭側から走査し、末尾に詰める
        uint32_t need = rs[0];

        bool result = tryFindTrailRecursiveDir(
            rs,
            0,
            0,
            need,
            false,
            plan,
            Direction::Forward,
            0,
            static_cast<uint32_t>(layers[0].slots.size()));
        plan.found = result;

        return plan;
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
                    plan.end_layer = 0;
                    plan.end_slot = consecutive_start;
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
    }

    Storage& storage_;
    SingleOps& single_ops_;
};

}  // namespace orteaf::internal::execution::allocator::policies

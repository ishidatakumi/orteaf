#pragma once

#include "orteaf/internal/execution/allocator/lowlevel/hierarchical_slot_dense_ops.h"
#include "orteaf/internal/execution/allocator/lowlevel/hierarchical_slot_single_ops.h"
#include <memory>
#include "orteaf/internal/execution/allocator/lowlevel/hierarchical_slot_storage.h"

namespace orteaf::internal::execution::allocator::policies {

/**
 * @brief 階層的スロットアロケータ（統合ファサード）
 *
 * Storage, SingleOps, DenseOpsを統合し、
 * 従来のAPIを提供する。
 */
template <class HeapOps, ::orteaf::internal::backend::Backend B>
class HierarchicalSlotAllocator {
public:
    using Storage = HierarchicalSlotStorage<HeapOps, B>;
    using SingleOps = HierarchicalSlotSingleOps<HeapOps, B>;
    using DenseOps = HierarchicalSlotDenseOps<HeapOps, B>;
    using BufferView = typename Storage::BufferView;
    using Config = typename Storage::Config;

    void initialize(const Config& config, HeapOps* heap_ops) {
        storage_.initialize(config, heap_ops);
        single_ops_ = std::make_unique<SingleOps>(storage_);
        dense_ops_ = std::make_unique<DenseOps>(storage_, *single_ops_);
    }

    // Single slot API
    BufferView allocate(std::size_t size) {
        return single_ops_->allocate(size);
    }

    void deallocate(BufferView view) {
        single_ops_->deallocate(view);
    }

    // Dense API
    BufferView allocateDense(std::size_t size) {
        return dense_ops_->allocateDense(size);
    }

    void deallocateDense(BufferView view, std::size_t size) {
        dense_ops_->deallocateDense(view, size);
    }

    // Utility
    [[nodiscard]] std::vector<uint32_t> computeRequestSlots(std::size_t size) const {
        return storage_.computeRequestSlots(size);
    }

    // Access to internals (for testing)
    [[nodiscard]] Storage& storage() noexcept { return storage_; }
    [[nodiscard]] const Storage& storage() const noexcept { return storage_; }

#if ORTEAF_ENABLE_TEST
    // テスト専用アクセス
    [[nodiscard]] typename DenseOps::AllocationPlan debugTryFindTrailPlan(const std::vector<uint32_t>& rs) {
        return dense_ops_->debugTryFindTrailPlan(rs);
    }
    [[nodiscard]] typename DenseOps::AllocationPlan debugTryFindMiddlePlan(const std::vector<uint32_t>& rs) {
        return dense_ops_->debugTryFindMiddlePlan(rs);
    }

    [[nodiscard]] auto debugSnapshot() const {
        return storage_.debugSnapshot();
    }
#endif

private:
    Storage storage_;
    std::unique_ptr<SingleOps> single_ops_;
    std::unique_ptr<DenseOps> dense_ops_;
};

}  // namespace orteaf::internal::execution::allocator::policies

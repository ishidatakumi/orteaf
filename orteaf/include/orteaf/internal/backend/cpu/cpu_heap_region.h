#pragma once

#include <cstddef>

namespace orteaf::internal::backend::cpu {

// Chunk-level view for CPU backend. Represents a reserved region (pointer +
// size) before it is turned into a BufferView for user-facing code.
struct CpuHeapRegion {
    CpuHeapRegion() = default;
    CpuHeapRegion(void* base, std::size_t size_bytes) : base_(base), size_(size_bytes) {}

    bool empty() const { return base_ == nullptr || size_ == 0; }
    explicit operator bool() const { return !empty(); }

    void* data() const { return base_; }
    std::size_t size() const { return size_; }

    bool contains(const CpuHeapRegion& other) const {
        if (base_ != other.base_) return false;
        return other.size_ <= size_;
    }

    friend bool operator==(const CpuHeapRegion& lhs, const CpuHeapRegion& rhs) {
        return lhs.base_ == rhs.base_ && lhs.size_ == rhs.size_;
    }

private:
    void* base_{nullptr};
    std::size_t size_{0};
};

}  // namespace orteaf::internal::backend::cpu

#pragma once

#include <cstddef>

#if ORTEAF_ENABLE_MPS
#include <orteaf/internal/execution/mps/platform/wrapper/mps_heap.h>

namespace orteaf::internal::execution::mps::resource {

// Represents a reserved span within the active MPS heap (offset + size).
// Non-owning; the heap itself is tracked elsewhere by the resource.
struct MpsHeapRegion {
    MpsHeapRegion() = default;
    MpsHeapRegion(std::size_t offset_bytes, std::size_t size_bytes)
        : offset_(offset_bytes), size_(size_bytes) {}

    bool empty() const { return size_ == 0; }
    explicit operator bool() const { return !empty(); }

    std::size_t offset() const { return offset_; }
    std::size_t size() const { return size_; }

    bool contains(const MpsHeapRegion& other) const {
        const std::size_t begin = offset_;
        const std::size_t end = offset_ + size_;
        const std::size_t other_begin = other.offset_;
        const std::size_t other_end = other_begin + other.size_;
        return begin <= other_begin && other_end <= end;
    }

    friend bool operator==(const MpsHeapRegion& lhs, const MpsHeapRegion& rhs) {
        return lhs.offset_ == rhs.offset_ && lhs.size_ == rhs.size_;
    }

private:
    std::size_t offset_{0};
    std::size_t size_{0};
};

}  // namespace orteaf::internal::execution::mps::resource

#endif  // ORTEAF_ENABLE_MPS

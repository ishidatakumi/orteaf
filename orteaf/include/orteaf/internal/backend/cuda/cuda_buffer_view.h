#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#if ORTEAF_ENABLE_CUDA
#include <orteaf/internal/backend/cuda/wrapper/cuda_stream.h>

namespace orteaf::internal::backend::cuda {

// Lightweight, non-owning CUDA buffer view (device pointer + offset/size).
class CudaBufferView {
public:
    using pointer = CUdeviceptr_t;

    CudaBufferView() = default;
    CudaBufferView(pointer base, std::size_t offset_bytes, std::size_t size_bytes)
        : base_(base), offset_(offset_bytes), size_(size_bytes) {}

    bool empty() const { return base_ == 0; }
    explicit operator bool() const { return !empty(); }

    pointer data() const { return base_ + offset_; }
    std::size_t offset() const { return offset_; }
    std::size_t size() const { return size_; }

    bool contains(const CudaBufferView& other, std::size_t span) const {
        if (base_ != other.base_) return false;
        const std::size_t begin = offset_;
        const std::size_t end = offset_ + size_;
        const std::size_t other_begin = other.offset_;
        const std::size_t other_end = other_begin + span;
        return begin <= other_begin && other_end <= end;
    }

    friend bool operator==(const CudaBufferView& lhs, const CudaBufferView& rhs) {
        return lhs.base_ == rhs.base_ && lhs.offset_ == rhs.offset_;
    }
    friend bool operator<(const CudaBufferView& lhs, const CudaBufferView& rhs) {
        if (lhs.base_ == rhs.base_) return lhs.offset_ < rhs.offset_;
        return lhs.base_ < rhs.base_;
    }

    struct Hash {
        std::size_t operator()(const CudaBufferView& h) const {
            return std::hash<pointer>{}(h.base_) ^ (h.offset_ + 0x9e3779b97f4a7c15ULL);
        }
    };

private:
    pointer base_{0};
    std::size_t offset_{0};
    std::size_t size_{0};
};

}  // namespace orteaf::internal::backend::cuda
#endif  // ORTEAF_ENABLE_CUDA

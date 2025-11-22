#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#if ORTEAF_ENABLE_MPS
#include <orteaf/internal/backend/mps/wrapper/mps_buffer.h>

namespace orteaf::internal::backend::mps {

// Lightweight, non-owning MPS buffer view (MTLBuffer + offset/size).
class MpsBufferView {
public:
    MpsBufferView() = default;
    MpsBufferView(MPSBuffer_t buffer, std::size_t offset_bytes, std::size_t size_bytes)
        : buffer_(buffer), offset_(offset_bytes), size_(size_bytes) {}

    bool empty() const { return buffer_ == nullptr; }
    explicit operator bool() const { return !empty(); }

    MPSBuffer_t raw() const { return buffer_; }
    std::size_t offset() const { return offset_; }
    std::size_t size() const { return size_; }

    bool contains(const MpsBufferView& other, std::size_t span) const {
        if (buffer_ != other.buffer_) return false;
        const std::size_t begin = offset_;
        const std::size_t end = offset_ + size_;
        const std::size_t other_begin = other.offset_;
        const std::size_t other_end = other_begin + span;
        return begin <= other_begin && other_end <= end;
    }

    friend bool operator==(const MpsBufferView& lhs, const MpsBufferView& rhs) {
        return lhs.buffer_ == rhs.buffer_ && lhs.offset_ == rhs.offset_;
    }
    friend bool operator<(const MpsBufferView& lhs, const MpsBufferView& rhs) {
        if (lhs.buffer_ == rhs.buffer_) return lhs.offset_ < rhs.offset_;
        return lhs.buffer_ < rhs.buffer_;
    }

    struct Hash {
        std::size_t operator()(const MpsBufferView& h) const {
            return std::hash<void*>{}(h.buffer_) ^ (h.offset_ + 0x9e3779b97f4a7c15ULL);
        }
    };

private:
    MPSBuffer_t buffer_{nullptr};
    std::size_t offset_{0};
    std::size_t size_{0};
};

}  // namespace orteaf::internal::backend::mps
#endif  // ORTEAF_ENABLE_MPS

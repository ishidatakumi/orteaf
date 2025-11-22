#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace orteaf::internal::backend::cpu {

// Lightweight, non-owning CPU buffer view (pointer + offset/size).
class CpuBufferView {
public:
    CpuBufferView() = default;
    CpuBufferView(void* base, std::size_t offset_bytes, std::size_t size_bytes)
        : base_(base), offset_(offset_bytes), size_(size_bytes) {}

    bool empty() const { return base_ == nullptr; }
    explicit operator bool() const { return !empty(); }

    void* data() const { return static_cast<char*>(base_) + offset_; }
    std::size_t offset() const { return offset_; }
    std::size_t size() const { return size_; }

    bool contains(const CpuBufferView& other, std::size_t span) const {
        if (base_ != other.base_) return false;
        const std::size_t begin = offset_;
        const std::size_t end = offset_ + size_;
        const std::size_t other_begin = other.offset_;
        const std::size_t other_end = other_begin + span;
        return begin <= other_begin && other_end <= end;
    }

    friend bool operator==(const CpuBufferView& lhs, const CpuBufferView& rhs) {
        return lhs.base_ == rhs.base_ && lhs.offset_ == rhs.offset_;
    }
    friend bool operator<(const CpuBufferView& lhs, const CpuBufferView& rhs) {
        if (lhs.base_ == rhs.base_) return lhs.offset_ < rhs.offset_;
        return lhs.base_ < rhs.base_;
    }

    struct Hash {
        std::size_t operator()(const CpuBufferView& h) const {
            return std::hash<void*>{}(h.base_) ^ (h.offset_ + 0x9e3779b97f4a7c15ULL);
        }
    };

private:
    void* base_{nullptr};
    std::size_t offset_{0};
    std::size_t size_{0};
};

}  // namespace orteaf::internal::backend::cpu

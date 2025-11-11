#pragma once

#include <cstddef>
#include <new>
#include <type_traits>

namespace orteaf::internal::base {

template <typename T>
class HeapVector {
public:
    HeapVector() noexcept : data_(nullptr), size_(0), capacity_(0) {}

    HeapVector(const HeapVector& other) requires std::is_copy_constructible_v<T>
        : data_(nullptr), size_(0), capacity_(0) {
        if (other.size_ == 0) return;
        allocate(other.size_);
        std::size_t i = 0;
        try {
            for (; i < other.size_; ++i) {
                new (data_ + i) T(other.data_[i]);
            }
        } catch (...) {
            destroyRange(0, i);
            ::operator delete(data_);
            data_ = nullptr;
            capacity_ = 0;
            throw;
        }
        size_ = other.size_;
    }

    HeapVector(const HeapVector& other) requires (!std::is_copy_constructible_v<T>) = delete;

    HeapVector(HeapVector&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    HeapVector& operator=(const HeapVector& other) requires std::is_copy_constructible_v<T> {
        if (this == &other) return *this;
        if (other.size_ == 0) {
            clear();
            deallocate();
            return *this;
        }
        if (capacity_ < other.size_) {
            T* new_data = static_cast<T*>(::operator new(sizeof(T) * other.size_));
            std::size_t constructed = 0;
            try {
                for (; constructed < other.size_; ++constructed) {
                    new (new_data + constructed) T(other.data_[constructed]);
                }
            } catch (...) {
                for (std::size_t i = 0; i < constructed; ++i) {
                    new_data[i].~T();
                }
                ::operator delete(new_data);
                throw;
            }
            destroyElements();
            ::operator delete(data_);
            data_ = new_data;
            capacity_ = other.size_;
            size_ = other.size_;
        } else {
            std::size_t i = 0;
            for (; i < size_ && i < other.size_; ++i) {
                data_[i] = other.data_[i];
            }
            for (; i < other.size_; ++i) {
                new (data_ + i) T(other.data_[i]);
            }
            for (; i < size_; ++i) {
                data_[i].~T();
            }
            size_ = other.size_;
        }
        return *this;
    }

    HeapVector& operator=(const HeapVector& other) requires (!std::is_copy_constructible_v<T>) = delete;

    HeapVector& operator=(HeapVector&& other) noexcept {
        if (this == &other) return *this;
        destroyElements();
        ::operator delete(data_);
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
        return *this;
    }

    ~HeapVector() {
        destroyElements();
        ::operator delete(data_);
    }

    void pushBack(const T& value) requires std::is_copy_constructible_v<T> { emplaceBack(value); }
    void pushBack(const T& value) requires (!std::is_copy_constructible_v<T>) = delete;
    void pushBack(T&& value) { emplaceBack(static_cast<T&&>(value)); }

    template <typename... Args>
    T& emplaceBack(Args&&... args) {
        ensureCapacityFor(size_ + 1);
        new (data_ + size_) T(static_cast<Args&&>(args)...);
        ++size_;
        return back();
    }

    void reserve(std::size_t new_capacity) {
        if (new_capacity <= capacity_) return;
        reallocate(new_capacity);
    }

    void resize(std::size_t new_size) {
        if (new_size < size_) {
            for (std::size_t i = new_size; i < size_; ++i) {
                data_[i].~T();
            }
            size_ = new_size;
            return;
        }
        ensureCapacityFor(new_size);
        std::size_t i = size_;
        try {
            for (; i < new_size; ++i) {
                new (data_ + i) T();
            }
        } catch (...) {
            for (std::size_t j = size_; j < i; ++j) {
                data_[j].~T();
            }
            throw;
        }
        size_ = new_size;
    }

    void resize(std::size_t new_size, const T& value) requires std::is_copy_constructible_v<T> {
        if (new_size < size_) {
            for (std::size_t i = new_size; i < size_; ++i) {
                data_[i].~T();
            }
            size_ = new_size;
            return;
        }
        ensureCapacityFor(new_size);
        for (std::size_t i = size_; i < new_size; ++i) {
            new (data_ + i) T(value);
        }
        size_ = new_size;
    }
    void resize(std::size_t, const T&) requires (!std::is_copy_constructible_v<T>) = delete;

    void clear() {
        destroyElements();
        size_ = 0;
    }

    void shrinkToFit() {
        if (size_ == capacity_) return;
        if (size_ == 0) {
            destroyElements();
            ::operator delete(data_);
            data_ = nullptr;
            capacity_ = 0;
            return;
        }
        reallocate(size_);
    }

    T* data() noexcept { return data_; }
    const T* data() const noexcept { return data_; }

    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return capacity_; }
    bool empty() const noexcept { return size_ == 0; }

    T& operator[](std::size_t idx) noexcept { return data_[idx]; }
    const T& operator[](std::size_t idx) const noexcept { return data_[idx]; }

    T& front() noexcept { return data_[0]; }
    const T& front() const noexcept { return data_[0]; }
    T& back() noexcept { return data_[size_ - 1]; }
    const T& back() const noexcept { return data_[size_ - 1]; }

private:
    T* data_;
    std::size_t size_;
    std::size_t capacity_;

    void destroyElements() {
        for (std::size_t i = 0; i < size_; ++i) {
            data_[i].~T();
        }
    }

    void ensureCapacityFor(std::size_t required) {
        if (required <= capacity_) return;
        std::size_t new_cap = capacity_ == 0 ? std::size_t(1) : capacity_;
        while (new_cap < required) new_cap *= 2;
        reallocate(new_cap);
    }

    void allocate(std::size_t new_capacity) {
        data_ = static_cast<T*>(::operator new(sizeof(T) * new_capacity));
        capacity_ = new_capacity;
    }

    void reallocate(std::size_t new_capacity) {
        T* new_data = static_cast<T*>(::operator new(sizeof(T) * new_capacity));
        std::size_t i = 0;
        try {
            for (; i < size_; ++i) {
                new (new_data + i) T(static_cast<T&&>(data_[i]));
            }
        } catch (...) {
            for (std::size_t j = 0; j < i; ++j) {
                new_data[j].~T();
            }
            ::operator delete(new_data);
            throw;
        }
        destroyElements();
        ::operator delete(data_);
        data_ = new_data;
        capacity_ = new_capacity;
    }

    void destroyRange(std::size_t begin, std::size_t end) {
        for (std::size_t i = begin; i < end; ++i) {
            data_[i].~T();
        }
    }

    void deallocate() {
        ::operator delete(data_);
        data_ = nullptr;
        capacity_ = 0;
    }
};

}  // namespace orteaf::internal::base

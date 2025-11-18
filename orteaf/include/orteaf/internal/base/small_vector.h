#pragma once

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

namespace orteaf::internal::base {

// SmallVector keeps up to N elements inline before falling back to heap storage.
template <typename T, std::size_t N>
class SmallVector {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = pointer;
    using const_iterator = const_pointer;

    SmallVector() noexcept {
        resetInlinePointer();
    }

    SmallVector(size_type count, const T& value) requires std::is_copy_constructible_v<T> : SmallVector() {
        resize(count, value);
    }

    SmallVector(size_type count, const T& value) requires (!std::is_copy_constructible_v<T>) = delete;

    explicit SmallVector(size_type count) : SmallVector() {
        resize(count);
    }

    SmallVector(std::initializer_list<T> init) requires std::is_copy_constructible_v<T> : SmallVector() {
        assign(init.begin(), init.end());
    }

    SmallVector(std::initializer_list<T> init) requires (!std::is_copy_constructible_v<T>) = delete;

    SmallVector(const SmallVector& other) requires std::is_copy_constructible_v<T> : SmallVector() {
        assign(other.begin(), other.end());
    }

    SmallVector(const SmallVector& other) requires (!std::is_copy_constructible_v<T>) = delete;

    SmallVector(SmallVector&& other) noexcept(std::is_nothrow_move_constructible_v<T>) : SmallVector() {
        moveFrom(other);
    }

    ~SmallVector() {
        clear();
        if (usingHeapStorage()) {
            deallocate(data_);
        }
    }

    SmallVector& operator=(const SmallVector& other) requires std::is_copy_constructible_v<T> {
        if (this != &other) {
            assign(other.begin(), other.end());
        }
        return *this;
    }

    SmallVector& operator=(const SmallVector& other) requires (!std::is_copy_constructible_v<T>) = delete;

    SmallVector& operator=(SmallVector&& other) noexcept(std::is_nothrow_move_assignable_v<T>) {
        if (this != &other) {
            clear();
            releaseHeapStorage();
            moveFrom(other);
        }
        return *this;
    }

    SmallVector& operator=(std::initializer_list<T> init) requires std::is_copy_constructible_v<T> {
        assign(init.begin(), init.end());
        return *this;
    }

    SmallVector& operator=(std::initializer_list<T> init) requires (!std::is_copy_constructible_v<T>) = delete;

    void assign(size_type count, const T& value) requires std::is_copy_constructible_v<T> {
        clear();
        if constexpr (has_inline_storage) {
            if (usingHeapStorage() && count <= stack_capacity_value) {
                releaseHeapStorage();
            }
        }
        ensureCapacity(count);
        size_type i = 0;
        try {
            for (; i < count; ++i) {
                ::new (static_cast<void*>(data_ + i)) T(value);
            }
        } catch (...) {
            destroyRange(data_, data_ + i);
            throw;
        }
        size_ = count;
    }

    void assign(size_type count, const T& value) requires (!std::is_copy_constructible_v<T>) = delete;

    template <typename InputIt>
    void assign(InputIt first, InputIt last) requires std::is_copy_constructible_v<T> {
        clear();
        using category = typename std::iterator_traits<InputIt>::iterator_category;
        if constexpr (std::is_base_of_v<std::forward_iterator_tag, category>) {
            const size_type count = static_cast<size_type>(std::distance(first, last));
            if constexpr (has_inline_storage) {
                if (usingHeapStorage() && count <= stack_capacity_value) {
                    releaseHeapStorage();
                }
            }
            ensureCapacity(count);
            size_type i = 0;
            try {
                for (; first != last; ++first, ++i) {
                    ::new (static_cast<void*>(data_ + i)) T(*first);
                }
            } catch (...) {
                destroyRange(data_, data_ + i);
                throw;
            }
            size_ = count;
        } else {
            if constexpr (has_inline_storage) {
                if (usingHeapStorage()) {
                    releaseHeapStorage();
                }
            }
            for (; first != last; ++first) {
                emplaceBack(*first);
            }
        }
    }

    template <typename InputIt>
    void assign(InputIt, InputIt) requires (!std::is_copy_constructible_v<T>) = delete;

    void assign(std::initializer_list<T> init) requires std::is_copy_constructible_v<T> {
        assign(init.begin(), init.end());
    }

    void assign(std::initializer_list<T>) requires (!std::is_copy_constructible_v<T>) = delete;

    reference operator[](size_type index) noexcept {
        return data_[index];
    }

    const_reference operator[](size_type index) const noexcept {
        return data_[index];
    }

    pointer data() noexcept {
        return data_;
    }

    const_pointer data() const noexcept {
        return data_;
    }

    iterator begin() noexcept {
        return data_;
    }

    const_iterator begin() const noexcept {
        return data_;
    }

    const_iterator cbegin() const noexcept {
        return data_;
    }

    iterator end() noexcept {
        return data_ + size_;
    }

    const_iterator end() const noexcept {
        return data_ + size_;
    }

    const_iterator cend() const noexcept {
        return data_ + size_;
    }

    bool empty() const noexcept {
        return size_ == 0;
    }

    size_type size() const noexcept {
        return size_;
    }

    size_type capacity() const noexcept {
        return capacity_;
    }

    size_type max_size() const noexcept {
        return std::numeric_limits<size_type>::max();
    }

    void reserve(size_type new_capacity) {
        if (new_capacity > capacity_) {
            reallocate(new_capacity);
        }
    }

    void resize(size_type count) {
        if (count < size_) {
            destroyRange(data_ + count, data_ + size_);
            size_ = count;
        } else if (count > size_) {
            if constexpr (has_inline_storage) {
                if (usingHeapStorage() && count <= stack_capacity_value) {
                    releaseHeapStorage();
                }
            }
            ensureCapacity(count);
            size_type i = size_;
            try {
                for (; i < count; ++i) {
                    ::new (static_cast<void*>(data_ + i)) T();
                }
            } catch (...) {
                destroyRange(data_ + size_, data_ + i);
                throw;
            }
            size_ = count;
        }
    }

    void resize(size_type count, const T& value) requires std::is_copy_constructible_v<T> {
        if (count < size_) {
            destroyRange(data_ + count, data_ + size_);
            size_ = count;
        } else if (count > size_) {
            if constexpr (has_inline_storage) {
                if (usingHeapStorage() && count <= stack_capacity_value) {
                    releaseHeapStorage();
                }
            }
            ensureCapacity(count);
            size_type i = size_;
            try {
                for (; i < count; ++i) {
                    ::new (static_cast<void*>(data_ + i)) T(value);
                }
            } catch (...) {
                destroyRange(data_ + size_, data_ + i);
                throw;
            }
            size_ = count;
        }
    }

    void resize(size_type count, const T& value) requires (!std::is_copy_constructible_v<T>) = delete;

    void clear() noexcept {
        destroyRange(data_, data_ + size_);
        size_ = 0;
    }

    template <typename... Args>
    reference emplaceBack(Args&&... args) {
        if (size_ == capacity_) {
            ensureCapacity(size_ + 1);
        }
        ::new (static_cast<void*>(data_ + size_)) T(std::forward<Args>(args)...);
        ++size_;
        return back();
    }

    void pushBack(const T& value) requires std::is_copy_constructible_v<T> {
        emplaceBack(value);
    }

    void pushBack(const T& value) requires (!std::is_copy_constructible_v<T>) = delete;

    void pushBack(T&& value) {
        emplaceBack(std::move(value));
    }

    void popBack() {
        if (size_ > 0) {
            --size_;
            data_[size_].~T();
        }
    }

    reference front() noexcept {
        return data_[0];
    }

    const_reference front() const noexcept {
        return data_[0];
    }

    reference back() noexcept {
        return data_[size_ - 1];
    }

    const_reference back() const noexcept {
        return data_[size_ - 1];
    }

    void swap(SmallVector& other) noexcept(
        std::is_nothrow_swappable_v<T>&&
        std::is_nothrow_move_constructible_v<T>&&
        std::is_nothrow_move_assignable_v<T>) {
        if (this == &other) {
            return;
        }

        if (usingHeapStorage() && other.usingHeapStorage()) {
            std::swap(data_, other.data_);
            std::swap(size_, other.size_);
            std::swap(capacity_, other.capacity_);
            return;
        }

        SmallVector tmp(std::move(other));
        other = std::move(*this);
        *this = std::move(tmp);
    }

private:
    using storage_type = std::aligned_storage_t<sizeof(T), alignof(T)>;

    static constexpr size_type stack_capacity_value = N;
    static constexpr bool has_inline_storage = stack_capacity_value > 0;

    pointer inlineData() noexcept {
        if constexpr (has_inline_storage) {
            return reinterpret_cast<pointer>(stack_storage_);
        }
        return nullptr;
    }

    const_pointer inlineData() const noexcept {
        if constexpr (has_inline_storage) {
            return reinterpret_cast<const_pointer>(stack_storage_);
        }
        return nullptr;
    }

    bool usingInlineStorage() const noexcept {
        if constexpr (has_inline_storage) {
            return data_ == inlineData();
        }
        return false;
    }

    bool usingHeapStorage() const noexcept {
        return data_ != nullptr && !usingInlineStorage();
    }

    void resetInlinePointer() noexcept {
        if constexpr (has_inline_storage) {
            data_ = inlineData();
            capacity_ = stack_capacity_value;
        } else {
            data_ = nullptr;
            capacity_ = 0;
        }
    }

    void resetToStack() noexcept {
        resetInlinePointer();
        size_ = 0;
    }

    void moveFrom(SmallVector& other) {
        if (other.usingHeapStorage()) {
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.resetToStack();
        } else {
            resetInlinePointer();
            size_ = 0;
            size_type i = 0;
            try {
                for (; i < other.size_; ++i) {
                    ::new (static_cast<void*>(data_ + i)) T(std::move_if_noexcept(other.data_[i]));
                }
            } catch (...) {
                destroyRange(data_, data_ + i);
                throw;
            }
            size_ = other.size_;
            other.clear();
        }
    }

    void releaseHeapStorage() noexcept {
        if (usingHeapStorage()) {
            deallocate(data_);
            resetInlinePointer();
        }
    }

    void ensureCapacity(size_type min_capacity) {
        if (min_capacity <= capacity_) {
            return;
        }

        size_type new_capacity = capacity_ ? capacity_ * 2 : (has_inline_storage ? stack_capacity_value : size_type{0});
        if (new_capacity == 0) {
            new_capacity = 1;
        }
        if (new_capacity < min_capacity) {
            new_capacity = min_capacity;
        }
        reallocate(new_capacity);
    }

    void reallocate(size_type new_capacity) {
        pointer new_data = allocate(new_capacity);
        size_type i = 0;
        try {
            for (; i < size_; ++i) {
                ::new (static_cast<void*>(new_data + i)) T(std::move_if_noexcept(data_[i]));
            }
        } catch (...) {
            destroyRange(new_data, new_data + i);
            deallocate(new_data);
            throw;
        }

        pointer old_data = data_;
        const size_type old_size = size_;
        const bool was_heap = usingHeapStorage();

        destroyRange(data_, data_ + size_);
        if (was_heap) {
            deallocate(old_data);
        }

        data_ = new_data;
        capacity_ = new_capacity;
        size_ = old_size;
    }

    static pointer allocate(size_type capacity) {
        if (capacity == 0) {
            return nullptr;
        }
        return static_cast<pointer>(::operator new(sizeof(T) * capacity));
    }

    static void deallocate(pointer ptr) noexcept {
        ::operator delete(ptr);
    }

    static void destroyRange(pointer first, pointer last) noexcept {
        while (first != last) {
            --last;
            last->~T();
        }
    }

    storage_type stack_storage_[has_inline_storage ? stack_capacity_value : 1];
    size_type size_{0};
    size_type capacity_{stack_capacity_value};
    pointer data_{nullptr};
};

template <typename T, std::size_t N>
void swap(SmallVector<T, N>& lhs, SmallVector<T, N>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

} // namespace orteaf::internal::base

#pragma once

#include <cstddef>
#include <iterator>
#include <new>
#include <stdexcept>
#include <type_traits>

#include "orteaf/internal/base/heap_vector.h"

namespace orteaf::internal::base {

/**
 * @brief Segmented vector with runtime-configurable block size.
 *
 * This variant keeps stable addresses like BlockVector, but accepts the block
 * size at runtime to avoid propagating a template parameter through layers.
 */
template <typename T>
class RuntimeBlockVector {
public:
  class iterator {
  public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T *;
    using reference = T &;

    iterator() = default;
    reference operator*() const { return (*owner_)[index_]; }
    pointer operator->() const { return &(*owner_)[index_]; }

    iterator &operator++() {
      ++index_;
      return *this;
    }
    iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }
    iterator &operator--() {
      --index_;
      return *this;
    }
    iterator operator--(int) {
      iterator tmp = *this;
      --(*this);
      return tmp;
    }

    iterator &operator+=(difference_type n) {
      index_ += static_cast<std::size_t>(n);
      return *this;
    }
    iterator &operator-=(difference_type n) {
      index_ -= static_cast<std::size_t>(n);
      return *this;
    }
    iterator operator+(difference_type n) const {
      iterator tmp = *this;
      return tmp += n;
    }
    iterator operator-(difference_type n) const {
      iterator tmp = *this;
      return tmp -= n;
    }
    difference_type operator-(const iterator &other) const {
      return static_cast<difference_type>(index_) -
             static_cast<difference_type>(other.index_);
    }

    bool operator==(const iterator &other) const {
      return owner_ == other.owner_ && index_ == other.index_;
    }
    bool operator!=(const iterator &other) const { return !(*this == other); }
    bool operator<(const iterator &other) const { return index_ < other.index_; }
    bool operator<=(const iterator &other) const {
      return index_ <= other.index_;
    }
    bool operator>(const iterator &other) const { return index_ > other.index_; }
    bool operator>=(const iterator &other) const {
      return index_ >= other.index_;
    }

  private:
    friend class RuntimeBlockVector;
    iterator(RuntimeBlockVector *owner, std::size_t index)
        : owner_(owner), index_(index) {}

    RuntimeBlockVector *owner_{nullptr};
    std::size_t index_{0};
  };

  class const_iterator {
  public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = const T;
    using difference_type = std::ptrdiff_t;
    using pointer = const T *;
    using reference = const T &;

    const_iterator() = default;
    reference operator*() const { return (*owner_)[index_]; }
    pointer operator->() const { return &(*owner_)[index_]; }

    const_iterator &operator++() {
      ++index_;
      return *this;
    }
    const_iterator operator++(int) {
      const_iterator tmp = *this;
      ++(*this);
      return tmp;
    }
    const_iterator &operator--() {
      --index_;
      return *this;
    }
    const_iterator operator--(int) {
      const_iterator tmp = *this;
      --(*this);
      return tmp;
    }

    const_iterator &operator+=(difference_type n) {
      index_ += static_cast<std::size_t>(n);
      return *this;
    }
    const_iterator &operator-=(difference_type n) {
      index_ -= static_cast<std::size_t>(n);
      return *this;
    }
    const_iterator operator+(difference_type n) const {
      const_iterator tmp = *this;
      return tmp += n;
    }
    const_iterator operator-(difference_type n) const {
      const_iterator tmp = *this;
      return tmp -= n;
    }
    difference_type operator-(const const_iterator &other) const {
      return static_cast<difference_type>(index_) -
             static_cast<difference_type>(other.index_);
    }

    bool operator==(const const_iterator &other) const {
      return owner_ == other.owner_ && index_ == other.index_;
    }
    bool operator!=(const const_iterator &other) const {
      return !(*this == other);
    }
    bool operator<(const const_iterator &other) const {
      return index_ < other.index_;
    }
    bool operator<=(const const_iterator &other) const {
      return index_ <= other.index_;
    }
    bool operator>(const const_iterator &other) const {
      return index_ > other.index_;
    }
    bool operator>=(const const_iterator &other) const {
      return index_ >= other.index_;
    }

  private:
    friend class RuntimeBlockVector;
    const_iterator(const RuntimeBlockVector *owner, std::size_t index)
        : owner_(owner), index_(index) {}

    const RuntimeBlockVector *owner_{nullptr};
    std::size_t index_{0};
  };

  explicit RuntimeBlockVector(std::size_t block_size = 64)
      : block_size_(block_size) {
    if (block_size_ == 0) {
      throw std::invalid_argument("RuntimeBlockVector block size must be > 0");
    }
  }

  RuntimeBlockVector(const RuntimeBlockVector &) = delete;
  RuntimeBlockVector &operator=(const RuntimeBlockVector &) = delete;
  RuntimeBlockVector(RuntimeBlockVector &&other) noexcept
      : blocks_(std::move(other.blocks_)),
        size_(other.size_),
        capacity_(other.capacity_),
        block_size_(other.block_size_) {
    other.size_ = 0;
    other.capacity_ = 0;
  }
  RuntimeBlockVector &operator=(RuntimeBlockVector &&other) noexcept {
    if (this == &other) {
      return *this;
    }
    destroyElements();
    releaseBlocks();
    blocks_ = std::move(other.blocks_);
    size_ = other.size_;
    capacity_ = other.capacity_;
    block_size_ = other.block_size_;
    other.size_ = 0;
    other.capacity_ = 0;
    return *this;
  }

  ~RuntimeBlockVector() {
    destroyElements();
    releaseBlocks();
  }

  void resize(std::size_t new_size) {
    if (new_size < size_) {
      destroyRange(new_size, size_);
      size_ = new_size;
      return;
    }
    ensureCapacityFor(new_size);
    std::size_t i = size_;
    try {
      for (; i < new_size; ++i) {
        new (ptrAt(i)) T();
      }
    } catch (...) {
      destroyRange(size_, i);
      throw;
    }
    size_ = new_size;
  }

  void pushBack(const T &value)
    requires std::is_copy_constructible_v<T>
  {
    emplaceBack(value);
  }
  void pushBack(const T &)
    requires (!std::is_copy_constructible_v<T>)
  = delete;
  void pushBack(T &&value) { emplaceBack(static_cast<T &&>(value)); }

  template <typename... Args>
  T &emplaceBack(Args &&...args) {
    ensureCapacityFor(size_ + 1);
    T *slot = ptrAt(size_);
    new (slot) T(static_cast<Args &&>(args)...);
    ++size_;
    return *slot;
  }

  void popBack() {
    if (size_ == 0) {
      return;
    }
    const std::size_t last = size_ - 1;
    ptrAt(last)->~T();
    size_ = last;
  }

  void clear() {
    destroyElements();
    size_ = 0;
  }

  void reserve(std::size_t new_capacity) {
    if (new_capacity <= capacity_) {
      return;
    }
    ensureCapacityFor(new_capacity);
  }

  std::size_t size() const noexcept { return size_; }
  std::size_t capacity() const noexcept { return capacity_; }
  bool empty() const noexcept { return size_ == 0; }
  std::size_t blockSize() const noexcept { return block_size_; }

  T &operator[](std::size_t idx) noexcept { return *ptrAt(idx); }
  const T &operator[](std::size_t idx) const noexcept { return *ptrAt(idx); }

  T &at(std::size_t idx) {
    if (idx >= size_) {
      throw std::out_of_range("RuntimeBlockVector::at");
    }
    return (*this)[idx];
  }
  const T &at(std::size_t idx) const {
    if (idx >= size_) {
      throw std::out_of_range("RuntimeBlockVector::at");
    }
    return (*this)[idx];
  }

  T &front() noexcept { return (*this)[0]; }
  const T &front() const noexcept { return (*this)[0]; }
  T &back() noexcept { return (*this)[size_ - 1]; }
  const T &back() const noexcept { return (*this)[size_ - 1]; }

  iterator begin() noexcept { return iterator(this, 0); }
  iterator end() noexcept { return iterator(this, size_); }
  const_iterator begin() const noexcept { return const_iterator(this, 0); }
  const_iterator end() const noexcept { return const_iterator(this, size_); }
  const_iterator cbegin() const noexcept { return const_iterator(this, 0); }
  const_iterator cend() const noexcept { return const_iterator(this, size_); }

private:
  using BlockPtr = T *;

  void ensureCapacityFor(std::size_t required) {
    if (required <= capacity_) {
      return;
    }
    const std::size_t needed_blocks =
        (required + block_size_ - 1) / block_size_;
    while (blocks_.size() < needed_blocks) {
      blocks_.pushBack(allocateBlock());
    }
    capacity_ = blocks_.size() * block_size_;
  }

  BlockPtr allocateBlock() {
    return static_cast<BlockPtr>(::operator new(sizeof(T) * block_size_));
  }

  void releaseBlocks() {
    for (std::size_t i = 0; i < blocks_.size(); ++i) {
      ::operator delete(blocks_[i]);
    }
    blocks_.clear();
    capacity_ = 0;
  }

  T *ptrAt(std::size_t idx) noexcept {
    const std::size_t block_index = idx / block_size_;
    const std::size_t offset = idx % block_size_;
    return blocks_[block_index] + offset;
  }

  const T *ptrAt(std::size_t idx) const noexcept {
    const std::size_t block_index = idx / block_size_;
    const std::size_t offset = idx % block_size_;
    return blocks_[block_index] + offset;
  }

  void destroyRange(std::size_t begin, std::size_t end) {
    for (std::size_t i = begin; i < end; ++i) {
      ptrAt(i)->~T();
    }
  }

  void destroyElements() { destroyRange(0, size_); }

  ::orteaf::internal::base::HeapVector<BlockPtr> blocks_{};
  std::size_t size_{0};
  std::size_t capacity_{0};
  std::size_t block_size_{64};
};

} // namespace orteaf::internal::base

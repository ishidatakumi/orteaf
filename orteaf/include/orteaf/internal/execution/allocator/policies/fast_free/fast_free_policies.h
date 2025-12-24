#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>

#include "orteaf/internal/execution/allocator/policies/policy_config.h"

namespace orteaf::internal::execution::allocator::policies {

// Fast path: round up to next power-of-two size class.
class FastFreePolicy {
public:
  template <typename Resource> using Config = PolicyConfig<Resource>;

  FastFreePolicy() = default;
  FastFreePolicy(const FastFreePolicy &) = delete;
  FastFreePolicy &operator=(const FastFreePolicy &) = delete;
  FastFreePolicy(FastFreePolicy &&) = default;
  FastFreePolicy &operator=(FastFreePolicy &&) = default;
  ~FastFreePolicy() = default;

  template <typename Resource> void initialize(const Config<Resource> &) {}

  void error() {}

  std::size_t get_block_size(std::size_t min_block_size,
                             std::size_t size_bytes) const {
    return std::bit_ceil(std::max(min_block_size, size_bytes));
  }
};

// Placeholder for a safety-oriented strategy (to be defined per allocator
// needs).
class SafeFreePolicy {
public:
  template <typename Resource> using Config = PolicyConfig<Resource>;

  SafeFreePolicy() = default;
  SafeFreePolicy(const SafeFreePolicy &) = delete;
  SafeFreePolicy &operator=(const SafeFreePolicy &) = delete;
  SafeFreePolicy(SafeFreePolicy &&) = default;
  SafeFreePolicy &operator=(SafeFreePolicy &&) = default;
  ~SafeFreePolicy() = default;

  template <typename Resource> void initialize(const Config<Resource> &) {}

  void error() {}

  std::size_t get_block_size(std::size_t /*min_block_size*/,
                             std::size_t /*size_bytes*/) const {
    return 0;
  }
};

} // namespace orteaf::internal::execution::allocator::policies

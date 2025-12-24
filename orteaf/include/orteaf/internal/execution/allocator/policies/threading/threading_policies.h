#pragma once

#include <mutex>

#include "orteaf/internal/execution/allocator/policies/policy_config.h"

namespace orteaf::internal::execution::allocator::policies {

// Mutex-based threading policy for multi-threaded contexts.
class LockingThreadingPolicy {
public:
  template <typename Resource> using Config = PolicyConfig<Resource>;

  LockingThreadingPolicy() = default;
  LockingThreadingPolicy(const LockingThreadingPolicy &) = delete;
  LockingThreadingPolicy &operator=(const LockingThreadingPolicy &) = delete;
  // std::mutex is not movable, so we delete move operations
  LockingThreadingPolicy(LockingThreadingPolicy &&) = delete;
  LockingThreadingPolicy &operator=(LockingThreadingPolicy &&) = delete;
  ~LockingThreadingPolicy() = default;

  template <typename Resource> void initialize(const Config<Resource> &) {}

  void lock() { mutex_.lock(); }
  void unlock() { mutex_.unlock(); }

private:
  std::mutex mutex_;
};

// No-op threading policy for single-threaded contexts.
class NoLockThreadingPolicy {
public:
  template <typename Resource> using Config = PolicyConfig<Resource>;

  NoLockThreadingPolicy() = default;
  NoLockThreadingPolicy(const NoLockThreadingPolicy &) = delete;
  NoLockThreadingPolicy &operator=(const NoLockThreadingPolicy &) = delete;
  NoLockThreadingPolicy(NoLockThreadingPolicy &&) = default;
  NoLockThreadingPolicy &operator=(NoLockThreadingPolicy &&) = default;
  ~NoLockThreadingPolicy() = default;

  template <typename Resource> void initialize(const Config<Resource> &) {}

  void lock() {}
  void unlock() {}
};

} // namespace orteaf::internal::execution::allocator::policies

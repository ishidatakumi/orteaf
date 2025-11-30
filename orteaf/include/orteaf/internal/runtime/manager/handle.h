#pragma once

#include <utility>

#include "orteaf/internal/base/strong_id.h"

namespace orteaf::internal::runtime::mps {

// Borrowed: shared resources (device/library/function/pipeline/...).
// Copyable; does not release on destruction.
template <class HandleT, class IdT>
class BorrowedHandle {
public:
  BorrowedHandle() = default;
  BorrowedHandle(HandleT handle, IdT id) : handle_(handle), id_(id) {}

  HandleT get() const noexcept { return handle_; }
  IdT id() const noexcept { return id_; }
  explicit operator bool() const noexcept { return handle_ != nullptr; }

private:
  HandleT handle_{nullptr};
  IdT id_{};
};

// Scoped: exclusive or must-return resources (queue/heap/fence/...).
// Move-only; releases via Releaser when destroyed or release() is called.
template <class HandleT, class IdT, class Releaser>
class ScopedHandle {
public:
  ScopedHandle() = delete;
  ScopedHandle(const ScopedHandle&) = delete;
  ScopedHandle& operator=(const ScopedHandle&) = delete;

  ScopedHandle(ScopedHandle&& other) noexcept { moveFrom(other); }
  ScopedHandle& operator=(ScopedHandle&& other) noexcept {
    if (this != &other) {
      release();
      moveFrom(other);
    }
    return *this;
  }

  ~ScopedHandle() { release(); }

  ScopedHandle(HandleT handle, IdT id, Releaser releaser = Releaser()) noexcept
      : handle_(handle), id_(id), releaser_(std::move(releaser)) {}

  HandleT get() const noexcept { return handle_; }
  IdT id() const noexcept { return id_; }
  explicit operator bool() const noexcept { return handle_ != nullptr; }

  void release() noexcept {
    if (handle_ != nullptr) {
      releaser_(id_, handle_);
    }
    handle_ = nullptr;
    id_ = IdT{};
  }

private:
  void moveFrom(ScopedHandle& other) noexcept {
    handle_ = other.handle_;
    id_ = other.id_;
    releaser_ = std::move(other.releaser_);
    other.handle_ = nullptr;
    other.id_ = IdT{};
    other.releaser_ = Releaser();
  }

  HandleT handle_{nullptr};
  IdT id_{};
  Releaser releaser_{};
};

} // namespace orteaf::internal::runtime::mps

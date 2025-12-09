#pragma once

#include <type_traits>
#include <utility>

namespace orteaf::internal::base {

/**
 * @brief A generic shared lease implementation that manages a resource using
 * reference counting provided by the ManagerT.
 *
 * This class is copyable and movable. Copying increments the reference count
 * via `manager_->retain(handle_)`. Destruction or release decrements the
 * reference count via `manager_->release(handle_)`.
 *
 * @tparam HandleT The type of the handle used to identify the resource in the
 * manager.
 * @tparam ResourceT The type of the actual resource being managed.
 * @tparam ManagerT The type of the manager that owns the resource. Must provide
 * `retain(HandleT)` and `release(HandleT)`.
 */
template <class HandleT, class ResourceT, class ManagerT> class SharedLease {
public:
  using HandleType = HandleT;
  using ResourceType = ResourceT;
  using ManagerType = ManagerT;

  SharedLease() = default;

  SharedLease(ManagerType *manager, HandleType handle, ResourceType resource)
      : manager_(manager), handle_(handle), resource_(resource) {}

  SharedLease(const SharedLease &other) { copyFrom(other); }

  SharedLease &operator=(const SharedLease &other) {
    if (this != &other) {
      release();
      copyFrom(other);
    }
    return *this;
  }

  SharedLease(SharedLease &&other) noexcept
      : manager_(std::exchange(other.manager_, nullptr)),
        handle_(std::exchange(other.handle_, HandleType{})),
        resource_(std::exchange(other.resource_, nullptr)) {}

  SharedLease &operator=(SharedLease &&other) noexcept {
    if (this != &other) {
      release();
      manager_ = std::exchange(other.manager_, nullptr);
      handle_ = std::exchange(other.handle_, HandleType{});
      resource_ = std::exchange(other.resource_, nullptr);
    }
    return *this;
  }

  ~SharedLease() { release(); }

  ResourceType get() const { return resource_; }
  ResourceType pointer() const { return resource_; } // for compatibility

  explicit operator bool() const { return manager_ != nullptr; }

  HandleType handle() const { return handle_; }

  void release() {
    if (manager_) {
      manager_->release(handle_);
      manager_ = nullptr;
      resource_ = nullptr;
      // handle_ is left as is, often useful for debugging or it's just a value
      // type
    }
  }

private:
  void copyFrom(const SharedLease &other) {
    if (other.manager_) {
      manager_ = other.manager_;
      handle_ = other.handle_;
      resource_ = other.resource_;
      manager_->retain(handle_);
    } else {
      manager_ = nullptr;
      resource_ = nullptr;
    }
  }

  ManagerType *manager_{nullptr};
  HandleType handle_{};
  ResourceType resource_{nullptr};
};

} // namespace orteaf::internal::base

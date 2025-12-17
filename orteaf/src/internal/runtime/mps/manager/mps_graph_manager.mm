#include "orteaf/internal/runtime/mps/manager/mps_graph_manager.h"

#if ORTEAF_ENABLE_MPS

#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::runtime::mps::manager {

void MpsGraphManager::initialize(DeviceType device, SlowOps *ops,
                                 std::size_t capacity) {
  shutdown();
  if (device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS graph manager requires a valid device");
  }
  if (ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS graph manager requires valid ops");
  }
  if (capacity > static_cast<std::size_t>(GraphHandle::invalid_index())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS graph manager capacity exceeds maximum handle range");
  }
  device_ = device;
  ops_ = ops;
  key_to_index_.clear();
  Base::setupPool(capacity);
}

void MpsGraphManager::shutdown() {
  Base::teardownPool([this](GraphControlBlock &cb, GraphHandle) {
    // Check if resource was created (payload has valid pointers)
    // isAlive() indicates acquisition state, not resource validity
    if (cb.payload().graph != nullptr || cb.payload().executable != nullptr) {
      destroyResource(cb.payload());
    }
  });
  key_to_index_.clear();
  device_ = nullptr;
  ops_ = nullptr;
}

MpsGraphManager::GraphLease
MpsGraphManager::acquire(const GraphKey &key, const CompileFn &compile_fn) {
  Base::ensureInitialized();
  validateKey(key);
  if (!compile_fn) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS graph compile function cannot be empty");
  }

  // Check if already cached
  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    // Reconstruct handle from cached index
    GraphHandle cached_handle{
        static_cast<typename GraphHandle::index_type>(it->second)};
    if constexpr (GraphHandle::has_generation) {
      cached_handle.generation =
          static_cast<typename GraphHandle::generation_type>(
              Base::getControlBlock(cached_handle).generation());
    }
    // For cache pattern: use direct acquire() instead of acquireShared()
    // acquireShared requires count>0, but cached resources may have count=0
    auto &cb = Base::getControlBlock(cached_handle);
    cb.acquire([](auto &) { return true; });
    return GraphLease{this, cached_handle, cb.payload().executable};
  }

  // Create new entry
  bool null_executable = false;
  auto handle = Base::acquireFresh([&](MpsGraphResource &resource) {
    resource.graph = ops_->createGraph();
    resource.executable = compile_fn(resource.graph, device_, ops_);
    if (resource.executable == nullptr) {
      // Cleanup the graph and signal failure
      ops_->destroyGraph(resource.graph);
      resource.graph = nullptr;
      null_executable = true;
      return false;
    }
    return true;
  });

  if (null_executable) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS graph compile function returned null executable");
  }

  if (handle == GraphHandle::invalid()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS graph manager failed to create graph");
  }

  key_to_index_.emplace(key, static_cast<std::size_t>(handle.index));
  return GraphLease{this, handle,
                    Base::getControlBlock(handle).payload().executable};
}

void MpsGraphManager::release(GraphLease &lease) noexcept {
  if (!lease) {
    return;
  }
  Base::releaseForReuse(lease.handle());
  lease.invalidate();
}

void MpsGraphManager::validateKey(const GraphKey &key) const {
  if (key.identifier.empty()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Graph identifier cannot be empty");
  }
  if (key.data_type == ::orteaf::internal::runtime::mps::platform::wrapper::
                           MpsGraphDataType::kInvalid) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Graph data type must be valid");
  }
  if (key.target_tensor_count == 0) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Graph target tensor count must be > 0");
  }
}

void MpsGraphManager::destroyResource(MpsGraphResource &resource) {
  if (resource.executable != nullptr) {
    ops_->destroyGraphExecutable(resource.executable);
    resource.executable = nullptr;
  }
  if (resource.graph != nullptr) {
    ops_->destroyGraph(resource.graph);
    resource.graph = nullptr;
  }
}

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

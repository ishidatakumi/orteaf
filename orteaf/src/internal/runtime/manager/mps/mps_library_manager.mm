#include "orteaf/internal/runtime/manager/mps/mps_library_manager.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps {

void MpsLibraryManager::initialize(
    ::orteaf::internal::backend::mps::MPSDevice_t device, BackendOps *ops,
    std::size_t capacity) {
  shutdown();
  if (device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS library manager requires a valid device");
  }
  if (ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS library manager requires valid ops");
  }
  device_ = device;
  ops_ = ops;
  if (capacity > base::LibraryHandle::invalid_index()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Requested MPS library capacity exceeds supported limit");
  }
  states_.clear();
  free_list_.clear();
  key_to_index_.clear();
  states_.reserve(capacity);
  free_list_.reserve(capacity);
  for (std::size_t i = 0; i < capacity; ++i) {
    states_.pushBack(State{});
    free_list_.pushBack(i);
  }
  initialized_ = true;
}

void MpsLibraryManager::shutdown() {
  if (!initialized_) {
    return;
  }
  for (std::size_t i = 0; i < states_.size(); ++i) {
    State &state = states_[i];
    if (state.alive) {
      state.pipeline_manager.shutdown();
      ops_->destroyLibrary(state.handle);
      state.reset();
    }
  }
  states_.clear();
  free_list_.clear();
  key_to_index_.clear();
  device_ = nullptr;
  ops_ = nullptr;
  initialized_ = false;
}

MpsLibraryManager::LibraryLease MpsLibraryManager::acquire(const LibraryKey &key) {
  ensureInitialized();
  validateKey(key);
  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    return acquireLibraryFromHandle(encodeId(it->second, states_[it->second].generation));
  }
  const std::size_t index = allocateSlot();
  State &state = states_[index];
  state.handle = createLibrary(key);
  state.key = key;
  state.pipeline_manager.initialize(device_, state.handle, ops_, 0);
  state.alive = true;
  state.use_count = 1;
  const auto id = encodeId(index, state.generation);
  key_to_index_.emplace(state.key, index);
  return LibraryLease{this, id, state.handle};
}

MpsLibraryManager::LibraryLease MpsLibraryManager::acquire(const PipelineManagerLease &pipeline_lease) {
  return acquireLibraryFromHandle(pipeline_lease.handle());
}

MpsLibraryManager::PipelineManagerLease
MpsLibraryManager::acquirePipelineManager(const LibraryLease &lease) {
  State &state = ensureAliveState(lease.handle());
  ++state.use_count;
  return PipelineManagerLease{this, lease.handle(), &state.pipeline_manager};
}

MpsLibraryManager::PipelineManagerLease
MpsLibraryManager::acquirePipelineManager(const LibraryKey &key) {
  auto library_lease = acquire(key);
  State &state = ensureAliveState(library_lease.handle());
  return PipelineManagerLease{this, library_lease.handle(), &state.pipeline_manager};
}

MpsLibraryManager::LibraryLease MpsLibraryManager::acquireLibraryFromHandle(base::LibraryHandle id) {
  State &state = ensureAliveState(id);
  ++state.use_count;
  return LibraryLease{this, id, state.handle};
}

void MpsLibraryManager::release(LibraryLease &lease) noexcept {
  releaseHandle(lease.handle());
}

void MpsLibraryManager::release(PipelineManagerLease &lease) noexcept {
  releaseHandle(lease.handle());
}

void MpsLibraryManager::releaseHandle(base::LibraryHandle id) noexcept {
  if (!initialized_ || device_ == nullptr || ops_ == nullptr) {
    return;
  }
  const std::size_t index = static_cast<std::size_t>(id.index);
  if (index >= states_.size()) {
    return;
  }
  State &state = states_[index];
  if (!state.alive) {
    return;
  }
  if (static_cast<base::LibraryHandle::generation_type>(state.generation) != id.generation) {
    return;
  }
  if (state.use_count > 0) {
    --state.use_count;
  }
  if (state.use_count != 0) {
    return;
  }
  key_to_index_.erase(state.key);
  state.pipeline_manager.shutdown();
  ops_->destroyLibrary(state.handle);
  state.reset();
  ++state.generation;
  free_list_.pushBack(index);
}

#if ORTEAF_ENABLE_TEST
MpsLibraryManager::DebugState
MpsLibraryManager::debugState(base::LibraryHandle id) const {
  DebugState snapshot{};
  snapshot.growth_chunk_size = growth_chunk_size_;
  const std::size_t index = static_cast<std::size_t>(id.index);
  if (index < states_.size()) {
    const State &state = states_[index];
    snapshot.alive = state.alive;
    snapshot.handle_allocated = state.handle != nullptr;
    snapshot.generation = state.generation;
    snapshot.use_count = state.use_count;
    snapshot.kind = state.key.kind;
    snapshot.identifier = state.key.identifier;
  } else {
    snapshot.generation = std::numeric_limits<std::uint32_t>::max();
  }
  return snapshot;
}
#endif

void MpsLibraryManager::ensureInitialized() const {
  if (!initialized_ || device_ == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS library manager not initialized");
  }
}

void MpsLibraryManager::validateKey(const LibraryKey &key) const {
  if (key.identifier.empty()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Library identifier cannot be empty");
  }
}

MpsLibraryManager::State &
MpsLibraryManager::ensureAliveState(base::LibraryHandle id) {
  ensureInitialized();
  const std::size_t index = static_cast<std::size_t>(id.index);
  if (index >= states_.size()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS library id out of range");
  }
  State &state = states_[index];
  if (!state.alive) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS library handle is inactive");
  }
  const std::uint32_t expected_generation = static_cast<std::uint32_t>(id.generation);
  if (static_cast<base::LibraryHandle::generation_type>(state.generation) != expected_generation) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS library handle is stale");
  }
  return state;
}

std::size_t MpsLibraryManager::allocateSlot() {
  if (free_list_.empty()) {
    growStatePool(growth_chunk_size_);
    if (free_list_.empty()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "No available MPS library slots");
    }
  }
  const std::size_t index = free_list_.back();
  free_list_.resize(free_list_.size() - 1);
  return index;
}

void MpsLibraryManager::growStatePool(std::size_t additional) {
  if (additional == 0) {
    return;
  }
  if (additional > (base::LibraryHandle::invalid_index() - states_.size())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Requested MPS library capacity exceeds supported limit");
  }
  const std::size_t start = states_.size();
  states_.reserve(states_.size() + additional);
  free_list_.reserve(free_list_.size() + additional);
  for (std::size_t offset = 0; offset < additional; ++offset) {
    states_.pushBack(State{});
    free_list_.pushBack(start + offset);
  }
}

base::LibraryHandle MpsLibraryManager::encodeId(std::size_t index,
                                            std::uint32_t generation) const {
  return base::LibraryHandle{static_cast<std::uint32_t>(index), static_cast<std::uint8_t>(generation)};
}

::orteaf::internal::backend::mps::MPSLibrary_t
MpsLibraryManager::createLibrary(const LibraryKey &key) {
  switch (key.kind) {
  case LibraryKeyKind::kNamed:
    return ops_->createLibraryWithName(device_, key.identifier);
  }
  ::orteaf::internal::diagnostics::error::throwError(
      ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
      "Unsupported MPS library key kind");
}

} // namespace orteaf::internal::runtime::mps

#endif // ORTEAF_ENABLE_MPS

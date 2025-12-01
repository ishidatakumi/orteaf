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
  if (capacity > base::LibraryId::invalid_index()) {
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

base::LibraryId MpsLibraryManager::getOrCreate(const LibraryKey &key) {
  ensureInitialized();
  validateKey(key);
  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    const State &state = states_[it->second];
    return encodeId(it->second, state.generation);
  }
  const std::size_t index = allocateSlot();
  State &state = states_[index];
  state.handle = createLibrary(key);
  state.key = key;
  state.pipeline_manager.initialize(device_, state.handle, ops_, 0);
  state.alive = true;
  const auto id = encodeId(index, state.generation);
  key_to_index_.emplace(state.key, index);
  return id;
}

void MpsLibraryManager::release(base::LibraryId id) {
  State &state = ensureAliveState(id);
  key_to_index_.erase(state.key);
  state.pipeline_manager.shutdown();
  ops_->destroyLibrary(state.handle);
  state.reset();
  ++state.generation;
  free_list_.pushBack(indexFromId(id));
}

::orteaf::internal::backend::mps::MPSLibrary_t
MpsLibraryManager::getLibrary(base::LibraryId id) const {
  return ensureAliveState(id).handle;
}

MpsLibraryManager::PipelineManager &
MpsLibraryManager::pipelineManager(base::LibraryId id) {
  State &state = ensureAliveState(id);
  return state.pipeline_manager;
}

const MpsLibraryManager::PipelineManager &
MpsLibraryManager::pipelineManager(base::LibraryId id) const {
  const State &state = ensureAliveState(id);
  return state.pipeline_manager;
}

#if ORTEAF_ENABLE_TEST
MpsLibraryManager::DebugState
MpsLibraryManager::debugState(base::LibraryId id) const {
  DebugState snapshot{};
  snapshot.growth_chunk_size = growth_chunk_size_;
  const std::size_t index = indexFromId(id);
  if (index < states_.size()) {
    const State &state = states_[index];
    snapshot.alive = state.alive;
    snapshot.handle_allocated = state.handle != nullptr;
    snapshot.generation = state.generation;
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
MpsLibraryManager::ensureAliveState(base::LibraryId id) {
  ensureInitialized();
  const std::size_t index = indexFromId(id);
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
  const std::uint32_t expected_generation = generationFromId(id);
  if (static_cast<base::LibraryId::generation_type>(state.generation) != expected_generation) {
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
  if (additional > (base::LibraryId::invalid_index() - states_.size())) {
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

base::LibraryId MpsLibraryManager::encodeId(std::size_t index,
                                            std::uint32_t generation) const {
  return base::LibraryId{static_cast<std::uint32_t>(index), static_cast<std::uint8_t>(generation)};
}

std::size_t MpsLibraryManager::indexFromId(base::LibraryId id) const {
  return static_cast<std::size_t>(id.index);
}

std::uint32_t MpsLibraryManager::generationFromId(base::LibraryId id) const {
  return static_cast<std::uint32_t>(id.generation);
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

#include "orteaf/internal/runtime/manager/mps/mps_compute_pipeline_state_manager.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps {

void MpsComputePipelineStateManager::initialize(
    ::orteaf::internal::backend::mps::MPSDevice_t device,
    ::orteaf::internal::backend::mps::MPSLibrary_t library, BackendOps *ops,
    std::size_t capacity) {
  shutdown();
  if (device == nullptr || library == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS compute pipeline state manager requires a valid device and "
        "library");
  }
  if (ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS compute pipeline state manager requires valid ops");
  }
  if (capacity > base::FunctionId::invalid_index()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Requested MPS compute pipeline capacity exceeds supported limit");
  }
  device_ = device;
  library_ = library;
  ops_ = ops;
  states_.clear();
  free_list_.clear();
  key_to_index_.clear();
  states_.reserve(capacity);
  free_list_.reserve(capacity);
  for (std::size_t i = 0; i < capacity; ++i) {
    states_.emplaceBack();
    free_list_.pushBack(i);
  }
  initialized_ = true;
}

void MpsComputePipelineStateManager::shutdown() {
  if (!initialized_) {
    return;
  }
  for (std::size_t i = 0; i < states_.size(); ++i) {
    destroyState(states_[i]);
  }
  states_.clear();
  free_list_.clear();
  key_to_index_.clear();
  device_ = nullptr;
  library_ = nullptr;
  ops_ = nullptr;
  initialized_ = false;
}

base::FunctionId
MpsComputePipelineStateManager::getOrCreate(const FunctionKey &key) {
  ensureInitialized();
  validateKey(key);
  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    const State &state = states_[it->second];
    return encodeId(it->second, state.generation);
  }
  const std::size_t index = allocateSlot();
  State &state = states_[index];
  state.key = key;
  state.function = ops_->createFunction(library_, key.identifier);
  if (state.function == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to create MPS function for compute pipeline");
  }
  state.pipeline_state =
      ops_->createComputePipelineState(device_, state.function);
  if (state.pipeline_state == nullptr) {
    ops_->destroyFunction(state.function);
    state.function = nullptr;
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "Failed to create MPS compute pipeline state");
  }
  state.alive = true;
  const auto id = encodeId(index, state.generation);
  key_to_index_.emplace(state.key, index);
  return id;
}

void MpsComputePipelineStateManager::release(base::FunctionId id) {
  State &state = ensureAliveState(id);
  key_to_index_.erase(state.key);
  destroyState(state);
  ++state.generation;
  free_list_.pushBack(indexFromId(id));
}

::orteaf::internal::backend::mps::MPSComputePipelineState_t
MpsComputePipelineStateManager::getPipelineState(base::FunctionId id) const {
  return ensureAliveState(id).pipeline_state;
}

::orteaf::internal::backend::mps::MPSFunction_t
MpsComputePipelineStateManager::getFunction(base::FunctionId id) const {
  return ensureAliveState(id).function;
}

#if ORTEAF_ENABLE_TEST
MpsComputePipelineStateManager::DebugState
MpsComputePipelineStateManager::debugState(base::FunctionId id) const {
  DebugState snapshot{};
  snapshot.growth_chunk_size = growth_chunk_size_;
  const std::size_t index = indexFromId(id);
  if (index < states_.size()) {
    const State &state = states_[index];
    snapshot.alive = state.alive;
    snapshot.pipeline_allocated = state.pipeline_state != nullptr;
    snapshot.function_allocated = state.function != nullptr;
    snapshot.generation = state.generation;
    snapshot.kind = state.key.kind;
    snapshot.identifier = state.key.identifier;
  } else {
    snapshot.generation = std::numeric_limits<std::uint32_t>::max();
  }
  return snapshot;
}
#endif

void MpsComputePipelineStateManager::ensureInitialized() const {
  if (!initialized_ || device_ == nullptr || library_ == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS compute pipeline state manager not initialized");
  }
}

void MpsComputePipelineStateManager::validateKey(const FunctionKey &key) const {
  if (key.identifier.empty()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Function identifier cannot be empty");
  }
}

void MpsComputePipelineStateManager::destroyState(State &state) {
  if (state.pipeline_state != nullptr) {
    ops_->destroyComputePipelineState(state.pipeline_state);
    state.pipeline_state = nullptr;
  }
  if (state.function != nullptr) {
    ops_->destroyFunction(state.function);
    state.function = nullptr;
  }
  state.alive = false;
}

MpsComputePipelineStateManager::State &
MpsComputePipelineStateManager::ensureAliveState(base::FunctionId id) {
  ensureInitialized();
  const std::size_t index = indexFromId(id);
  if (index >= states_.size()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS function id out of range");
  }
  State &state = states_[index];
  if (!state.alive) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS compute pipeline state is inactive");
  }
  const std::uint32_t expected_generation = generationFromId(id);
  if (static_cast<base::FunctionId::generation_type>(state.generation) != expected_generation) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS compute pipeline handle is stale");
  }
  return state;
}

std::size_t MpsComputePipelineStateManager::allocateSlot() {
  if (free_list_.empty()) {
    growStatePool(growth_chunk_size_);
    if (free_list_.empty()) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "No available MPS compute pipeline slots");
    }
  }
  const std::size_t index = free_list_.back();
  free_list_.resize(free_list_.size() - 1);
  return index;
}

void MpsComputePipelineStateManager::growStatePool(std::size_t additional) {
  if (additional == 0) {
    return;
  }
  if (additional > (base::FunctionId::invalid_index() - states_.size())) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Requested MPS compute pipeline capacity exceeds supported limit");
  }
  const std::size_t start = states_.size();
  states_.reserve(states_.size() + additional);
  free_list_.reserve(free_list_.size() + additional);
  for (std::size_t offset = 0; offset < additional; ++offset) {
    states_.emplaceBack();
    free_list_.pushBack(start + offset);
  }
}

base::FunctionId
MpsComputePipelineStateManager::encodeId(std::size_t index,
                                         std::uint32_t generation) const {
  return base::FunctionId{static_cast<std::uint32_t>(index), static_cast<std::uint8_t>(generation)};
}

std::size_t
MpsComputePipelineStateManager::indexFromId(base::FunctionId id) const {
  return static_cast<std::size_t>(id.index);
}

std::uint32_t
MpsComputePipelineStateManager::generationFromId(base::FunctionId id) const {
  return static_cast<std::uint32_t>(id.generation);
}

} // namespace orteaf::internal::runtime::mps

#endif // ORTEAF_ENABLE_MPS

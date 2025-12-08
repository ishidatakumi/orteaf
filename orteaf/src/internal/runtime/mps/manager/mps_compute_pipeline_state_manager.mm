#include <orteaf/internal/runtime/mps/manager/mps_compute_pipeline_state_manager.h>

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps::manager {

void MpsComputePipelineStateManager::initialize(
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t device,
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSLibrary_t library, SlowOps *slow_ops,
    std::size_t capacity) {
  shutdown();
  if (device == nullptr || library == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS compute pipeline state manager requires a valid device and "
        "library");
  }
  if (slow_ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS compute pipeline state manager requires valid ops");
  }
  if (capacity > ::orteaf::internal::base::FunctionHandle::invalid_index()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Requested MPS compute pipeline capacity exceeds supported limit");
  }
  device_ = device;
  library_ = library;
  ops_ = slow_ops;
  states_.clear();
  free_list_.clear();
  key_to_index_.clear();
  
  if (capacity > 0) {
    growPool(capacity);
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

MpsComputePipelineStateManager::PipelineLease
MpsComputePipelineStateManager::acquire(const FunctionKey &key) {
  ensureInitialized();
  validateKey(key);
  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    State &state = ensureAliveState(encodeHandle(it->second, states_[it->second].generation));
    ++state.use_count;
    return PipelineLease{this, encodeHandle(it->second, state.generation), state.pipeline_state};
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
  state.use_count = 1;
  const auto handle = encodeHandle(index, state.generation);
  key_to_index_.emplace(state.key, index);
  return PipelineLease{this, handle, state.pipeline_state};
}

void MpsComputePipelineStateManager::release(PipelineLease &lease) noexcept {
  if (!lease) {
    return;
  }
  releaseHandle(lease.handle());
  lease.invalidate();
}

#if ORTEAF_ENABLE_TEST
MpsComputePipelineStateManager::DebugState
MpsComputePipelineStateManager::debugState(::orteaf::internal::base::FunctionHandle handle) const {
  DebugState snapshot{};
  snapshot.growth_chunk_size = growth_chunk_size_;
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index < states_.size()) {
    const State &state = states_[index];
    snapshot.alive = state.alive;
    snapshot.pipeline_allocated = state.pipeline_state != nullptr;
    snapshot.function_allocated = state.function != nullptr;
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
  state.use_count = 0;
  state.alive = false;
}

MpsComputePipelineStateManager::State &
MpsComputePipelineStateManager::ensureAliveState(::orteaf::internal::base::FunctionHandle handle) {
  ensureInitialized();
  const std::size_t index = static_cast<std::size_t>(handle.index);
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
  const std::uint32_t expected_generation = static_cast<std::uint32_t>(handle.generation);
  if (static_cast<::orteaf::internal::base::FunctionHandle::generation_type>(state.generation) != expected_generation) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS compute pipeline handle is stale");
  }
  return state;
}

::orteaf::internal::base::FunctionHandle
MpsComputePipelineStateManager::encodeHandle(std::size_t index,
                                             std::uint32_t generation) const {
  return ::orteaf::internal::base::FunctionHandle{static_cast<std::uint32_t>(index), static_cast<std::uint8_t>(generation)};
}

void MpsComputePipelineStateManager::releaseHandle(::orteaf::internal::base::FunctionHandle handle) noexcept {
  if (!initialized_ || device_ == nullptr || library_ == nullptr || ops_ == nullptr) {
    return;
  }
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index >= states_.size()) {
    return;
  }
  State &state = states_[index];
  if (!state.alive) {
    return;
  }
  if (static_cast<::orteaf::internal::base::FunctionHandle::generation_type>(state.generation) != handle.generation) {
    return;
  }
  if (state.use_count > 0) {
    --state.use_count;
  }
  if (state.use_count != 0) {
    return;
  }
  key_to_index_.erase(state.key);
  destroyState(state);
  ++state.generation;
  free_list_.pushBack(index);
}

} // namespace orteaf::internal::runtime::mps::manager

#endif // ORTEAF_ENABLE_MPS

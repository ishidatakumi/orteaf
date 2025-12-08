#include "orteaf/internal/runtime/mps/manager/mps_graph_manager.h"

#if ORTEAF_ENABLE_MPS

namespace orteaf::internal::runtime::mps::manager {

void MpsGraphManager::initialize(
  ::orteaf::internal::runtime::mps::platform::wrapper::MPSDevice_t device, SlowOps* slow_ops,
    std::size_t capacity) {
  shutdown();
  if (device == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS graph manager requires a valid device");
  }
  if (slow_ops == nullptr) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS graph manager requires valid ops");
  }
  if (capacity > ::orteaf::internal::base::GraphHandle::invalid_index()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Requested MPS graph capacity exceeds supported limit");
  }

  device_ = device;
  ops_ = slow_ops;
  states_.clear();
  free_list_.clear();
  key_to_index_.clear();

  growth_chunk_size_ = capacity > 0 ? capacity : 1;
  if (capacity > 0) {
    growPool(capacity);
  }
  initialized_ = true;
}

void MpsGraphManager::shutdown() {
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
  ops_ = nullptr;
  initialized_ = false;
}

MpsGraphManager::GraphLease MpsGraphManager::acquire(
    const GraphKey& key, const CompileFn& compile_fn) {
  ensureInitialized();
  validateKey(key);
  if (!compile_fn) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS graph compile function cannot be empty");
  }

  if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
    const std::size_t index = it->second;
    MpsGraphManagerState& state = ensureAliveState(
        encodeHandle(index, states_[index].generation));
    return GraphLease{this,
                      encodeHandle(index, state.generation),
                      state.executable};
  }

  const std::size_t index = allocateSlot();
  MpsGraphManagerState& state = states_[index];
  try {
    state.graph = ops_->createGraph();
    state.executable = compile_fn(state.graph, device_, ops_);
    if (state.executable == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS graph compile function returned null executable");
    }
    state.key = key;
    state.alive = true;
    state.generation = 0;
    key_to_index_.emplace(state.key, index);
    return GraphLease{
        this, encodeHandle(index, state.generation), state.executable};
  } catch (...) {
    destroyState(state);
    if (auto it = key_to_index_.find(key); it != key_to_index_.end()) {
      key_to_index_.erase(it);
    }
    free_list_.pushBack(index);
    throw;
  }
}

void MpsGraphManager::release(GraphLease& lease) noexcept {
  if (!lease) {
    return;
  }
  lease.invalidate();
}

#if ORTEAF_ENABLE_TEST
MpsGraphManager::DebugState
MpsGraphManager::debugState(::orteaf::internal::base::GraphHandle handle) const {
  DebugState snapshot{};
  snapshot.growth_chunk_size = growth_chunk_size_;
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index < states_.size()) {
    const auto& state = states_[index];
    snapshot.alive = state.alive;
    snapshot.graph_allocated = state.graph != nullptr;
    snapshot.executable_allocated = state.executable != nullptr;
    snapshot.kind = state.key.kind;
    snapshot.identifier = state.key.identifier;
    snapshot.generation = state.generation;
    snapshot.shape = state.key.shape;
    snapshot.data_type = state.key.data_type;
    snapshot.target_tensor_count = state.key.target_tensor_count;
    snapshot.has_gradients = state.key.has_gradients;
  }
  return snapshot;
}
#endif

void MpsGraphManager::validateKey(const GraphKey& key) const {
  if (key.identifier.empty()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "Graph identifier cannot be empty");
  }
    if (key.data_type ==
      ::orteaf::internal::runtime::mps::platform::wrapper::MpsGraphDataType::kInvalid) {
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

::orteaf::internal::base::GraphHandle
MpsGraphManager::encodeHandle(std::size_t index,
                              std::uint32_t generation) const {
  return ::orteaf::internal::base::GraphHandle{
      static_cast<std::uint32_t>(index),
      static_cast<::orteaf::internal::base::GraphHandle::generation_type>(
          generation)};
}

void MpsGraphManager::destroyState(MpsGraphManagerState& state) {
  if (state.executable != nullptr) {
    ops_->destroyGraphExecutable(state.executable);
  }
  if (state.graph != nullptr) {
    ops_->destroyGraph(state.graph);
  }
  state.graph = nullptr;
  state.executable = nullptr;
  state.alive = false;
  ++state.generation;
}

MpsGraphManagerState& MpsGraphManager::ensureAliveState(
    ::orteaf::internal::base::GraphHandle handle) {
  ensureInitialized();
  const std::size_t index = static_cast<std::size_t>(handle.index);
  if (index >= states_.size()) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidArgument,
        "MPS graph handle out of range");
  }
  MpsGraphManagerState& state = states_[index];
  if (!state.alive ||
      static_cast<::orteaf::internal::base::GraphHandle::generation_type>(
          state.generation) != handle.generation) {
    ::orteaf::internal::diagnostics::error::throwError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        "MPS graph handle is inactive");
  }
  return state;
}

}  // namespace orteaf::internal::runtime::mps

#endif  // ORTEAF_ENABLE_MPS

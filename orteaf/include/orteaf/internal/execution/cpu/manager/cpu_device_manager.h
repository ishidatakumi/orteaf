#pragma once

#include "orteaf/internal/architecture/architecture.h"
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/execution/cpu/platform/cpu_backend_ops.h"

namespace orteaf::internal::execution::cpu::manager {

/**
 * @brief Minimal CPU device manager that exposes getters for the host CPU
 * state.
 *
 * See `docs/developer/runtime-architecture.md` for the runtime manager vision;
 * this concrete CPU implementation currently exposes only a single device with
 * `Architecture` metadata and an `is_alive` flag, driven by the lightweight
 * detector in `orteaf/internal/architecture/cpu_detect.h`.
 */
template <class BackendOps =
              ::orteaf::internal::execution::cpu::platform::CpuBackendOps>
struct CpuDeviceManager {
  void initializeDevices() {
    if (initialized_) {
      return;
    }
    state_.arch = BackendOps::detectArchitecture();
    state_.is_alive = true;
    initialized_ = true;
  }

  void shutdown() {
    if (!initialized_) {
      return;
    }
    state_.is_alive = false;
    initialized_ = false;
  }

  /**
   * @brief Return the number of managed CPU devices (0 or 1 for now).
   *
   * The count is derived from the initialization state. This manager only ever
   * reports a single host CPU device (`DeviceHandle{0}`) once
   * `initializeDevices()` has run.
   */
  std::size_t getDeviceCount() const { return initialized_ ? 1u : 0u; }

  /**
   * @brief Return the detected architecture for the requested CPU device.
   *
   * The architecture originates from `detectCpuArchitecture()`, so the
   * operating system's signals determine the value.
   */
  ::orteaf::internal::architecture::Architecture
  getArch(::orteaf::internal::base::DeviceHandle handle) const {
    ensureValid(handle);
    return state_.arch;
  }

  /**
   * @brief Query whether the CPU device is considered alive.
   *
   * Only the primary device exists today, so this will be `true` when the
   * manager is initialized and the caller supplies `DeviceHandle{0}`.
   */
  bool isAlive(::orteaf::internal::base::DeviceHandle handle) const {
    ensureValid(handle);
    return state_.is_alive;
  }

private:
  /**
   * @brief Validate the device id and initialization state.
   *
   * Throws `diagnostics::error::InvalidState` when validation fails, mirroring
   * what the rest of the runtime manager suite would expect from well-formed
   * getters.
   */
  void ensureValid(::orteaf::internal::base::DeviceHandle handle) const {
    if (!initialized_ || handle != kPrimaryDevice) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "invalid CPU device");
    }
  }

  /**
   * @brief Local storage for the detected CPU device information.
   */
  struct State {
    ::orteaf::internal::architecture::Architecture arch{
        ::orteaf::internal::architecture::Architecture::CpuGeneric};
    bool is_alive{false};
  };

  static constexpr ::orteaf::internal::base::DeviceHandle kPrimaryDevice{0};

  State state_{};
  bool initialized_{false};
};

inline CpuDeviceManager<> CpuDeviceManagerInstance{};

inline CpuDeviceManager<> &GetCpuDeviceManager() {
  return CpuDeviceManagerInstance;
}

} // namespace orteaf::internal::execution::cpu::manager

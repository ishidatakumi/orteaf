#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <memory>
#include <utility>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/execution/mps/manager/mps_runtime_manager.h"
#include "orteaf/internal/execution/mps/platform/mps_slow_ops.h"

namespace orteaf::internal::execution::mps::api {

class MpsRuntimeApi {
public:
  using Runtime = ::orteaf::internal::execution::mps::manager::MpsRuntimeManager;
  using DeviceHandle = ::orteaf::internal::base::DeviceHandle;
  using LibraryKey = ::orteaf::internal::execution::mps::manager::LibraryKey;
  using FunctionKey = ::orteaf::internal::execution::mps::manager::FunctionKey;
  using PipelineLease = ::orteaf::internal::execution::mps::manager::
      MpsComputePipelineStateManager::PipelineLease;
  using FenceLease =
      ::orteaf::internal::execution::mps::manager::MpsFenceManager::FenceLease;
  using SlowOps = ::orteaf::internal::execution::mps::platform::MpsSlowOps;

  MpsRuntimeApi() = delete;

  // Initialize runtime with a provided SlowOps implementation, or default
  // MpsSlowOpsImpl.
  static void initialize(std::unique_ptr<SlowOps> slow_ops = nullptr) {
    runtime().initialize(std::move(slow_ops));
  }

  static void shutdown() { runtime().shutdown(); }

  // Acquire a single pipeline for the given device/library/function key trio.
  static PipelineLease acquirePipeline(DeviceHandle device,
                                       const LibraryKey &library_key,
                                       const FunctionKey &function_key) {
    Runtime &rt = runtime();
    auto device_lease = rt.deviceManager().acquire(device);
    auto *resource = device_lease.payloadPtr();
    if (resource == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS device lease has no payload");
    }
    auto library_lease = resource->library_manager.acquire(library_key);
    auto *library_resource = library_lease.payloadPtr();
    if (library_resource == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS library lease has no payload");
    }
    return library_resource->pipeline_manager.acquire(function_key);
  }

  static FenceLease acquireFence(DeviceHandle device) {
    Runtime &rt = runtime();
    auto device_lease = rt.deviceManager().acquire(device);
    auto *resource = device_lease.payloadPtr();
    if (resource == nullptr) {
      ::orteaf::internal::diagnostics::error::throwError(
          ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
          "MPS device lease has no payload");
    }
    return resource->fence_pool.acquire();
  }

private:
  // Singleton access to the runtime manager (hidden from external callers).
  static Runtime &runtime() {
    static Runtime instance{};
    return instance;
  }
};

} // namespace orteaf::internal::execution::mps::api

#endif // ORTEAF_ENABLE_MPS

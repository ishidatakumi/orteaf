#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <utility>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/runtime/mps/manager/mps_runtime_manager.h"
#include "orteaf/internal/runtime/mps/ops/mps_common_ops.h"

namespace orteaf::internal::runtime::mps::ops {

class MpsPrivateOps {
  using Runtime = ::orteaf::internal::runtime::mps::manager::MpsRuntimeManager;
  using DeviceHandle = ::orteaf::internal::base::DeviceHandle;
  using LibraryKey = ::orteaf::internal::runtime::mps::manager::LibraryKey;
  using FunctionKey = ::orteaf::internal::runtime::mps::manager::FunctionKey;
  using PipelineLease = ::orteaf::internal::runtime::mps::manager::
      MpsComputePipelineStateManager::PipelineLease;
  using FenceLease =
      ::orteaf::internal::runtime::mps::manager::MpsFencePool::FenceLease;

public:
  MpsPrivateOps() = default;
  MpsPrivateOps(const MpsPrivateOps &) = default;
  MpsPrivateOps &operator=(const MpsPrivateOps &) = default;
  MpsPrivateOps(MpsPrivateOps &&) = default;
  MpsPrivateOps &operator=(MpsPrivateOps &&) = default;
  ~MpsPrivateOps() = default;

  // Acquire a single pipeline for the given device/library/function key trio.
  static PipelineLease acquirePipeline(DeviceHandle device,
                                       const LibraryKey &library_key,
                                       const FunctionKey &function_key) {
    Runtime &rt = MpsCommonOps::runtime();
    auto lib_mgr_lease = rt.deviceManager().acquireLibraryManager(device);
    auto library = lib_mgr_lease->acquire(library_key);
    auto pipeline_mgr = lib_mgr_lease->acquirePipelineManager(library);
    return pipeline_mgr->acquire(function_key);
  }

  static FenceLease acquireFence(DeviceHandle device) {
    Runtime &rt = MpsCommonOps::runtime();
    auto fence_pool = rt.deviceManager().acquireFencePool(device);
    return fence_pool->acquireFence();
  }
};

} // namespace orteaf::internal::runtime::mps::ops

#endif // ORTEAF_ENABLE_MPS

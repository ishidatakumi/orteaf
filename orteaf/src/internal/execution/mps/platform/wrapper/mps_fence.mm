/**
 * @file mps_fence.mm
 * @brief Implementation of MPS/Metal fence helpers.
 */
#ifndef __OBJC__
#error                                                                         \
    "mps_fence.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif

#include "orteaf/internal/execution/mps/platform/wrapper/mps_fence.h"

#import <Metal/Metal.h>

#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_autorelease_pool.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_compute_command_encoder.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_objc_bridge.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_stats.h"

namespace orteaf::internal::execution::mps::platform::wrapper {

/**
 * @copydoc orteaf::internal::backend::mps::createFence
 */
MpsFence_t createFence(MpsDevice_t device) {
  if (device == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer,
               "createFence: device cannot be nullptr");
  }
  AutoreleasePool pool{};
  id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
  id<MTLFence> objc_fence = [objc_device newFence];
  if (objc_fence == nil) {
    return nullptr;
  }
  updateCreateFence();
  return (MpsFence_t)opaqueFromObjcRetained(objc_fence);
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyFence
 */
void destroyFence(MpsFence_t fence) {
  if (fence == nullptr) {
    return;
  }
  AutoreleasePool pool{};
  opaqueReleaseRetained(fence);
  updateDestroyFence();
}

/**
 * @copydoc orteaf::internal::backend::mps::updateFence
 */
void updateFence(MpsComputeCommandEncoder_t encoder, MpsFence_t fence) {
  if (encoder == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer,
               "updateFence: encoder cannot be nullptr");
  }
  if (fence == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer, "updateFence: fence cannot be nullptr");
  }
  id<MTLComputeCommandEncoder> objc_encoder =
      objcFromOpaqueNoown<id<MTLComputeCommandEncoder>>(encoder);
  id<MTLFence> objc_fence = objcFromOpaqueNoown<id<MTLFence>>(fence);
  [objc_encoder updateFence:objc_fence];
}

/**
 * @copydoc orteaf::internal::backend::mps::waitForFence
 */
void waitForFence(MpsComputeCommandEncoder_t encoder, MpsFence_t fence) {
  if (encoder == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer,
               "waitForFence: encoder cannot be nullptr");
  }
  if (fence == nullptr) {
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::NullPointer,
               "waitForFence: fence cannot be nullptr");
  }
  id<MTLComputeCommandEncoder> objc_encoder =
      objcFromOpaqueNoown<id<MTLComputeCommandEncoder>>(encoder);
  id<MTLFence> objc_fence = objcFromOpaqueNoown<id<MTLFence>>(fence);
  [objc_encoder waitForFence:objc_fence];
}

} // namespace orteaf::internal::execution::mps::platform::wrapper

/**
 * @file mps_compute_pipeline_state.mm
 * @brief Implementation of MPS/Metal compute pipeline state helpers.
 */
#ifndef __OBJC__
#error                                                                         \
    "mps_compute_pipeline_state.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/execution/mps/platform/wrapper/mps_compute_pipeline_state.h"
#include "orteaf/internal/execution/mps/platform/wrapper/mps_objc_bridge.h"

#include "orteaf/internal/diagnostics/error/error.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

namespace orteaf::internal::execution::mps::platform::wrapper {

/**
 * @copydoc orteaf::internal::backend::mps::createComputePipelineState
 */
MpsComputePipelineState_t createComputePipelineState(MpsDevice_t device,
                                                     MpsFunction_t function,
                                                     MpsError_t *error) {
  if (device == nullptr || function == nullptr) {
    (void)error;
    using namespace orteaf::internal::diagnostics::error;
    throwError(
        OrteafErrc::NullPointer,
        "createComputePipelineState: device and function cannot be nullptr");
  }
  id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
  id<MTLFunction> objc_function =
      objcFromOpaqueNoown<id<MTLFunction>>(function);
  NSError **objc_error = error ? (NSError **)error : nullptr;

  id<MTLComputePipelineState> objc_pipeline_state =
      [objc_device newComputePipelineStateWithFunction:objc_function
                                                 error:objc_error];
  return (MpsComputePipelineState_t)opaqueFromObjcRetained(objc_pipeline_state);
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyComputePipelineState
 */
void destroyComputePipelineState(MpsComputePipelineState_t pipeline_state) {
  if (pipeline_state == nullptr)
    return;
  opaqueReleaseRetained(pipeline_state);
}

} // namespace orteaf::internal::execution::mps::platform::wrapper

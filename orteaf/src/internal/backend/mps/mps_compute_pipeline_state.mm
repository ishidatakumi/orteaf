/**
 * @file mps_compute_pipeline_state.mm
 * @brief Implementation of MPS/Metal compute pipeline state helpers.
 */
#ifndef __OBJC__
#error "mps_compute_pipeline_state.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/backend/mps/mps_compute_pipeline_state.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::createComputePipelineState
 */
MPSComputePipelineState_t createComputePipelineState(MPSDevice_t device, MPSFunction_t function, MPSError_t* error) {
    if (device == nullptr || function == nullptr) {
        (void)error;
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "createComputePipelineState: device and function cannot be nullptr");
    }
    id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
    id<MTLFunction> objc_function = objcFromOpaqueNoown<id<MTLFunction>>(function);
    NSError** objc_error = error ? (NSError**)error : nullptr;
    
    id<MTLComputePipelineState> objc_pipeline_state = [objc_device newComputePipelineStateWithFunction:objc_function error:objc_error];
    return (MPSComputePipelineState_t)opaqueFromObjcRetained(objc_pipeline_state);
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyComputePipelineState
 */
void destroyComputePipelineState(MPSComputePipelineState_t pipeline_state) {
    if (pipeline_state == nullptr) return;
    opaqueReleaseRetained(pipeline_state);
}

} // namespace orteaf::internal::backend::mps

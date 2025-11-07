/**
 * @file mps_pipeline_state.mm
 * @brief Implementation of MPS/Metal compute pipeline state helpers.
 */
#include "orteaf/internal/backend/mps/mps_pipeline_state.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "orteaf/internal/diagnostics/error/error.h"
#endif

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::create_pipeline_state
 */
MPSPipelineState_t create_pipeline_state(MPSDevice_t device, MPSFunction_t function, MPSError_t* error) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (!device || !function) {
        (void)error;
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "create_pipeline_state: device and function cannot be nullptr");
    }
    id<MTLDevice> objc_device = objc_from_opaque_noown<id<MTLDevice>>(device);
    id<MTLFunction> objc_function = objc_from_opaque_noown<id<MTLFunction>>(function);
    NSError** objc_error = error ? (NSError**)error : nullptr;
    
    id<MTLComputePipelineState> objc_pipeline_state = [objc_device newComputePipelineStateWithFunction:objc_function error:objc_error];
    return (MPSPipelineState_t)opaque_from_objc_retained(objc_pipeline_state);
#else
    (void)device;
    (void)function;
    (void)error;
    return nullptr;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::destroy_pipeline_state
 */
void destroy_pipeline_state(MPSPipelineState_t pipeline_state) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (pipeline_state) {
        opaque_release_retained(pipeline_state);
    }
#else
    (void)pipeline_state;
#endif
}

} // namespace orteaf::internal::backend::mps
/**
 * @file mps_command_queue.mm
 * @brief Implementation of MPS/Metal command queue helpers.
 */
#ifndef __OBJC__
#error "mps_command_queue.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_stats.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#import <Metal/Metal.h>
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::createCommandQueue
 */
MPSCommandQueue_t createCommandQueue(MPSDevice_t device) {
    if (device == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "createCommandQueue: device cannot be nullptr");
    }
    id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
    id<MTLCommandQueue> objc_command_queue = [objc_device newCommandQueue];
    updateCreateCommandQueue();
    return (MPSCommandQueue_t)opaqueFromObjcRetained(objc_command_queue);
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyCommandQueue
 */
void destroyCommandQueue(MPSCommandQueue_t command_queue) {
    if (command_queue == nullptr) return;
    opaqueReleaseRetained(command_queue);
    updateDestroyCommandQueue();
}

} // namespace orteaf::internal::backend::mps
/**
 * @file mps_command_queue.mm
 * @brief Implementation of MPS/Metal command queue helpers.
 */
#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_stats.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
#import <Metal/Metal.h>
#include "orteaf/internal/diagnostics/error/error.h"
#else
#include "orteaf/internal/diagnostics/error/error_impl.h"
#endif

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::createCommandQueue
 */
MPSCommandQueue_t createCommandQueue(MPSDevice_t device) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (device == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "createCommandQueue: device cannot be nullptr");
    }
    id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
    id<MTLCommandQueue> objc_command_queue = [objc_device newCommandQueue];
    updateCreateCommandQueue();
    return (MPSCommandQueue_t)opaqueFromObjcRetained(objc_command_queue);
#else
    (void)device;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "createCommandQueue: MPS backend is not available (MPS disabled)");
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyCommandQueue
 */
void destroyCommandQueue(MPSCommandQueue_t command_queue) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (command_queue == nullptr) return;
    opaqueReleaseRetained(command_queue);
    updateDestroyCommandQueue();
#else
    (void)command_queue;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "destroyCommandQueue: MPS backend is not available (MPS disabled)");
#endif
}

} // namespace orteaf::internal::backend::mps
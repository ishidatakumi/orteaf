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
#endif

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::create_command_queue
 */
MPSCommandQueue_t create_command_queue(MPSDevice_t device) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (device == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "create_command_queue: device cannot be nullptr");
    }
    id<MTLDevice> objc_device = objc_from_opaque_noown<id<MTLDevice>>(device);
    id<MTLCommandQueue> objc_command_queue = [objc_device newCommandQueue];
    update_create_command_queue();
    return (MPSCommandQueue_t)opaque_from_objc_retained(objc_command_queue);
#else
    (void)device;
    return nullptr;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::destroy_command_queue
 */
void destroy_command_queue(MPSCommandQueue_t command_queue) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (!command_queue) return;
    opaque_release_retained(command_queue);
    update_destroy_command_queue();
#else
    (void)command_queue;
#endif
}

} // namespace orteaf::internal::backend::mps
/**
 * @file mps_buffer.mm
 * @brief Implementation of MPS/Metal buffer helpers.
 */
#include "orteaf/internal/backend/mps/mps_buffer.h"
#include "orteaf/internal/backend/mps/mps_stats.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "orteaf/internal/diagnostics/error/error.h"
#endif

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::create_buffer
 */
MPSBuffer_t create_buffer(MPSDevice_t device, size_t size, MPSBufferUsage_t usage) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (device == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::NullPointer, "create_buffer: device cannot be nullptr");
    }
    if (size == 0) {
        using namespace orteaf::internal::diagnostics::error;
        throw_error(OrteafErrc::InvalidParameter, "create_buffer: size cannot be 0");
    }
    id<MTLDevice> objc_device = objc_from_opaque_noown<id<MTLDevice>>(device);
    MTLResourceOptions objc_usage = static_cast<MTLResourceOptions>(usage);
    id<MTLBuffer> objc_buffer = [objc_device newBufferWithLength:size options:objc_usage];
    update_alloc(size);
    return (MPSBuffer_t)opaque_from_objc_retained(objc_buffer);
#else
    (void)device;
    (void)size;
    (void)usage;
    return nullptr;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::destroy_buffer
 */
void destroy_buffer(MPSBuffer_t buffer) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (buffer != nullptr) {
        id<MTLBuffer> objc_buffer = objc_from_opaque_noown<id<MTLBuffer>>(buffer);
        size_t size = objc_buffer ? [objc_buffer length] : 0;
        opaque_release_retained(buffer);
        update_dealloc(size);
    }
#else
    (void)buffer;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::get_buffer_contents_const
 */
const void* get_buffer_contents_const(MPSBuffer_t buffer) {
#if defined(ORTEAF_ENABLE_MPS) && defined(__OBJC__)
    if (!buffer) return nullptr;
    id<MTLBuffer> objc_buffer = objc_from_opaque_noown<id<MTLBuffer>>(buffer);
    return [objc_buffer contents];
#else
    (void)buffer;
    return nullptr;
#endif
}

/**
 * @copydoc orteaf::internal::backend::mps::get_buffer_contents
 */
void* get_buffer_contents(MPSBuffer_t buffer) {
    return const_cast<void*>(get_buffer_contents_const(buffer));
}
} // namespace orteaf::internal::backend::mps
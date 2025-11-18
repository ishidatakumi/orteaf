/**
 * @file mps_library.mm
 * @brief Implementation of MPS/Metal library creation and destruction.
 */
#ifndef __OBJC__
#error "mps_library.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif
#include "orteaf/internal/backend/mps/mps_library.h"
#include "orteaf/internal/backend/mps/mps_objc_bridge.h"

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#import <objc/message.h>
#import <dispatch/dispatch.h>
#include "orteaf/internal/diagnostics/error/error.h"

namespace orteaf::internal::backend::mps {

/**
 * @copydoc orteaf::internal::backend::mps::createLibrary
 */
[[nodiscard]] MPSLibrary_t createLibrary(MPSDevice_t device, MPSString_t name, MPSError_t* error) {
    if (device == nullptr) {
        if (error != nullptr) {
            *error = nullptr;
        }
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "createLibrary: device cannot be nullptr");
    }
    if (name == nullptr) {
        if (error != nullptr) {
            *error = nullptr;
        }
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "createLibrary: name cannot be nullptr");
    }

    NSString* library_name = objcFromOpaqueNoown<NSString*>(name);
    if ([library_name length] == 0) {
        if (error != nullptr) {
            *error = nullptr;
        }
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidParameter, "createLibrary: name cannot be empty");
    }
    id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
    
    NSError* objc_error = nil;
    id<MTLLibrary> objc_library = nil;
    SEL selector = @selector(newLibraryWithName:error:);
    if ([objc_device respondsToSelector:selector]) {
        using NewLibraryFn = id<MTLLibrary> (*)(id, SEL, NSString*, NSError**);
        NewLibraryFn fn = reinterpret_cast<NewLibraryFn>(objc_msgSend);
        objc_library = fn(objc_device, selector, library_name, &objc_error);
    } else {
        objc_error = [NSError errorWithDomain:@"MTLLibraryErrorDomain"
                                        code:-2
                                    userInfo:@{NSLocalizedDescriptionKey : @"MTLDevice does not support newLibraryWithName:error:"}];
    }
    
    if (error != nullptr) {
        if (objc_error != nil) {
            *error = (MPSError_t)opaqueFromObjcRetained(objc_error);
        } else {
            *error = nullptr;
        }
    }
    
    return (MPSLibrary_t)opaqueFromObjcRetained(objc_library);
}

/**
 * @copydoc orteaf::internal::backend::mps::createLibraryWithSource
 */
[[nodiscard]] MPSLibrary_t createLibraryWithSource(MPSDevice_t device,
                                                      MPSString_t source,
                                                      MPSCompileOptions_t compile_options,
                                                      MPSError_t* error) {
    if (device == nullptr) {
        if (error != nullptr) {
            *error = nullptr;
        }
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "createLibraryWithSource: device cannot be nullptr");
    }
    if (source == nullptr) {
        if (error != nullptr) {
            *error = nullptr;
        }
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "createLibraryWithSource: source cannot be nullptr");
    }

    NSString* source_string = objcFromOpaqueNoown<NSString*>(source);
    if ([source_string length] == 0) {
        if (error != nullptr) {
            *error = nullptr;
        }
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidParameter, "createLibraryWithSource: source cannot be empty");
    }
    MTLCompileOptions* objc_compile_options = compile_options != nullptr
        ? objcFromOpaqueNoown<MTLCompileOptions*>(compile_options)
        : nil;
    
    id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
    NSError* objc_error = nil;
    id<MTLLibrary> objc_library = [objc_device newLibraryWithSource:source_string options:objc_compile_options error:&objc_error];
    
    if (error != nullptr) {
        if (objc_error != nil) {
            *error = (MPSError_t)opaqueFromObjcRetained(objc_error);
        } else {
            *error = nullptr;
        }
    }
    
    return (MPSLibrary_t)opaqueFromObjcRetained(objc_library);
}

/**
 * @copydoc orteaf::internal::backend::mps::destroyLibrary
 */
void destroyLibrary(MPSLibrary_t library) {
    if (library == nullptr) return;
    opaqueReleaseRetained(library);
}

/**
 * @copydoc orteaf::internal::backend::mps::createLibraryWithData
 */
[[nodiscard]] MPSLibrary_t createLibraryWithData(MPSDevice_t device,
                                                    const void* data,
                                                    std::size_t size,
                                                    MPSError_t* error) {
    if (device == nullptr) {
        if (error != nullptr) {
            *error = nullptr;
        }
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "createLibraryWithData: device cannot be nullptr");
    }
    if (data == nullptr) {
        if (error != nullptr) {
            *error = nullptr;
        }
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "createLibraryWithData: data cannot be nullptr");
    }
    if (size == 0) {
        if (error != nullptr) {
            *error = nullptr;
        }
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidParameter, "createLibraryWithData: size cannot be 0");
    }

    dispatch_data_t dispatch_data = dispatch_data_create(data, size, nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    if (dispatch_data == nullptr) {
        if (error != nullptr) {
            *error = nullptr;
        }
        return nullptr;
    }

    id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);
    NSError* objc_error = nil;
    id<MTLLibrary> objc_library = nil;
    SEL selector = @selector(newLibraryWithData:error:);
    if ([objc_device respondsToSelector:selector]) {
        using NewLibraryWithDataFn = id<MTLLibrary> (*)(id, SEL, dispatch_data_t, NSError**);
        NewLibraryWithDataFn fn = reinterpret_cast<NewLibraryWithDataFn>(objc_msgSend);
        objc_library = fn(objc_device, selector, dispatch_data, &objc_error);
    } else {
        objc_error = [NSError errorWithDomain:@"MTLLibraryErrorDomain"
                                        code:-3
                                    userInfo:@{NSLocalizedDescriptionKey : @"MTLDevice does not support newLibraryWithData:error:"}];
    }
    dispatch_release(dispatch_data);

    if (error != nullptr) {
        if (objc_error != nil) {
            *error = (MPSError_t)opaqueFromObjcRetained(objc_error);
        } else {
            *error = nullptr;
        }
    }
    
    return (MPSLibrary_t)opaqueFromObjcRetained(objc_library);
}

} // namespace orteaf::internal::backend::mps

/**
 * @file cuda_module.cu
 * @brief Implementation of CUDA module load/unload and function lookup.
 */
#ifndef __CUDACC__
#error "cuda_module.cu must be compiled with a CUDA compiler (__CUDACC__ not defined)"
#endif
#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_module.h"
#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_objc_bridge.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include <cuda.h>
#include "orteaf/internal/runtime/cuda/platform/wrapper/cuda_check.h"

namespace orteaf::internal::runtime::cuda::platform::wrapper {

/**
 * @copydoc orteaf::internal::backend::cuda::loadModuleFromFile
 */
CudaModule_t loadModuleFromFile(const char* filepath) {
    if (filepath == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "loadModuleFromFile: filepath cannot be nullptr");
    }
    if (filepath[0] == '\0') {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidParameter, "loadModuleFromFile: filepath cannot be empty");
    }
    CUmodule module;
    CU_CHECK(cuModuleLoad(&module, filepath));
    return opaqueFromObjcNoown<CudaModule_t, CUmodule>(module);
}

/**
 * @copydoc orteaf::internal::backend::cuda::loadModuleFromImage
 */
CudaModule_t loadModuleFromImage(const void* image) {
    if (image == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "loadModuleFromImage: image cannot be nullptr");
    }
    CUmodule module;
    CU_CHECK(cuModuleLoadData(&module, image));
    return opaqueFromObjcNoown<CudaModule_t, CUmodule>(module);
}

/**
 * @copydoc orteaf::internal::backend::cuda::getFunction
 */
CudaFunction_t getFunction(CudaModule_t module, const char* kernel_name) {
    if (module == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "getFunction: module cannot be nullptr");
    }
    if (kernel_name == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "getFunction: kernel_name cannot be nullptr");
    }
    if (kernel_name[0] == '\0') {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidParameter, "getFunction: kernel_name cannot be empty");
    }
    CUmodule objc_module = objcFromOpaqueNoown<CUmodule>(module);
    CUfunction function;
    CU_CHECK(cuModuleGetFunction(&function, objc_module, kernel_name));
    return opaqueFromObjcNoown<CudaFunction_t, CUfunction>(function);
}

/**
 * @copydoc orteaf::internal::backend::cuda::unloadModule
 */
void unloadModule(CudaModule_t module) {
    if (module == nullptr) return;
    CUmodule objc_module = objcFromOpaqueNoown<CUmodule>(module);
    CU_CHECK(cuModuleUnload(objc_module));
}

} // namespace orteaf::internal::runtime::cuda::platform::wrapper

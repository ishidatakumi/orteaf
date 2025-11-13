/**
 * @file cuda_module.cu
 * @brief Implementation of CUDA module load/unload and function lookup.
 */
#include "orteaf/internal/backend/cuda/cuda_module.h"
#include "orteaf/internal/backend/cuda/cuda_objc_bridge.h"
#include "orteaf/internal/diagnostics/error/error_impl.h"

#ifdef ORTEAF_ENABLE_CUDA
#include <cuda.h>
#include "orteaf/internal/backend/cuda/cuda_check.h"
#endif

namespace orteaf::internal::backend::cuda {

/**
 * @copydoc orteaf::internal::backend::cuda::loadModuleFromFile
 */
CUmodule_t loadModuleFromFile(const char* filepath) {
#ifdef ORTEAF_ENABLE_CUDA
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
    return opaqueFromObjcNoown<CUmodule_t, CUmodule>(module);
#else
    (void)filepath;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "CUDA backend is not available (CUDA is disabled). Called loadModuleFromFile()");
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::loadModuleFromImage
 */
CUmodule_t loadModuleFromImage(const void* image) {
#ifdef ORTEAF_ENABLE_CUDA
    if (image == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "loadModuleFromImage: image cannot be nullptr");
    }
    CUmodule module;
    CU_CHECK(cuModuleLoadData(&module, image));
    return opaqueFromObjcNoown<CUmodule_t, CUmodule>(module);
#else
    (void)image;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "CUDA backend is not available (CUDA is disabled). Called loadModuleFromImage()");
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::getFunction
 */
CUfunction_t getFunction(CUmodule_t module, const char* kernel_name) {
#ifdef ORTEAF_ENABLE_CUDA
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
    return opaqueFromObjcNoown<CUfunction_t, CUfunction>(function);
#else
    (void)module;
    (void)kernel_name;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "CUDA backend is not available (CUDA is disabled). Called getFunction()");
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::unloadModule
 */
void unloadModule(CUmodule_t module) {
#ifdef ORTEAF_ENABLE_CUDA
    if (module == nullptr) return;
    CUmodule objc_module = objcFromOpaqueNoown<CUmodule>(module);
    CU_CHECK(cuModuleUnload(objc_module));
#else
    (void)module;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "CUDA backend is not available (CUDA is disabled). Called unloadModule()");
#endif
}

} // namespace orteaf::internal::backend::cuda

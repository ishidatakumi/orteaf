#include "orteaf/internal/backend/cuda/cuda_module.h"
#include "orteaf/internal/backend/cuda/cuda_objc_bridge.h"

#ifdef CUDA_AVAILABLE
#include <cuda.h>
#include "orteaf/internal/backend/cuda/cuda_check.h"
#endif

namespace orteaf::internal::backend::cuda {

CUmodule_t load_module_from_file(const char* filepath) {
#ifdef CUDA_AVAILABLE
    CUmodule module;
    CU_CHECK(cuModuleLoad(&module, filepath));
    return opaque_from_objc_noown<CUmodule_t, CUmodule>(module);
#else
    (void)filepath;
    return nullptr;
#endif
}

CUmodule_t load_module_from_image(const void* image) {
#ifdef CUDA_AVAILABLE
    CUmodule module;
    CU_CHECK(cuModuleLoadData(&module, image));
    return opaque_from_objc_noown<CUmodule_t, CUmodule>(module);
#else
    (void)image;
    return nullptr;
#endif
}

CUfunction_t get_function(CUmodule_t module, const char* kernel_name) {
#ifdef CUDA_AVAILABLE
    CUmodule objc_module = objc_from_opaque_noown<CUmodule>(module);
    CUfunction function;
    CU_CHECK(cuModuleGetFunction(&function, objc_module, kernel_name));
    return opaque_from_objc_noown<CUfunction_t, CUfunction>(function);
#else
    (void)module;
    (void)kernel_name;
    return nullptr;
#endif
}

void unload_module(CUmodule_t module) {
#ifdef CUDA_AVAILABLE
    CUmodule objc_module = objc_from_opaque_noown<CUmodule>(module);
    CU_CHECK(cuModuleUnload(objc_module));
#else
    (void)module;
#endif
}

} // namespace orteaf::internal::backend::cuda

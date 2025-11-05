#include "orteaf/internal/backend/cuda/cuda_stream.h"
#include "orteaf/internal/backend/cuda/cuda_check.h"
#include "orteaf/internal/backend/cuda/cuda_stats.h"
#include "orteaf/internal/backend/cuda/cuda_objc_bridge.h"

#ifdef CUDA_AVAILABLE
#include <cuda.h>
#endif

namespace orteaf::internal::backend::cuda {

CUstream_t get_stream() {
#ifdef CUDA_AVAILABLE
    CUstream stream;
    CU_CHECK(cuStreamCreate(&stream, CU_STREAM_NON_BLOCKING));
    stats_on_create_stream();
    return opaque_from_objc_noown<CUstream_t, CUstream>(stream);
#else
    return nullptr;
#endif
}

void set_stream(CUstream_t stream) {
#ifdef CUDA_AVAILABLE
    (void)stream; // No driver API to set a global current stream.
#else
    (void)stream;
#endif
}

void release_stream(CUstream_t stream) {
#ifdef CUDA_AVAILABLE
    if (!stream) return;
    CUstream objc_stream = objc_from_opaque_noown<CUstream>(stream);
    CU_CHECK(cuStreamDestroy(objc_stream));
    stats_on_destroy_stream();
#else
    (void)stream;
#endif
}

void synchronize_stream(CUstream_t stream) {
#ifdef CUDA_AVAILABLE
    CUstream objc_stream = objc_from_opaque_noown<CUstream>(stream);
    CU_CHECK(cuStreamSynchronize(objc_stream));
#else
    (void)stream;
#endif
}

void wait_stream(CUstream_t stream, CUdeviceptr_t addr, uint32_t value) {
#ifdef CUDA_AVAILABLE
    CUstream objc_stream = objc_from_opaque_noown<CUstream>(stream);
    CUdeviceptr objc_addr = cu_deviceptr_from_opaque(addr);
    CU_CHECK(cuStreamWaitValue32(objc_stream, objc_addr, value, CU_STREAM_WAIT_VALUE_GEQ));
#else
    (void)stream;
    (void)addr;
    (void)value;
#endif
}

void write_stream(CUstream_t stream, CUdeviceptr_t addr, uint32_t value) {
#ifdef CUDA_AVAILABLE
    CUstream objc_stream = objc_from_opaque_noown<CUstream>(stream);
    CUdeviceptr objc_addr = cu_deviceptr_from_opaque(addr);
    CU_CHECK(cuStreamWriteValue32(objc_stream, objc_addr, value, CU_STREAM_WRITE_VALUE_DEFAULT));
#else
    (void)stream;
    (void)addr;
    (void)value;
#endif
}

} // namespace orteaf::internal::backend::cuda

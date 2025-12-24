/**
 * @file cuda_stream.cu
 * @brief Implementation of CUDA stream helpers (create/destroy/sync/mem signals).
 */
#ifndef __CUDACC__
#error "cuda_stream.cu must be compiled with a CUDA compiler (__CUDACC__ not defined)"
#endif
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_stream.h"
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_check.h"
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_stats.h"
#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_objc_bridge.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include <cuda.h>

namespace orteaf::internal::execution::cuda::platform::wrapper {

/**
 * @copydoc orteaf::internal::backend::cuda::getStream
 */
CudaStream_t getStream() {
    CUstream stream;
    CU_CHECK(cuStreamCreate(&stream, CU_STREAM_NON_BLOCKING));
    updateCreateStream();
    return opaqueFromObjcNoown<CudaStream_t, CUstream>(stream);
}

/**
 * @copydoc orteaf::internal::backend::cuda::releaseStream
 */
void releaseStream(CudaStream_t stream) {
    if (stream == nullptr) return;
    CUstream objc_stream = objcFromOpaqueNoown<CUstream>(stream);
    CU_CHECK(cuStreamDestroy(objc_stream));
    updateDestroyStream();
}

/**
 * @copydoc orteaf::internal::backend::cuda::synchronizeStream
 */
void synchronizeStream(CudaStream_t stream) {
    if (stream == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "synchronizeStream: stream cannot be nullptr");
    }
    CUstream objc_stream = objcFromOpaqueNoown<CUstream>(stream);
    CU_CHECK(cuStreamSynchronize(objc_stream));
}

/**
 * @copydoc orteaf::internal::backend::cuda::waitStream
 */
void waitStream(CudaStream_t stream, CudaDevicePtr_t addr, uint32_t value) {
    if (stream == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "waitStream: stream cannot be nullptr");
    }
    if (addr == 0) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidParameter, "waitStream: addr cannot be 0");
    }
    CUstream objc_stream = objcFromOpaqueNoown<CUstream>(stream);
    CUdeviceptr objc_addr = cuDeviceptrFromOpaque(addr);
    CU_CHECK(cuStreamWaitValue32(objc_stream, objc_addr, value, CU_STREAM_WAIT_VALUE_GEQ));
}

/**
 * @copydoc orteaf::internal::backend::cuda::writeStream
 */
void writeStream(CudaStream_t stream, CudaDevicePtr_t addr, uint32_t value) {
    if (stream == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "writeStream: stream cannot be nullptr");
    }
    if (addr == 0) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidParameter, "writeStream: addr cannot be 0");
    }
    CUstream objc_stream = objcFromOpaqueNoown<CUstream>(stream);
    CUdeviceptr objc_addr = cuDeviceptrFromOpaque(addr);
    CU_CHECK(cuStreamWriteValue32(objc_stream, objc_addr, value, CU_STREAM_WRITE_VALUE_DEFAULT));
}

} // namespace orteaf::internal::execution::cuda::platform::wrapper

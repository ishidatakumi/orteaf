/**
 * @file cuda_stream.cu
 * @brief Implementation of CUDA stream helpers (create/destroy/sync/mem signals).
 */
#include "orteaf/internal/backend/cuda/cuda_stream.h"
#include "orteaf/internal/backend/cuda/cuda_check.h"
#include "orteaf/internal/backend/cuda/cuda_stats.h"
#include "orteaf/internal/backend/cuda/cuda_objc_bridge.h"
#include "orteaf/internal/diagnostics/error/error_impl.h"

#ifdef ORTEAF_ENABLE_CUDA
#include <cuda.h>
#endif

namespace orteaf::internal::backend::cuda {

/**
 * @copydoc orteaf::internal::backend::cuda::getStream
 */
CUstream_t getStream() {
#ifdef ORTEAF_ENABLE_CUDA
    CUstream stream;
    CU_CHECK(cuStreamCreate(&stream, CU_STREAM_NON_BLOCKING));
    updateCreateStream();
    return opaqueFromObjcNoown<CUstream_t, CUstream>(stream);
#else
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "CUDA backend is not available (CUDA is disabled). Called getStream()");
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::releaseStream
 */
void releaseStream(CUstream_t stream) {
#ifdef ORTEAF_ENABLE_CUDA
    if (stream == nullptr) return;
    CUstream objc_stream = objcFromOpaqueNoown<CUstream>(stream);
    CU_CHECK(cuStreamDestroy(objc_stream));
    updateDestroyStream();
#else
    (void)stream;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "CUDA backend is not available (CUDA is disabled). Called releaseStream()");
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::synchronizeStream
 */
void synchronizeStream(CUstream_t stream) {
#ifdef ORTEAF_ENABLE_CUDA
    if (stream == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "synchronizeStream: stream cannot be nullptr");
    }
    CUstream objc_stream = objcFromOpaqueNoown<CUstream>(stream);
    CU_CHECK(cuStreamSynchronize(objc_stream));
#else
    (void)stream;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "CUDA backend is not available (CUDA is disabled). Called synchronizeStream()");
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::waitStream
 */
void waitStream(CUstream_t stream, CUdeviceptr_t addr, uint32_t value) {
#ifdef ORTEAF_ENABLE_CUDA
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
#else
    (void)stream;
    (void)addr;
    (void)value;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "CUDA backend is not available (CUDA is disabled). Called waitStream()");
#endif
}

/**
 * @copydoc orteaf::internal::backend::cuda::writeStream
 */
void writeStream(CUstream_t stream, CUdeviceptr_t addr, uint32_t value) {
#ifdef ORTEAF_ENABLE_CUDA
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
#else
    (void)stream;
    (void)addr;
    (void)value;
    using namespace orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::BackendUnavailable, "CUDA backend is not available (CUDA is disabled). Called writeStream()");
#endif
}

} // namespace orteaf::internal::backend::cuda

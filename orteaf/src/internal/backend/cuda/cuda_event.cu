/**
 * @file cuda_event.cu
 * @brief Implementation of CUDA event helpers (create/destroy/record/query/wait).
 */
#ifndef __CUDACC__
#error "cuda_event.cu must be compiled with a CUDA compiler (__CUDACC__ not defined)"
#endif
#include "orteaf/internal/backend/cuda/cuda_event.h"
#include "orteaf/internal/backend/cuda/cuda_stats.h"
#include "orteaf/internal/backend/cuda/cuda_check.h"
#include "orteaf/internal/backend/cuda/cuda_objc_bridge.h"

#include "orteaf/internal/diagnostics/error/error.h"
#include <cuda.h>

namespace orteaf::internal::backend::cuda {

/**
 * @copydoc orteaf::internal::backend::cuda::createEvent
 */
CUevent_t createEvent() {
    CUevent event;
    CU_CHECK(cuEventCreate(&event, CU_EVENT_DISABLE_TIMING));
    updateCreateEvent();
    return opaqueFromObjcNoown<CUevent_t, CUevent>(event);
}

/**
 * @copydoc orteaf::internal::backend::cuda::destroyEvent
 */
void destroyEvent(CUevent_t event) {
    if (event == nullptr) return;
    CUevent objc_event = objcFromOpaqueNoown<CUevent>(event);
    CU_CHECK(cuEventDestroy(objc_event));
    updateDestroyEvent();
}

/**
 * @copydoc orteaf::internal::backend::cuda::recordEvent
 */
void recordEvent(CUevent_t event, CUstream_t stream) {
    if (event == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "recordEvent: event cannot be nullptr");
    }
    if (stream == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "recordEvent: stream cannot be nullptr");
    }
    CUevent objc_event = objcFromOpaqueNoown<CUevent>(event);
    CUstream objc_stream = objcFromOpaqueNoown<CUstream>(stream);
    CU_CHECK(cuEventRecord(objc_event, objc_stream));
}

/**
 * @copydoc orteaf::internal::backend::cuda::queryEvent
 */
bool queryEvent(CUevent_t event) {
    if (event == nullptr) return true;
    CUevent objc_event = objcFromOpaqueNoown<CUevent>(event);
    CUresult status = cuEventQuery(objc_event);
    if (status == CUDA_SUCCESS) return true;
    if (status == CUDA_ERROR_NOT_READY) return false;
    CU_CHECK(status);
    return false;
}

/**
 * @copydoc orteaf::internal::backend::cuda::waitEvent
 */
void waitEvent(CUstream_t stream, CUevent_t event) {
    if (stream == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "waitEvent: stream cannot be nullptr");
    }
    if (event == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "waitEvent: event cannot be nullptr");
    }
    CUstream objc_stream = objcFromOpaqueNoown<CUstream>(stream);
    CUevent objc_event = objcFromOpaqueNoown<CUevent>(event);
    CU_CHECK(cuStreamWaitEvent(objc_stream, objc_event, 0));
}

} // namespace orteaf::internal::backend::cuda

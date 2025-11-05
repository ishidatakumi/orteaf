#include "orteaf/internal/backend/cuda/cuda_event.h"
#include "orteaf/internal/backend/cuda/cuda_stats.h"
#include "orteaf/internal/backend/cuda/cuda_check.h"
#include "orteaf/internal/backend/cuda/cuda_objc_bridge.h"

#ifdef CUDA_AVAILABLE
#include <cuda.h>
#endif

namespace orteaf::internal::backend::cuda {

CUevent_t create_event() {
#ifdef CUDA_AVAILABLE
    CUevent event;
    CU_CHECK(cuEventCreate(&event, CU_EVENT_DISABLE_TIMING));
    stats_on_create_event();
    return opaque_from_objc_noown<CUevent_t, CUevent>(event);
#else
    return nullptr;
#endif
}

void destroy_event(CUevent_t event) {
#ifdef CUDA_AVAILABLE
    if (event == nullptr) return;
    CUevent objc_event = objc_from_opaque_noown<CUevent>(event);
    CU_CHECK(cuEventDestroy(objc_event));
    stats_on_destroy_event();
#else
    (void)event;
#endif
}

void record_event(CUevent_t event, CUstream_t stream) {
#ifdef CUDA_AVAILABLE
    if (event == nullptr || stream == nullptr) return;
    CUevent objc_event = objc_from_opaque_noown<CUevent>(event);
    CUstream objc_stream = objc_from_opaque_noown<CUstream>(stream);
    CU_CHECK(cuEventRecord(objc_event, objc_stream));
#else
    (void)event;
    (void)stream;
#endif
}

bool query_event(CUevent_t event) {
#ifdef CUDA_AVAILABLE
    if (event == nullptr) return true;
    CUevent objc_event = objc_from_opaque_noown<CUevent>(event);
    CUresult status = cuEventQuery(objc_event);
    if (status == CUDA_SUCCESS) return true;
    if (status == CUDA_ERROR_NOT_READY) return false;
    CU_CHECK(status);
    return false;
#else
    (void)event;
    return true;
#endif
}

void wait_event(CUstream_t stream, CUevent_t event) {
#ifdef CUDA_AVAILABLE
    if (stream == nullptr || event == nullptr) return;
    CUstream objc_stream = objc_from_opaque_noown<CUstream>(stream);
    CUevent objc_event = objc_from_opaque_noown<CUevent>(event);
    CU_CHECK(cuStreamWaitEvent(objc_stream, objc_event, 0));
#else
    (void)stream;
    (void)event;
#endif
}

} // namespace orteaf::internal::backend::cuda

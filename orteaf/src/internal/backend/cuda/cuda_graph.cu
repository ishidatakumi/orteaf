/**
 * @file cuda_graph.cu
 * @brief Implementation of CUDA Graph helpers (create/destroy/capture/instantiate/launch).
 */
#ifndef __CUDACC__
#error "cuda_graph.cu must be compiled with a CUDA compiler (__CUDACC__ not defined)"
#endif
#include "orteaf/internal/backend/cuda/cuda_graph.h"
#include "orteaf/internal/backend/cuda/cuda_check.h"
#include "orteaf/internal/backend/cuda/cuda_objc_bridge.h"
#include "orteaf/internal/diagnostics/error/error.h"
#include <cuda.h>

namespace orteaf::internal::backend::cuda {

/**
 * @copydoc orteaf::internal::backend::cuda::createGraph
 */
CUgraph_t createGraph() {
    CUgraph graph;
    CU_CHECK(cuGraphCreate(&graph, 0));
    return opaqueFromObjcNoown<CUgraph_t, CUgraph>(graph);
}

/**
 * @copydoc orteaf::internal::backend::cuda::createGraphExec
 */
CUgraphExec_t createGraphExec(CUgraph_t graph) {
    if (graph == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "createGraphExec: graph cannot be nullptr");
    }
    CUgraph objc_graph = objcFromOpaqueNoown<CUgraph>(graph);
    CUgraphExec graph_exec;
    CU_CHECK(cuGraphInstantiateWithFlags(&graph_exec, objc_graph, 0));
    return opaqueFromObjcNoown<CUgraphExec_t, CUgraphExec>(graph_exec);
}

/**
 * @copydoc orteaf::internal::backend::cuda::destroyGraph
 */
void destroyGraph(CUgraph_t graph) {
    if (graph == nullptr) return;
    CUgraph objc_graph = objcFromOpaqueNoown<CUgraph>(graph);
    CU_CHECK(cuGraphDestroy(objc_graph));
}

/**
 * @copydoc orteaf::internal::backend::cuda::destroyGraphExec
 */
void destroyGraphExec(CUgraphExec_t graph_exec) {
    if (graph_exec == nullptr) return;
    CUgraphExec objc_graph_exec = objcFromOpaqueNoown<CUgraphExec>(graph_exec);
    CU_CHECK(cuGraphExecDestroy(objc_graph_exec));
}

/**
 * @copydoc orteaf::internal::backend::cuda::beginGraphCapture
 */
void beginGraphCapture(CUstream_t stream) {
    if (stream == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "beginGraphCapture: stream cannot be nullptr");
    }
    CUstream objc_stream = objcFromOpaqueNoown<CUstream>(stream);
    CU_CHECK(cuStreamBeginCapture(objc_stream, CU_STREAM_CAPTURE_MODE_GLOBAL));
}

/**
 * @copydoc orteaf::internal::backend::cuda::endGraphCapture
 */
void endGraphCapture(CUstream_t stream, CUgraph_t* graph) {
    if (stream == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "endGraphCapture: stream cannot be nullptr");
    }
    if (graph == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "endGraphCapture: graph cannot be nullptr");
    }
    CUstream objc_stream = objcFromOpaqueNoown<CUstream>(stream);
    CUstreamCaptureStatus capture_status = CU_STREAM_CAPTURE_STATUS_NONE;
    CUresult capture_query = cuStreamIsCapturing(objc_stream, &capture_status);
    if (capture_query == CUDA_SUCCESS && capture_status != CU_STREAM_CAPTURE_STATUS_ACTIVE) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::InvalidState, "endGraphCapture: stream is not actively capturing");
    }
    CUgraph objc_graph;
    CU_CHECK(cuStreamEndCapture(objc_stream, &objc_graph));
    *graph = opaqueFromObjcNoown<CUgraph_t, CUgraph>(objc_graph);
}

/**
 * @copydoc orteaf::internal::backend::cuda::instantiateGraph
 */
void instantiateGraph(CUgraph_t graph, CUgraphExec_t* graph_exec) {
    if (graph == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "instantiateGraph: graph cannot be nullptr");
    }
    if (graph_exec == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "instantiateGraph: graph_exec cannot be nullptr");
    }
    CUgraph objc_graph = objcFromOpaqueNoown<CUgraph>(graph);
    CUgraphExec objc_graph_exec;
    CU_CHECK(cuGraphInstantiateWithFlags(&objc_graph_exec, objc_graph, 0));
    *graph_exec = opaqueFromObjcNoown<CUgraphExec_t, CUgraphExec>(objc_graph_exec);
}

/**
 * @copydoc orteaf::internal::backend::cuda::graphLaunch
 */
void graphLaunch(CUgraphExec_t graph_exec, CUstream_t stream) {
    if (graph_exec == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "graphLaunch: graph_exec cannot be nullptr");
    }
    if (stream == nullptr) {
        using namespace orteaf::internal::diagnostics::error;
        throwError(OrteafErrc::NullPointer, "graphLaunch: stream cannot be nullptr");
    }
    CUgraphExec objc_graph_exec = objcFromOpaqueNoown<CUgraphExec>(graph_exec);
    CUstream objc_stream = objcFromOpaqueNoown<CUstream>(stream);
    CU_CHECK(cuGraphLaunch(objc_graph_exec, objc_stream));
}

} // namespace orteaf::internal::backend::cuda

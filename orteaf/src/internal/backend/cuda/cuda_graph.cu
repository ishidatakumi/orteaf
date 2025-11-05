#include "orteaf/internal/backend/cuda/cuda_graph.h"
#include "orteaf/internal/backend/cuda/cuda_check.h"
#include "orteaf/internal/backend/cuda/cuda_objc_bridge.h"

#ifdef CUDA_AVAILABLE
#include <cuda.h>
#endif

namespace orteaf::internal::backend::cuda {

CUgraph_t create_graph() {
#ifdef CUDA_AVAILABLE
    CUgraph graph;
    CU_CHECK(cuGraphCreate(&graph, 0));
    return opaque_from_objc_noown<CUgraph_t, CUgraph>(graph);
#else
    return nullptr;
#endif
}

CUgraphExec_t create_graph_exec(CUgraph_t graph) {
#ifdef CUDA_AVAILABLE
    CUgraph objc_graph = objc_from_opaque_noown<CUgraph>(graph);
    CUgraphExec graph_exec;
    CU_CHECK(cuGraphInstantiate(&graph_exec, objc_graph, nullptr, nullptr, 0));
    return opaque_from_objc_noown<CUgraphExec_t, CUgraphExec>(graph_exec);
#else
    (void)graph;
    return nullptr;
#endif
}

void destroy_graph(CUgraph_t graph) {
#ifdef CUDA_AVAILABLE
    CUgraph objc_graph = objc_from_opaque_noown<CUgraph>(graph);
    CU_CHECK(cuGraphDestroy(objc_graph));
#else
    (void)graph;
#endif
}

void destroy_graph_exec(CUgraphExec_t graph_exec) {
#ifdef CUDA_AVAILABLE
    CUgraphExec objc_graph_exec = objc_from_opaque_noown<CUgraphExec>(graph_exec);
    CU_CHECK(cuGraphExecDestroy(objc_graph_exec));
#else
    (void)graph_exec;
#endif
}

void begin_graph_capture(CUstream_t stream) {
#ifdef CUDA_AVAILABLE
    CUstream objc_stream = objc_from_opaque_noown<CUstream>(stream);
    CU_CHECK(cuStreamBeginCapture(objc_stream, CU_STREAM_CAPTURE_MODE_GLOBAL));
#else
    (void)stream;
#endif
}

void end_graph_capture(CUstream_t stream, CUgraph_t* graph) {
#ifdef CUDA_AVAILABLE
    CUstream objc_stream = objc_from_opaque_noown<CUstream>(stream);
    CUgraph objc_graph;
    CU_CHECK(cuStreamEndCapture(objc_stream, &objc_graph));
    *graph = opaque_from_objc_noown<CUgraph_t, CUgraph>(objc_graph);
#else
    (void)stream;
    (void)graph;
#endif
}

void instantiate_graph(CUgraph_t graph, CUgraphExec_t* graph_exec) {
#ifdef CUDA_AVAILABLE
    CUgraph objc_graph = objc_from_opaque_noown<CUgraph>(graph);
    CUgraphExec objc_graph_exec;
    CU_CHECK(cuGraphInstantiate(&objc_graph_exec, objc_graph, nullptr, nullptr, 0));
    *graph_exec = opaque_from_objc_noown<CUgraphExec_t, CUgraphExec>(objc_graph_exec);
#else
    (void)graph;
    (void)graph_exec;
#endif
}

void graph_launch(CUgraphExec_t graph_exec, CUstream_t stream) {
#ifdef CUDA_AVAILABLE
    CUgraphExec objc_graph_exec = objc_from_opaque_noown<CUgraphExec>(graph_exec);
    CUstream objc_stream = objc_from_opaque_noown<CUstream>(stream);
    CU_CHECK(cuGraphLaunch(objc_graph_exec, objc_stream));
#else
    (void)graph_exec;
    (void)stream;
#endif
}

} // namespace orteaf::internal::backend::cuda

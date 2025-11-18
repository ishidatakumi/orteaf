/**
 * @file cuda_graph.h
 * @brief CUDA Graph creation, capture, instantiation, and launch helpers.
 *
 * Thin wrappers over CUDA Driver API for CUDA Graph workflows. When CUDA is
 * disabled, functions are available but behave as no-ops and return neutral
 * values (e.g., nullptr) where applicable.
 */
#pragma once

#if ORTEAF_ENABLE_CUDA

#include "orteaf/internal/backend/cuda/cuda_stream.h"

namespace orteaf::internal::backend::cuda {

struct CUgraph_st;
using CUgraph_t = CUgraph_st*;

struct CUgraphExec_st;
using CUgraphExec_t = CUgraphExec_st*;

/**
 * @brief Create an empty CUDA graph.
 * @return Opaque graph handle, or nullptr when CUDA is disabled.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
CUgraph_t createGraph();

/**
 * @brief Create an executable instance from a graph.
 * @param graph Opaque graph handle
 * @return Opaque executable graph, or nullptr when CUDA is disabled.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
CUgraphExec_t createGraphExec(CUgraph_t graph);

/**
 * @brief Destroy a graph.
 * @param graph Opaque graph handle (ignored if nullptr)
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void destroyGraph(CUgraph_t graph);

/**
 * @brief Destroy an executable graph instance.
 * @param graph_exec Opaque graph exec handle (ignored if nullptr)
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void destroyGraphExec(CUgraphExec_t graph_exec);

/**
 * @brief Begin capturing operations on a stream into a graph (global mode).
 * @param stream Opaque stream handle
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void beginGraphCapture(CUstream_t stream);

/**
 * @brief End capture and return the captured graph.
 * @param stream Opaque stream handle
 * @param graph Out parameter for the captured opaque graph handle
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void endGraphCapture(CUstream_t stream, CUgraph_t* graph);

/**
 * @brief Instantiate an executable graph from a graph.
 * @param graph Opaque graph handle
 * @param graph_exec Out parameter for opaque executable graph handle
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void instantiateGraph(CUgraph_t graph, CUgraphExec_t* graph_exec);

/**
 * @brief Launch an executable graph on a stream.
 * @param graph_exec Opaque executable graph handle
 * @param stream Opaque stream handle
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void graphLaunch(CUgraphExec_t graph_exec, CUstream_t stream);

} // namespace orteaf::internal::backend::cuda

#endif  // ORTEAF_ENABLE_CUDA
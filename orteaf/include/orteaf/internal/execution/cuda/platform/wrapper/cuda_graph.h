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

#include "orteaf/internal/execution/cuda/platform/wrapper/cuda_stream.h"

namespace orteaf::internal::execution::cuda::platform::wrapper {

/**
 * @brief Create an empty CUDA graph.
 * @return Opaque graph handle, or nullptr when CUDA is disabled.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
CudaGraph_t createGraph();

/**
 * @brief Create an executable instance from a graph.
 * @param graph Opaque graph handle
 * @return Opaque executable graph, or nullptr when CUDA is disabled.
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
CudaGraphExec_t createGraphExec(CudaGraph_t graph);

/**
 * @brief Destroy a graph.
 * @param graph Opaque graph handle (ignored if nullptr)
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void destroyGraph(CudaGraph_t graph);

/**
 * @brief Destroy an executable graph instance.
 * @param graph_exec Opaque graph exec handle (ignored if nullptr)
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void destroyGraphExec(CudaGraphExec_t graph_exec);

/**
 * @brief Begin capturing operations on a stream into a graph (global mode).
 * @param stream Opaque stream handle
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void beginGraphCapture(CudaStream_t stream);

/**
 * @brief End capture and return the captured graph.
 * @param stream Opaque stream handle
 * @param graph Out parameter for the captured opaque graph handle
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void endGraphCapture(CudaStream_t stream, CudaGraph_t* graph);

/**
 * @brief Instantiate an executable graph from a graph.
 * @param graph Opaque graph handle
 * @param graph_exec Out parameter for opaque executable graph handle
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void instantiateGraph(CudaGraph_t graph, CudaGraphExec_t* graph_exec);

/**
 * @brief Launch an executable graph on a stream.
 * @param graph_exec Opaque executable graph handle
 * @param stream Opaque stream handle
 * @throws std::system_error On CUDA driver error (via `OrteafErrc`).
 */
void graphLaunch(CudaGraphExec_t graph_exec, CudaStream_t stream);

} // namespace orteaf::internal::execution::cuda::platform::wrapper

#endif  // ORTEAF_ENABLE_CUDA

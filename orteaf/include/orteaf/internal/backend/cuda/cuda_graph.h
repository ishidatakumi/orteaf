#pragma once

#include "orteaf/internal/backend/cuda/cuda_stream.h"

struct CUgraph_st;
using CUgraph_t = CUgraph_st*;

struct CUgraphExec_st;
using CUgraphExec_t = CUgraphExec_st*;

namespace orteaf::internal::backend::cuda {

CUgraph_t create_graph();
CUgraphExec_t create_graph_exec(CUgraph_t graph);
void destroy_graph(CUgraph_t graph);
void destroy_graph_exec(CUgraphExec_t graph_exec);

void begin_graph_capture(CUstream_t stream);
void end_graph_capture(CUstream_t stream, CUgraph_t* graph);
void instantiate_graph(CUgraph_t graph, CUgraphExec_t* graph_exec);

void graph_launch(CUgraphExec_t graph_exec, CUstream_t stream);

} // namespace orteaf::internal::backend::cuda
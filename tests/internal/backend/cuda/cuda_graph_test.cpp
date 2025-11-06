/**
 * @file cuda_graph_test.cpp
 * @brief Tests for CUDA Graph creation, capture, instantiation, and launch.
 */

#include "orteaf/internal/backend/cuda/cuda_graph.h"
#include "orteaf/internal/backend/cuda/cuda_init.h"
#include "orteaf/internal/backend/cuda/cuda_device.h"
#include "orteaf/internal/backend/cuda/cuda_context.h"
#include "orteaf/internal/backend/cuda/cuda_stream.h"

#include <gtest/gtest.h>

namespace cuda = orteaf::internal::backend::cuda;

#ifdef ORTEAF_ENABLE_CUDA

/**
 * @brief Test fixture that initializes CUDA and sets up a device and context.
 */
class CudaGraphTest : public ::testing::Test {
protected:
    void SetUp() override {
        cuda::cuda_init();
        int count = cuda::get_device_count();
        if (count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        device_ = cuda::get_device(0);
        context_ = cuda::get_primary_context(device_);
        cuda::set_context(context_);
    }
    
    void TearDown() override {
        if (context_ != nullptr) {
            cuda::release_primary_context(device_);
        }
    }
    
    cuda::CUdevice_t device_ = 0;
    cuda::CUcontext_t context_ = nullptr;
};

/**
 * @brief Test that graph creation succeeds.
 */
TEST_F(CudaGraphTest, CreateGraphSucceeds) {
    cuda::CUgraph_t graph = cuda::create_graph();
    EXPECT_NE(graph, nullptr);
    
    cuda::destroy_graph(graph);
}

/**
 * @brief Test that multiple graphs can be created.
 */
TEST_F(CudaGraphTest, CreateMultipleGraphs) {
    cuda::CUgraph_t graph1 = cuda::create_graph();
    cuda::CUgraph_t graph2 = cuda::create_graph();
    EXPECT_NE(graph1, nullptr);
    EXPECT_NE(graph2, nullptr);
    EXPECT_NE(graph1, graph2);
    
    cuda::destroy_graph(graph1);
    cuda::destroy_graph(graph2);
}

/**
 * @brief Test that destroy_graph works.
 */
TEST_F(CudaGraphTest, DestroyGraphSucceeds) {
    cuda::CUgraph_t graph = cuda::create_graph();
    EXPECT_NO_THROW(cuda::destroy_graph(graph));
}

/**
 * @brief Test that destroy_graph with nullptr is handled.
 */
TEST_F(CudaGraphTest, DestroyGraphNullptr) {
    // Implementation may throw or ignore nullptr
    try {
        cuda::destroy_graph(nullptr);
    } catch (const std::system_error&) {
        // Exception is acceptable
    }
}

/**
 * @brief Test that create_graph_exec succeeds with empty graph.
 */
TEST_F(CudaGraphTest, CreateGraphExecSucceeds) {
    cuda::CUgraph_t graph = cuda::create_graph();
    cuda::CUgraphExec_t graph_exec = cuda::create_graph_exec(graph);
    EXPECT_NE(graph_exec, nullptr);
    
    cuda::destroy_graph_exec(graph_exec);
    cuda::destroy_graph(graph);
}

/**
 * @brief Test that create_graph_exec with nullptr throws.
 */
TEST_F(CudaGraphTest, CreateGraphExecNullptrThrows) {
    EXPECT_THROW(cuda::create_graph_exec(nullptr), std::system_error);
}

/**
 * @brief Test that destroy_graph_exec works.
 */
TEST_F(CudaGraphTest, DestroyGraphExecSucceeds) {
    cuda::CUgraph_t graph = cuda::create_graph();
    cuda::CUgraphExec_t graph_exec = cuda::create_graph_exec(graph);
    
    EXPECT_NO_THROW(cuda::destroy_graph_exec(graph_exec));
    cuda::destroy_graph(graph);
}

/**
 * @brief Test that destroy_graph_exec with nullptr is handled.
 */
TEST_F(CudaGraphTest, DestroyGraphExecNullptr) {
    // Implementation may throw or ignore nullptr
    try {
        cuda::destroy_graph_exec(nullptr);
    } catch (const std::system_error&) {
        // Exception is acceptable
    }
}

/**
 * @brief Test that begin_graph_capture succeeds.
 */
TEST_F(CudaGraphTest, BeginGraphCaptureSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    EXPECT_NO_THROW(cuda::begin_graph_capture(stream));
    
    // End capture immediately (empty graph)
    cuda::CUgraph_t captured_graph = nullptr;
    EXPECT_NO_THROW(cuda::end_graph_capture(stream, &captured_graph));
    EXPECT_NE(captured_graph, nullptr);
    
    cuda::destroy_graph(captured_graph);
    cuda::release_stream(stream);
}

/**
 * @brief Test that begin_graph_capture with nullptr stream throws.
 */
TEST_F(CudaGraphTest, BeginGraphCaptureNullptrThrows) {
    EXPECT_THROW(cuda::begin_graph_capture(nullptr), std::system_error);
}

/**
 * @brief Test that end_graph_capture succeeds with empty capture.
 */
TEST_F(CudaGraphTest, EndGraphCaptureSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    cuda::begin_graph_capture(stream);
    
    cuda::CUgraph_t graph = nullptr;
    EXPECT_NO_THROW(cuda::end_graph_capture(stream, &graph));
    EXPECT_NE(graph, nullptr);
    
    cuda::destroy_graph(graph);
    cuda::release_stream(stream);
}

/**
 * @brief Test that end_graph_capture without begin throws.
 */
TEST_F(CudaGraphTest, EndGraphCaptureWithoutBeginThrows) {
    cuda::CUstream_t stream = cuda::get_stream();
    cuda::CUgraph_t graph = nullptr;
    
    EXPECT_THROW(cuda::end_graph_capture(stream, &graph), std::system_error);
    
    cuda::release_stream(stream);
}

/**
 * @brief Test that end_graph_capture with nullptr stream throws.
 */
TEST_F(CudaGraphTest, EndGraphCaptureNullptrStreamThrows) {
    cuda::CUgraph_t graph = nullptr;
    EXPECT_THROW(cuda::end_graph_capture(nullptr, &graph), std::system_error);
}

/**
 * @brief Test that instantiate_graph succeeds.
 */
TEST_F(CudaGraphTest, InstantiateGraphSucceeds) {
    cuda::CUgraph_t graph = cuda::create_graph();
    cuda::CUgraphExec_t graph_exec = nullptr;
    
    EXPECT_NO_THROW(cuda::instantiate_graph(graph, &graph_exec));
    EXPECT_NE(graph_exec, nullptr);
    
    cuda::destroy_graph_exec(graph_exec);
    cuda::destroy_graph(graph);
}

/**
 * @brief Test that instantiate_graph with nullptr graph throws.
 */
TEST_F(CudaGraphTest, InstantiateGraphNullptrThrows) {
    cuda::CUgraphExec_t graph_exec = nullptr;
    EXPECT_THROW(cuda::instantiate_graph(nullptr, &graph_exec), std::system_error);
}

/**
 * @brief Test that instantiate_graph with nullptr out parameter throws.
 */
TEST_F(CudaGraphTest, InstantiateGraphNullptrOutParamThrows) {
    cuda::CUgraph_t graph = cuda::create_graph();
    EXPECT_THROW(cuda::instantiate_graph(graph, nullptr), std::system_error);
    cuda::destroy_graph(graph);
}

/**
 * @brief Test that graph_launch succeeds with empty graph.
 */
TEST_F(CudaGraphTest, GraphLaunchSucceeds) {
    cuda::CUstream_t stream = cuda::get_stream();
    cuda::CUgraph_t graph = cuda::create_graph();
    cuda::CUgraphExec_t graph_exec = cuda::create_graph_exec(graph);
    
    EXPECT_NO_THROW(cuda::graph_launch(graph_exec, stream));
    cuda::synchronize_stream(stream);
    
    cuda::destroy_graph_exec(graph_exec);
    cuda::destroy_graph(graph);
    cuda::release_stream(stream);
}

/**
 * @brief Test that graph_launch with nullptr graph_exec throws.
 */
TEST_F(CudaGraphTest, GraphLaunchNullptrGraphExecThrows) {
    cuda::CUstream_t stream = cuda::get_stream();
    EXPECT_THROW(cuda::graph_launch(nullptr, stream), std::system_error);
    cuda::release_stream(stream);
}

/**
 * @brief Test that graph_launch with nullptr stream throws.
 */
TEST_F(CudaGraphTest, GraphLaunchNullptrStreamThrows) {
    cuda::CUgraph_t graph = cuda::create_graph();
    cuda::CUgraphExec_t graph_exec = cuda::create_graph_exec(graph);
    
    EXPECT_THROW(cuda::graph_launch(graph_exec, nullptr), std::system_error);
    
    cuda::destroy_graph_exec(graph_exec);
    cuda::destroy_graph(graph);
}

/**
 * @brief Test complete graph lifecycle (create, instantiate, launch, destroy).
 */
TEST_F(CudaGraphTest, CompleteGraphLifecycle) {
    cuda::CUstream_t stream = cuda::get_stream();
    
    // Create empty graph
    cuda::CUgraph_t graph = cuda::create_graph();
    EXPECT_NE(graph, nullptr);
    
    // Instantiate graph
    cuda::CUgraphExec_t graph_exec = nullptr;
    cuda::instantiate_graph(graph, &graph_exec);
    EXPECT_NE(graph_exec, nullptr);
    
    // Launch graph
    cuda::graph_launch(graph_exec, stream);
    cuda::synchronize_stream(stream);
    
    // Destroy graph exec
    cuda::destroy_graph_exec(graph_exec);
    
    // Destroy graph
    cuda::destroy_graph(graph);
    
    cuda::release_stream(stream);
}

/**
 * @brief Test graph capture lifecycle (begin, end, instantiate, launch).
 */
TEST_F(CudaGraphTest, GraphCaptureLifecycle) {
    cuda::CUstream_t stream = cuda::get_stream();
    
    // Begin capture
    cuda::begin_graph_capture(stream);
    
    // End capture (empty graph)
    cuda::CUgraph_t captured_graph = nullptr;
    cuda::end_graph_capture(stream, &captured_graph);
    EXPECT_NE(captured_graph, nullptr);
    
    // Instantiate captured graph
    cuda::CUgraphExec_t graph_exec = nullptr;
    cuda::instantiate_graph(captured_graph, &graph_exec);
    EXPECT_NE(graph_exec, nullptr);
    
    // Launch graph
    cuda::graph_launch(graph_exec, stream);
    cuda::synchronize_stream(stream);
    
    // Cleanup
    cuda::destroy_graph_exec(graph_exec);
    cuda::destroy_graph(captured_graph);
    cuda::release_stream(stream);
}

/**
 * @brief Test that multiple graph captures can be performed.
 */
TEST_F(CudaGraphTest, MultipleGraphCaptures) {
    cuda::CUstream_t stream = cuda::get_stream();
    
    // First capture
    cuda::begin_graph_capture(stream);
    cuda::CUgraph_t graph1 = nullptr;
    cuda::end_graph_capture(stream, &graph1);
    EXPECT_NE(graph1, nullptr);
    
    // Second capture
    cuda::begin_graph_capture(stream);
    cuda::CUgraph_t graph2 = nullptr;
    cuda::end_graph_capture(stream, &graph2);
    EXPECT_NE(graph2, nullptr);
    
    cuda::destroy_graph(graph1);
    cuda::destroy_graph(graph2);
    cuda::release_stream(stream);
}

#else  // !ORTEAF_ENABLE_CUDA

/**
 * @brief Test that graph functions return nullptr when CUDA is disabled.
 */
TEST(CudaGraph, DisabledReturnsNeutralValues) {
    EXPECT_EQ(cuda::create_graph(), nullptr);
    EXPECT_EQ(cuda::create_graph_exec(nullptr), nullptr);
    
    EXPECT_NO_THROW(cuda::destroy_graph(nullptr));
    EXPECT_NO_THROW(cuda::destroy_graph_exec(nullptr));
    EXPECT_NO_THROW(cuda::begin_graph_capture(nullptr));
    
    cuda::CUgraph_t graph = nullptr;
    EXPECT_NO_THROW(cuda::end_graph_capture(nullptr, &graph));
    
    cuda::CUgraphExec_t graph_exec = nullptr;
    EXPECT_NO_THROW(cuda::instantiate_graph(nullptr, &graph_exec));
    EXPECT_NO_THROW(cuda::graph_launch(nullptr, nullptr));
}

#endif  // ORTEAF_ENABLE_CUDA

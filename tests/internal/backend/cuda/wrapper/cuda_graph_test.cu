/**
 * @file cuda_graph_test.cpp
 * @brief Tests for CUDA Graph creation, capture, instantiation, and launch.
 */

#include "orteaf/internal/backend/cuda/wrapper/cuda_graph.h"
#include "orteaf/internal/backend/cuda/wrapper/cuda_init.h"
#include "orteaf/internal/backend/cuda/wrapper/cuda_device.h"
#include "orteaf/internal/backend/cuda/wrapper/cuda_context.h"
#include "orteaf/internal/backend/cuda/wrapper/cuda_stream.h"
#include "tests/internal/testing/error_assert.h"

#include <gtest/gtest.h>

namespace cuda = orteaf::internal::backend::cuda;

/**
 * @brief Test fixture that initializes CUDA and sets up a device and context.
 */
class CudaGraphTest : public ::testing::Test {
protected:
    void SetUp() override {
        cuda::cudaInit();
        int count = cuda::getDeviceCount();
        if (count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        device_ = cuda::getDevice(0);
        context_ = cuda::getPrimaryContext(device_);
        cuda::setContext(context_);
    }
    
    void TearDown() override {
        if (context_ != nullptr) {
            cuda::releasePrimaryContext(device_);
        }
    }
    
    cuda::CUdevice_t device_{0};
    cuda::CUcontext_t context_ = nullptr;
};

/**
 * @brief Test that graph creation succeeds.
 */
TEST_F(CudaGraphTest, CreateGraphSucceeds) {
    cuda::CUgraph_t graph = cuda::createGraph();
    EXPECT_NE(graph, nullptr);
    
    cuda::destroyGraph(graph);
}

/**
 * @brief Test that multiple graphs can be created.
 */
TEST_F(CudaGraphTest, CreateMultipleGraphs) {
    cuda::CUgraph_t graph1 = cuda::createGraph();
    cuda::CUgraph_t graph2 = cuda::createGraph();
    EXPECT_NE(graph1, nullptr);
    EXPECT_NE(graph2, nullptr);
    EXPECT_NE(graph1, graph2);
    
    cuda::destroyGraph(graph1);
    cuda::destroyGraph(graph2);
}

/**
 * @brief Test that destroy_graph works.
 */
TEST_F(CudaGraphTest, DestroyGraphSucceeds) {
    cuda::CUgraph_t graph = cuda::createGraph();
    EXPECT_NO_THROW(cuda::destroyGraph(graph));
}

/**
 * @brief Test that destroy_graph with nullptr is handled.
 */
TEST_F(CudaGraphTest, DestroyGraphNullptr) {
    EXPECT_NO_THROW(cuda::destroyGraph(nullptr));
}

/**
 * @brief Test that create_graph_exec succeeds with empty graph.
 */
TEST_F(CudaGraphTest, CreateGraphExecSucceeds) {
    cuda::CUgraph_t graph = cuda::createGraph();
    cuda::CUgraphExec_t graph_exec = cuda::createGraphExec(graph);
    EXPECT_NE(graph_exec, nullptr);
    
    cuda::destroyGraphExec(graph_exec);
    cuda::destroyGraph(graph);
}

/**
 * @brief Test that create_graph_exec with nullptr throws.
 */
TEST_F(CudaGraphTest, CreateGraphExecNullptrThrows) {
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [] { cuda::createGraphExec(nullptr); });
}

/**
 * @brief Test that destroy_graph_exec works.
 */
TEST_F(CudaGraphTest, DestroyGraphExecSucceeds) {
    cuda::CUgraph_t graph = cuda::createGraph();
    cuda::CUgraphExec_t graph_exec = cuda::createGraphExec(graph);
    
    EXPECT_NO_THROW(cuda::destroyGraphExec(graph_exec));
    cuda::destroyGraph(graph);
}

/**
 * @brief Test that destroy_graph_exec with nullptr is handled.
 */
TEST_F(CudaGraphTest, DestroyGraphExecNullptr) {
    EXPECT_NO_THROW(cuda::destroyGraphExec(nullptr));
}

/**
 * @brief Test that begin_graph_capture succeeds.
 */
TEST_F(CudaGraphTest, BeginGraphCaptureSucceeds) {
    cuda::CUstream_t stream = cuda::getStream();
    EXPECT_NO_THROW(cuda::beginGraphCapture(stream));
    
    // End capture immediately (empty graph)
    cuda::CUgraph_t captured_graph = nullptr;
    EXPECT_NO_THROW(cuda::endGraphCapture(stream, &captured_graph));
    EXPECT_NE(captured_graph, nullptr);
    
    cuda::destroyGraph(captured_graph);
    cuda::releaseStream(stream);
}

/**
 * @brief Test that begin_graph_capture with nullptr stream throws.
 */
TEST_F(CudaGraphTest, BeginGraphCaptureNullptrThrows) {
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [] { cuda::beginGraphCapture(nullptr); });
}

/**
 * @brief 
 * 
 */
TEST_F(CudaGraphTest, EndGraphCaptureSucceeds) {
    cuda::CUstream_t stream = cuda::getStream();
    cuda::beginGraphCapture(stream);
    
    cuda::CUgraph_t graph = nullptr;
    EXPECT_NO_THROW(cuda::endGraphCapture(stream, &graph));
    EXPECT_NE(graph, nullptr);
    
    cuda::destroyGraph(graph);
    cuda::releaseStream(stream);
}

/**
 * @brief Test that end_graph_capture without begin throws.
 */
TEST_F(CudaGraphTest, EndGraphCaptureWithoutBeginThrows) {
    cuda::CUstream_t stream = cuda::getStream();
    cuda::CUgraph_t graph = nullptr;

    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidState,
        [&] { cuda::endGraphCapture(stream, &graph); });

    cuda::releaseStream(stream);
}

/**
 * @brief Test that end_graph_capture with nullptr stream throws.
 */
TEST_F(CudaGraphTest, EndGraphCaptureNullptrStreamThrows) {
    cuda::CUgraph_t graph = nullptr;
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&] { cuda::endGraphCapture(nullptr, &graph); });
}

/**
 * @brief Test that end_graph_capture with nullptr graph throws.
 */
TEST_F(CudaGraphTest, EndGraphCaptureNullptrGraphThrows) {
    cuda::CUstream_t stream = cuda::getStream();
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&] { cuda::endGraphCapture(stream, nullptr); });
    cuda::releaseStream(stream);
}

/**
 * @brief Test that instantiate_graph succeeds.
 */
TEST_F(CudaGraphTest, InstantiateGraphSucceeds) {
    cuda::CUgraph_t graph = cuda::createGraph();
    cuda::CUgraphExec_t graph_exec = nullptr;
    
    EXPECT_NO_THROW(cuda::instantiateGraph(graph, &graph_exec));
    EXPECT_NE(graph_exec, nullptr);
    
    cuda::destroyGraphExec(graph_exec);
    cuda::destroyGraph(graph);
}

/**
 * @brief Test that instantiate_graph with nullptr graph throws.
 */
TEST_F(CudaGraphTest, InstantiateGraphNullptrThrows) {
    cuda::CUgraphExec_t graph_exec = nullptr;
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&] { cuda::instantiateGraph(nullptr, &graph_exec); });
}

/**
 * @brief Test that instantiate_graph with nullptr out parameter throws.
 */
TEST_F(CudaGraphTest, InstantiateGraphNullptrOutParamThrows) {
    cuda::CUgraph_t graph = cuda::createGraph();
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&] { cuda::instantiateGraph(graph, nullptr); });
    cuda::destroyGraph(graph);
}

/**
 * @brief Test that graph_launch succeeds with empty graph.
 */
TEST_F(CudaGraphTest, GraphLaunchSucceeds) {
    cuda::CUstream_t stream = cuda::getStream();
    cuda::CUgraph_t graph = cuda::createGraph();
    cuda::CUgraphExec_t graph_exec = cuda::createGraphExec(graph);
    
    EXPECT_NO_THROW(cuda::graphLaunch(graph_exec, stream));
    cuda::synchronizeStream(stream);
    
    cuda::destroyGraphExec(graph_exec);
    cuda::destroyGraph(graph);
    cuda::releaseStream(stream);
}

/**
 * @brief Test that graph_launch with nullptr graph_exec throws.
 */
TEST_F(CudaGraphTest, GraphLaunchNullptrGraphExecThrows) {
    cuda::CUstream_t stream = cuda::getStream();
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&] { cuda::graphLaunch(nullptr, stream); });
    cuda::releaseStream(stream);
}

/**
 * @brief Test that graph_launch with nullptr stream throws.
 */
TEST_F(CudaGraphTest, GraphLaunchNullptrStreamThrows) {
    cuda::CUgraph_t graph = cuda::createGraph();
    cuda::CUgraphExec_t graph_exec = cuda::createGraphExec(graph);
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::NullPointer,
        [&] { cuda::graphLaunch(graph_exec, nullptr); });
    cuda::destroyGraphExec(graph_exec);
    cuda::destroyGraph(graph);
}

/**
 * @brief Test complete graph lifecycle (create, instantiate, launch, destroy).
 */
TEST_F(CudaGraphTest, CompleteGraphLifecycle) {
    cuda::CUstream_t stream = cuda::getStream();
    
    // Create empty graph
    cuda::CUgraph_t graph = cuda::createGraph();
    EXPECT_NE(graph, nullptr);
    
    // Instantiate graph
    cuda::CUgraphExec_t graph_exec = nullptr;
    cuda::instantiateGraph(graph, &graph_exec);
    EXPECT_NE(graph_exec, nullptr);
    
    // Launch graph
    cuda::graphLaunch(graph_exec, stream);
    cuda::synchronizeStream(stream);
    
    // Destroy graph exec
    cuda::destroyGraphExec(graph_exec);
    
    // Destroy graph
    cuda::destroyGraph(graph);
    
    cuda::releaseStream(stream);
}

/**
 * @brief Test graph capture lifecycle (begin, end, instantiate, launch).
 */
TEST_F(CudaGraphTest, GraphCaptureLifecycle) {
    cuda::CUstream_t stream = cuda::getStream();
    
    // Begin capture
    cuda::beginGraphCapture(stream);
    
    // End capture (empty graph)
    cuda::CUgraph_t captured_graph = nullptr;
    cuda::endGraphCapture(stream, &captured_graph);
    EXPECT_NE(captured_graph, nullptr);
    
    // Instantiate captured graph
    cuda::CUgraphExec_t graph_exec = nullptr;
    cuda::instantiateGraph(captured_graph, &graph_exec);
    EXPECT_NE(graph_exec, nullptr);
    
    // Launch graph
    cuda::graphLaunch(graph_exec, stream);
    cuda::synchronizeStream(stream);
    
    // Cleanup
    cuda::destroyGraphExec(graph_exec);
    cuda::destroyGraph(captured_graph);
    cuda::releaseStream(stream);
}

/**
 * @brief Test that multiple graph captures can be performed.
 */
TEST_F(CudaGraphTest, MultipleGraphCaptures) {
    cuda::CUstream_t stream = cuda::getStream();
    
    // First capture
    cuda::beginGraphCapture(stream);
    cuda::CUgraph_t graph1 = nullptr;
    cuda::endGraphCapture(stream, &graph1);
    EXPECT_NE(graph1, nullptr);
    
    // Second capture
    cuda::beginGraphCapture(stream);
    cuda::CUgraph_t graph2 = nullptr;
    cuda::endGraphCapture(stream, &graph2);
    EXPECT_NE(graph2, nullptr);
    
    cuda::destroyGraph(graph1);
    cuda::destroyGraph(graph2);
    cuda::releaseStream(stream);
}

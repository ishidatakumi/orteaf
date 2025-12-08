#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include <orteaf/internal/runtime/mps/manager/mps_graph_manager.h>
#include <tests/internal/runtime/mps/manager/testing/backend_mock.h>
#include <tests/internal/testing/error_assert.h>

namespace backend = orteaf::internal::backend;
namespace base = orteaf::internal::base;
namespace mps_wrapper = orteaf::internal::runtime::mps::platform::wrapper;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::runtime::mps::manager;
using orteaf::tests::ExpectError;

namespace {

mps_wrapper::MPSDevice_t makeDevice(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MPSDevice_t>(value);
}

mps_wrapper::MPSGraph_t makeGraph(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MPSGraph_t>(value);
}

mps_wrapper::MPSGraphExecutable_t makeExecutable(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MPSGraphExecutable_t>(value);
}

mps_wrapper::MPSGraphTensor_t makeTensor(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MPSGraphTensor_t>(value);
}

mps_wrapper::MPSGraphTensorData_t makeTensorData(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MPSGraphTensorData_t>(value);
}

class MpsGraphManagerTest : public ::testing::Test {
protected:
  void SetUp() override { device_ = makeDevice(0xDEADBEEF); }

  void TearDown() override { manager_.shutdown(); }

  mps_rt::MpsGraphManager manager_{};
  ::testing::NiceMock<orteaf::tests::runtime::mps::MpsBackendOpsMock> mock_{};
  mps_wrapper::MPSDevice_t device_{nullptr};
};

TEST_F(MpsGraphManagerTest, AccessBeforeInitializationThrows) {
  mps_rt::GraphKey key = mps_rt::GraphKey::Named("g0");
  key.data_type = mps_wrapper::MpsGraphDataType::kFloat32;
  key.target_tensor_count = 1;
  ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
    manager_.acquire(key, [](auto, auto, auto) {
      return mps_wrapper::MPSGraphExecutable_t{};
    });
  });
}

TEST_F(MpsGraphManagerTest, AcquireCachesExecutableForSameKey) {
  mps_wrapper::MPSGraph_t graph = makeGraph(0x1010);
  mps_wrapper::MPSGraphExecutable_t exe = makeExecutable(0x2020);
  mps_wrapper::MPSGraphTensor_t target = makeTensor(0x3030);
  mps_wrapper::MPSGraphTensorData_t feed_data = makeTensorData(0x4040);

  EXPECT_CALL(mock_, createGraph()).WillOnce(::testing::Return(graph));
  EXPECT_CALL(mock_, compileGraph(graph, device_, ::testing::_, 1, ::testing::_,
                                  1, ::testing::_, 0))
      .WillOnce(::testing::Return(exe));
  EXPECT_CALL(mock_, destroyGraphExecutable(exe)).Times(1);
  EXPECT_CALL(mock_, destroyGraph(graph)).Times(1);

  manager_.initialize(device_, &mock_, /*capacity=*/1);
  mps_rt::GraphKey key = mps_rt::GraphKey::Named("g-cache");
  key.shape = {1, 2, 3};
  key.data_type = mps_wrapper::MpsGraphDataType::kFloat32;
  key.target_tensor_count = 1;

  auto compile_fn = [&](mps_wrapper::MPSGraph_t g, mps_wrapper::MPSDevice_t dev,
                        mps_rt::MpsGraphManager::SlowOps *ops) {
    mps_wrapper::MpsGraphFeed feed{target, feed_data};
    return ops->compileGraph(g, dev, &feed, 1, &target, 1, nullptr, 0);
  };

  auto lease1 = manager_.acquire(key, compile_fn);
  auto exe1 = lease1.pointer();
  manager_.release(lease1);

  auto lease2 = manager_.acquire(key, compile_fn);
  EXPECT_EQ(exe1, lease2.pointer());
  manager_.release(lease2);
}

TEST_F(MpsGraphManagerTest, DifferentKeyShapeTriggersNewCompile) {
  mps_wrapper::MPSGraph_t graph1 = makeGraph(0x1111);
  mps_wrapper::MPSGraphExecutable_t exe1 = makeExecutable(0x2222);
  mps_wrapper::MPSGraph_t graph2 = makeGraph(0x3333);
  mps_wrapper::MPSGraphExecutable_t exe2 = makeExecutable(0x4444);
  mps_wrapper::MPSGraphTensor_t target = makeTensor(0x5555);
  mps_wrapper::MPSGraphTensorData_t feed_data = makeTensorData(0x6666);

  ::testing::InSequence seq;
  EXPECT_CALL(mock_, createGraph()).WillOnce(::testing::Return(graph1));
  EXPECT_CALL(mock_, compileGraph(graph1, device_, ::testing::_, 1,
                                  ::testing::_, 1, ::testing::_, 0))
      .WillOnce(::testing::Return(exe1));
  EXPECT_CALL(mock_, createGraph()).WillOnce(::testing::Return(graph2));
  EXPECT_CALL(mock_, compileGraph(graph2, device_, ::testing::_, 1,
                                  ::testing::_, 1, ::testing::_, 0))
      .WillOnce(::testing::Return(exe2));
  // shutdown walks states in order: state0 exe/graph, then state1 exe/graph
  EXPECT_CALL(mock_, destroyGraphExecutable(exe1)).Times(1);
  EXPECT_CALL(mock_, destroyGraph(graph1)).Times(1);
  EXPECT_CALL(mock_, destroyGraphExecutable(exe2)).Times(1);
  EXPECT_CALL(mock_, destroyGraph(graph2)).Times(1);

  manager_.initialize(device_, &mock_, /*capacity=*/2);

  auto compile_fn = [&](mps_wrapper::MPSGraph_t g, mps_wrapper::MPSDevice_t dev,
                        mps_rt::MpsGraphManager::SlowOps *ops) {
    mps_wrapper::MpsGraphFeed feed{target, feed_data};
    return ops->compileGraph(g, dev, &feed, 1, &target, 1, nullptr, 0);
  };

  mps_rt::GraphKey key1 = mps_rt::GraphKey::Named("g-diff");
  key1.shape = {1, 2};
  key1.data_type = mps_wrapper::MpsGraphDataType::kFloat16;
  key1.target_tensor_count = 1;

  mps_rt::GraphKey key2 = key1;
  key2.shape = {2, 1}; // different shape -> new entry

  auto lease1 = manager_.acquire(key1, compile_fn);
  auto lease2 = manager_.acquire(key2, compile_fn);
  EXPECT_NE(lease1.pointer(), lease2.pointer());
  manager_.release(lease1);
  manager_.release(lease2);
}

TEST_F(MpsGraphManagerTest, InvalidKeyRejected) {
  manager_.initialize(device_, &mock_, /*capacity=*/1);
  mps_rt::GraphKey key = mps_rt::GraphKey::Named("");
  key.data_type = mps_wrapper::MpsGraphDataType::kFloat32;
  key.target_tensor_count = 1;
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    manager_.acquire(key, [](auto, auto, auto) {
      return mps_wrapper::MPSGraphExecutable_t{};
    });
  });
}

TEST_F(MpsGraphManagerTest, NullExecutableFromCompileThrowsAndCleansUp) {
  mps_wrapper::MPSGraph_t graph = makeGraph(0xABCD);
  EXPECT_CALL(mock_, createGraph()).WillOnce(::testing::Return(graph));
  EXPECT_CALL(mock_, destroyGraph(graph)).Times(1);

  manager_.initialize(device_, &mock_, /*capacity=*/1);
  mps_rt::GraphKey key = mps_rt::GraphKey::Named("null-exe");
  key.data_type = mps_wrapper::MpsGraphDataType::kFloat32;
  key.target_tensor_count = 1;

  auto compile_fn = [](mps_wrapper::MPSGraph_t, mps_wrapper::MPSDevice_t,
                       mps_rt::MpsGraphManager::SlowOps *) {
    return mps_wrapper::MPSGraphExecutable_t{};
  };

  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { manager_.acquire(key, compile_fn); });
}

} // namespace

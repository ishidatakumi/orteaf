#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include <orteaf/internal/execution/mps/manager/mps_graph_manager.h>
#include <tests/internal/execution/mps/manager/testing/backend_mock.h>
#include <tests/internal/testing/error_assert.h>

namespace mps_wrapper = orteaf::internal::execution::mps::platform::wrapper;
namespace diag_error = orteaf::internal::diagnostics::error;
namespace mps_rt = orteaf::internal::execution::mps::manager;
using orteaf::tests::ExpectError;

namespace {

mps_wrapper::MpsDevice_t makeDevice(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsDevice_t>(value);
}

mps_wrapper::MpsGraph_t makeGraph(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsGraph_t>(value);
}

mps_wrapper::MpsGraphExecutable_t makeExecutable(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsGraphExecutable_t>(value);
}

mps_wrapper::MpsGraphTensor_t makeTensor(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsGraphTensor_t>(value);
}

mps_wrapper::MpsGraphTensorData_t makeTensorData(std::uintptr_t value) {
  return reinterpret_cast<mps_wrapper::MpsGraphTensorData_t>(value);
}

class MpsGraphManagerTest : public ::testing::Test {
protected:
  void SetUp() override { device_ = makeDevice(0xDEADBEEF); }

  void TearDown() override { manager_.shutdown(); }

  mps_rt::MpsGraphManager manager_{};
  ::testing::NiceMock<orteaf::tests::execution::mps::MpsBackendOpsMock> mock_{};
  mps_wrapper::MpsDevice_t device_{nullptr};
};

// =============================================================================
// Initialization Tests
// =============================================================================

TEST_F(MpsGraphManagerTest, AccessBeforeInitializationThrows) {
  // Arrange
  mps_rt::GraphKey key = mps_rt::GraphKey::Named("g0");
  key.data_type = mps_wrapper::MpsGraphDataType::kFloat32;
  key.target_tensor_count = 1;

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidState, [&] {
    manager_.acquire(key, [](auto, auto, auto) {
      return mps_wrapper::MpsGraphExecutable_t{};
    });
  });
}

// =============================================================================
// Acquire/Cache Tests
// =============================================================================

TEST_F(MpsGraphManagerTest, AcquireCachesExecutableForSameKey) {
  // Arrange
  mps_wrapper::MpsGraph_t graph = makeGraph(0x1010);
  mps_wrapper::MpsGraphExecutable_t exe = makeExecutable(0x2020);
  mps_wrapper::MpsGraphTensor_t target = makeTensor(0x3030);
  mps_wrapper::MpsGraphTensorData_t feed_data = makeTensorData(0x4040);

  EXPECT_CALL(mock_, createGraph()).WillOnce(::testing::Return(graph));
  EXPECT_CALL(mock_, compileGraph(graph, device_, ::testing::_, 1, ::testing::_,
                                  1, ::testing::_, 0))
      .WillOnce(::testing::Return(exe));
  EXPECT_CALL(mock_, destroyGraphExecutable(exe)).Times(1);
  EXPECT_CALL(mock_, destroyGraph(graph)).Times(1);

  mps_rt::MpsGraphManager::Config config{};
  config.device = device_;
  config.ops = &mock_;
  config.payload_capacity = 1;
  config.control_block_capacity = 1;
  manager_.configure(config);
  mps_rt::GraphKey key = mps_rt::GraphKey::Named("g-cache");
  key.shape = {1, 2, 3};
  key.data_type = mps_wrapper::MpsGraphDataType::kFloat32;
  key.target_tensor_count = 1;

  auto compile_fn = [&](mps_wrapper::MpsGraph_t g, mps_wrapper::MpsDevice_t dev,
                        mps_rt::MpsGraphManager::SlowOps *ops) {
    mps_wrapper::MpsGraphFeed feed{target, feed_data};
    return ops->compileGraph(g, dev, &feed, 1, &target, 1, nullptr, 0);
  };

  // Act: Acquire twice with same key
  auto lease1 = manager_.acquire(key, compile_fn);
  auto *payload1 = lease1.payloadPtr();
  ASSERT_NE(payload1, nullptr);
  auto exe1 = payload1->executable;
  manager_.release(lease1);

  auto lease2 = manager_.acquire(key, compile_fn);

  // Assert: Same executable (cached)
  auto *payload2 = lease2.payloadPtr();
  ASSERT_NE(payload2, nullptr);
  EXPECT_EQ(exe1, payload2->executable);

  // Cleanup
  manager_.release(lease2);
}

TEST_F(MpsGraphManagerTest, DifferentKeyShapeTriggersNewCompile) {
  // Arrange
  mps_wrapper::MpsGraph_t graph1 = makeGraph(0x1111);
  mps_wrapper::MpsGraphExecutable_t exe1 = makeExecutable(0x2222);
  mps_wrapper::MpsGraph_t graph2 = makeGraph(0x3333);
  mps_wrapper::MpsGraphExecutable_t exe2 = makeExecutable(0x4444);
  mps_wrapper::MpsGraphTensor_t target = makeTensor(0x5555);
  mps_wrapper::MpsGraphTensorData_t feed_data = makeTensorData(0x6666);

  // Creation order is strict
  {
    ::testing::InSequence seq;
    EXPECT_CALL(mock_, createGraph()).WillOnce(::testing::Return(graph1));
    EXPECT_CALL(mock_, compileGraph(graph1, device_, ::testing::_, 1,
                                    ::testing::_, 1, ::testing::_, 0))
        .WillOnce(::testing::Return(exe1));
    EXPECT_CALL(mock_, createGraph()).WillOnce(::testing::Return(graph2));
    EXPECT_CALL(mock_, compileGraph(graph2, device_, ::testing::_, 1,
                                    ::testing::_, 1, ::testing::_, 0))
        .WillOnce(::testing::Return(exe2));
  }
  // Destroy order is not guaranteed (depends on pool iteration)
  EXPECT_CALL(mock_, destroyGraphExecutable(exe1)).Times(1);
  EXPECT_CALL(mock_, destroyGraph(graph1)).Times(1);
  EXPECT_CALL(mock_, destroyGraphExecutable(exe2)).Times(1);
  EXPECT_CALL(mock_, destroyGraph(graph2)).Times(1);

  mps_rt::MpsGraphManager::Config config{};
  config.device = device_;
  config.ops = &mock_;
  config.payload_capacity = 2;
  config.control_block_capacity = 2;
  manager_.configure(config);

  auto compile_fn = [&](mps_wrapper::MpsGraph_t g, mps_wrapper::MpsDevice_t dev,
                        mps_rt::MpsGraphManager::SlowOps *ops) {
    mps_wrapper::MpsGraphFeed feed{target, feed_data};
    return ops->compileGraph(g, dev, &feed, 1, &target, 1, nullptr, 0);
  };

  mps_rt::GraphKey key1 = mps_rt::GraphKey::Named("g-diff");
  key1.shape = {1, 2};
  key1.data_type = mps_wrapper::MpsGraphDataType::kFloat16;
  key1.target_tensor_count = 1;

  mps_rt::GraphKey key2 = key1;
  key2.shape = {2, 1}; // Different shape -> new entry

  // Act
  auto lease1 = manager_.acquire(key1, compile_fn);
  auto lease2 = manager_.acquire(key2, compile_fn);

  // Assert: Different executables
  auto *payload1 = lease1.payloadPtr();
  auto *payload2 = lease2.payloadPtr();
  ASSERT_NE(payload1, nullptr);
  ASSERT_NE(payload2, nullptr);
  EXPECT_NE(payload1->executable, payload2->executable);

  // Cleanup
  manager_.release(lease1);
  manager_.release(lease2);
}

// =============================================================================
// Validation Tests
// =============================================================================

TEST_F(MpsGraphManagerTest, InvalidKeyRejected) {
  // Arrange
  mps_rt::MpsGraphManager::Config config{};
  config.device = device_;
  config.ops = &mock_;
  config.payload_capacity = 1;
  config.control_block_capacity = 1;
  manager_.configure(config);
  mps_rt::GraphKey key = mps_rt::GraphKey::Named("");
  key.data_type = mps_wrapper::MpsGraphDataType::kFloat32;
  key.target_tensor_count = 1;

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidArgument, [&] {
    manager_.acquire(key, [](auto, auto, auto) {
      return mps_wrapper::MpsGraphExecutable_t{};
    });
  });
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(MpsGraphManagerTest, NullExecutableFromCompileThrowsAndCleansUp) {
  // Arrange
  mps_wrapper::MpsGraph_t graph = makeGraph(0xABCD);
  EXPECT_CALL(mock_, createGraph()).WillOnce(::testing::Return(graph));
  EXPECT_CALL(mock_, destroyGraph(graph)).Times(1);

  mps_rt::MpsGraphManager::Config config{};
  config.device = device_;
  config.ops = &mock_;
  config.payload_capacity = 1;
  config.control_block_capacity = 1;
  manager_.configure(config);
  mps_rt::GraphKey key = mps_rt::GraphKey::Named("null-exe");
  key.data_type = mps_wrapper::MpsGraphDataType::kFloat32;
  key.target_tensor_count = 1;

  auto compile_fn = [](mps_wrapper::MpsGraph_t, mps_wrapper::MpsDevice_t,
                       mps_rt::MpsGraphManager::SlowOps *) {
    return mps_wrapper::MpsGraphExecutable_t{};
  };

  // Act & Assert
  ExpectError(diag_error::OrteafErrc::InvalidState,
              [&] { manager_.acquire(key, compile_fn); });
}

} // namespace

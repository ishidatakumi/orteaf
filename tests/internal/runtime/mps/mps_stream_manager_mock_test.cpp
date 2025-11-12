// #include <gmock/gmock.h>
// #include <gtest/gtest.h>

// #include <cstdint>
// #include <system_error>

// #include "orteaf/internal/runtime/manager/mps/mps_stream_manager.h"
// #include "orteaf/internal/backend/mps/mps_command_queue.h"
// #include "orteaf/internal/base/strong_id.h"
// #include "tests/internal/runtime/mps/testing/backend_mock.h"

// namespace backend = orteaf::internal::backend;
// namespace base = orteaf::internal::base;
// namespace mps_rt = orteaf::internal::runtime::mps;
// namespace test_mps = orteaf::tests::runtime::mps;

// using ::testing::_;
// using ::testing::NiceMock;
// using ::testing::Return;

// namespace {

// using MockMpsStreamManager = mps_rt::MpsStreamManager<test_mps::MpsBackendOpsMockAdapter>;

// backend::mps::MPSCommandQueue_t makeFakeStream(std::uintptr_t value) {
//     return reinterpret_cast<backend::mps::MPSCommandQueue_t>(value);
// }

// }  // namespace

// class MpsStreamManagerMockTest : public ::testing::Test {
// protected:
//     MpsStreamManagerMockTest()
//         : guard_(mock_) {}

//     void TearDown() override {
//         manager_.shutdown();
//         test_mps::MpsBackendOpsMockRegistry::unbind(mock_);
//     }

//     NiceMock<test_mps::MpsBackendOpsMock> mock_;
//     test_mps::MpsBackendOpsMockRegistry::Guard guard_;
//     MockMpsStreamManager manager_;
// };

// TEST_F(MpsStreamManagerMockTest, InitializesWithCapacity) {
//     const std::size_t initial_capacity = 8;
    
//     manager_.initialize(initial_capacity);
//     EXPECT_EQ(manager_.capacity(), initial_capacity);
    
//     manager_.shutdown();
//     EXPECT_EQ(manager_.capacity(), 0u);
// }

// TEST_F(MpsStreamManagerMockTest, ZeroCapacityStillMarksInitialized) {
//     manager_.initialize(0);
//     EXPECT_EQ(manager_.capacity(), 0u);
    
//     manager_.shutdown();
// }

// TEST_F(MpsStreamManagerMockTest, ReinitializeResetsCapacity) {
//     {
//         ::testing::InSequence seq;
//         // First initialization
//         // Second initialization should reset
//     }
    
//     manager_.initialize(4);
//     EXPECT_EQ(manager_.capacity(), 4u);
    
//     manager_.initialize(8);
//     EXPECT_EQ(manager_.capacity(), 8u);
    
//     manager_.shutdown();
// }

// TEST_F(MpsStreamManagerMockTest, AccessBeforeInitializationThrows) {
//     EXPECT_THROW(manager_.getStream(base::StreamId{0}), std::system_error);
//     EXPECT_FALSE(manager_.isAlive(base::StreamId{0}));
// }

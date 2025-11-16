#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <limits>
#include <memory>

#include "orteaf/internal/runtime/manager/mps/mps_command_queue_manager.h"
#include "tests/internal/runtime/mps/testing/backend_mock.h"

namespace backend = orteaf::internal::backend;
namespace base = orteaf::internal::base;
namespace mps_rt = orteaf::internal::runtime::mps;
namespace test_mps = orteaf::tests::runtime::mps;

using ::testing::NiceMock;
using ::testing::_;

namespace {

backend::mps::MPSCommandQueue_t makeQueue(std::uintptr_t value) {
    return reinterpret_cast<backend::mps::MPSCommandQueue_t>(value);
}

backend::mps::MPSEvent_t makeEvent(std::uintptr_t value) {
    return reinterpret_cast<backend::mps::MPSEvent_t>(value);
}

struct MockManagerProvider {
    using Manager = mps_rt::MpsCommandQueueManager<test_mps::MpsBackendOpsMockAdapter>;

    void setUp() {
        mock_ = std::make_unique<NiceMock<test_mps::MpsBackendOpsMock>>();
        guard_ = std::make_unique<test_mps::MpsBackendOpsMockRegistry::Guard>(*mock_);

        using ::testing::Invoke;
        ON_CALL(*mock_, createCommandQueue(_)).WillByDefault(Invoke([this](auto) {
            return makeQueue(next_queue_++);
        }));
        ON_CALL(*mock_, createEvent(_)).WillByDefault(Invoke([this](auto) {
            return makeEvent(next_event_++);
        }));
    }

    void tearDown() {
        if (mock_) {
            test_mps::MpsBackendOpsMockRegistry::unbind(*mock_);
        }
        guard_.reset();
        mock_.reset();
    }

private:
    std::unique_ptr<NiceMock<test_mps::MpsBackendOpsMock>> mock_;
    std::unique_ptr<test_mps::MpsBackendOpsMockRegistry::Guard> guard_;
    std::uintptr_t next_queue_{0xA000};
    std::uintptr_t next_event_{0xB000};
};

struct RealManagerProvider {
    using Manager = mps_rt::MpsCommandQueueManager<>;
    void setUp() {}
    void tearDown() {}
};

template <class Provider>
class MpsCommandQueueManagerTypedTest : public ::testing::Test {
protected:
    using Manager = typename Provider::Manager;

    void SetUp() override {
        provider_.setUp();
    }

    void TearDown() override {
        manager_.shutdown();
        provider_.tearDown();
    }

    Manager manager_;
    Provider provider_;
};

#if ORTEAF_ENABLE_MPS
using ManagerProviders = ::testing::Types<MockManagerProvider, RealManagerProvider>;
#else
using ManagerProviders = ::testing::Types<MockManagerProvider>;
#endif

TYPED_TEST_SUITE(MpsCommandQueueManagerTypedTest, ManagerProviders);

TYPED_TEST(MpsCommandQueueManagerTypedTest, AcquireReleaseRoundTrip) {
    auto& manager = this->manager_;
    manager.setGrowthChunkSize(1);
    manager.initialize(2);

    auto id0 = manager.acquire();
    auto id1 = manager.acquire();
    EXPECT_NE(id0, id1);

    manager.setSubmitSerial(id0, 1);
    manager.setCompletedSerial(id0, 1);
    manager.release(id0);

    manager.setSubmitSerial(id1, 2);
    manager.setCompletedSerial(id1, 2);
    manager.release(id1);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, ShutdownThenReinitialize) {
    auto& manager = this->manager_;
    manager.initialize(1);
    EXPECT_EQ(manager.capacity(), 1u);
    manager.shutdown();
    EXPECT_EQ(manager.capacity(), 0u);

    manager.initialize(1);
    auto id = manager.acquire();
    manager.setSubmitSerial(id, 3);
    manager.setCompletedSerial(id, 3);
    manager.release(id);
}

TYPED_TEST(MpsCommandQueueManagerTypedTest, HazardCountersResetAfterRelease) {
    auto& manager = this->manager_;
    manager.initialize(1);
    auto id = manager.acquire();
    manager.setSubmitSerial(id, 5);
    manager.setCompletedSerial(id, 5);
    manager.release(id);

    auto recycled = manager.acquire();
    EXPECT_EQ(manager.submitSerial(recycled), 0u);
    EXPECT_EQ(manager.completedSerial(recycled), 0u);
    manager.setCompletedSerial(recycled, 0);
    manager.release(recycled);
}

}  // namespace

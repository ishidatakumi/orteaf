#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "tests/internal/runtime/mps/testing/backend_mock_expectations.h"
#include "orteaf/internal/backend/mps/mps_device.h"

namespace orteaf::tests::runtime::mps::testing {

template <class ManagerT, class Provider>
class ManagerAdapter {
public:
    using Manager = ManagerT;
    using Context = typename Provider::Context;

    void bind(Manager& manager, Context& context) {
        manager_ = &manager;
        context_ = &context;
    }

    Manager& manager() {
        return *manager_;
    }

    const Manager& manager() const {
        return *manager_;
    }

    void expectCreateCommandQueues(
        std::initializer_list<::orteaf::internal::backend::mps::MPSCommandQueue_t> handles,
        ::testing::Matcher<::orteaf::internal::backend::mps::MPSDevice_t> matcher = ::testing::_) {
        if constexpr (Provider::is_mock) {
            auto& mock = Provider::mock(*context_);
            BackendMockExpectations::expectCreateCommandQueues(mock, handles, matcher);
        } else {
            (void)handles;
            (void)matcher;
        }
    }

    void expectCreateEvents(
        std::initializer_list<::orteaf::internal::backend::mps::MPSEvent_t> handles,
        ::testing::Matcher<::orteaf::internal::backend::mps::MPSDevice_t> matcher = ::testing::_) {
        if constexpr (Provider::is_mock) {
            auto& mock = Provider::mock(*context_);
            BackendMockExpectations::expectCreateEvents(mock, handles, matcher);
        } else {
            (void)handles;
            (void)matcher;
        }
    }

    void expectCreateLibraries(
        std::initializer_list<std::pair<std::string, ::orteaf::internal::backend::mps::MPSLibrary_t>> expectations,
        ::testing::Matcher<::orteaf::internal::backend::mps::MPSDevice_t> matcher = ::testing::_) {
        if constexpr (Provider::is_mock) {
            auto& mock = Provider::mock(*context_);
            BackendMockExpectations::expectCreateLibraries(mock, expectations, matcher);
        } else {
            (void)expectations;
            (void)matcher;
        }
    }

    void expectDestroyCommandQueues(
        std::initializer_list<::orteaf::internal::backend::mps::MPSCommandQueue_t> handles) {
        if constexpr (Provider::is_mock) {
            auto& mock = Provider::mock(*context_);
            BackendMockExpectations::expectDestroyCommandQueues(mock, handles);
        } else {
            (void)handles;
        }
    }

    void expectDestroyEvents(
        std::initializer_list<::orteaf::internal::backend::mps::MPSEvent_t> handles) {
        if constexpr (Provider::is_mock) {
            auto& mock = Provider::mock(*context_);
            BackendMockExpectations::expectDestroyEvents(mock, handles);
        } else {
            (void)handles;
        }
    }

    void expectDestroyLibraries(
        std::initializer_list<::orteaf::internal::backend::mps::MPSLibrary_t> handles) {
        if constexpr (Provider::is_mock) {
            auto& mock = Provider::mock(*context_);
            BackendMockExpectations::expectDestroyLibraries(mock, handles);
        } else {
            (void)handles;
        }
    }

    void expectDestroyCommandQueuesInOrder(
        std::initializer_list<::orteaf::internal::backend::mps::MPSCommandQueue_t> handles) {
        if constexpr (Provider::is_mock) {
            auto& mock = Provider::mock(*context_);
            BackendMockExpectations::expectDestroyCommandQueuesInOrder(mock, handles);
        } else {
            (void)handles;
        }
    }

    void expectDestroyEventsInOrder(
        std::initializer_list<::orteaf::internal::backend::mps::MPSEvent_t> handles) {
        if constexpr (Provider::is_mock) {
            auto& mock = Provider::mock(*context_);
            BackendMockExpectations::expectDestroyEventsInOrder(mock, handles);
        } else {
            (void)handles;
        }
    }

    void expectGetDeviceCount(int count) {
        if constexpr (Provider::is_mock) {
            auto& mock = Provider::mock(*context_);
            BackendMockExpectations::expectGetDeviceCount(mock, count);
        } else {
            (void)count;
        }
    }

    void expectGetDevices(
        std::initializer_list<std::pair<
            ::orteaf::internal::backend::mps::MPSInt_t,
            ::orteaf::internal::backend::mps::MPSDevice_t>> expectations) {
        if constexpr (Provider::is_mock) {
            auto& mock = Provider::mock(*context_);
            BackendMockExpectations::expectGetDevices(mock, expectations);
        } else {
            (void)expectations;
        }
    }

    void expectDetectArchitectures(
        std::initializer_list<std::pair<
            ::orteaf::internal::base::DeviceId,
            ::orteaf::internal::architecture::Architecture>> expectations) {
        if constexpr (Provider::is_mock) {
            auto& mock = Provider::mock(*context_);
            BackendMockExpectations::expectDetectArchitectures(mock, expectations);
        } else {
            (void)expectations;
        }
    }

    void expectReleaseDevices(
        std::initializer_list<::orteaf::internal::backend::mps::MPSDevice_t> devices) {
        if constexpr (Provider::is_mock) {
            auto& mock = Provider::mock(*context_);
            BackendMockExpectations::expectReleaseDevices(mock, devices);
        } else {
            (void)devices;
        }
    }

    ::orteaf::internal::backend::mps::MPSDevice_t device() {
        if (!device_initialized_) {
            acquireDeviceOrSkip();
        }
        return device_;
    }

private:
    static ::orteaf::internal::backend::mps::MPSDevice_t mockDeviceHandle() {
        return reinterpret_cast<::orteaf::internal::backend::mps::MPSDevice_t>(0xD1);
    }

    void acquireDeviceOrSkip() {
        if constexpr (Provider::is_mock) {
            auto& mock = Provider::mock(*context_);
            BackendMockExpectations::expectGetDeviceCount(mock, 1);
            BackendMockExpectations::expectGetDevices(mock, {{0, mockDeviceHandle()}});
        }
        const int count = Provider::BackendOps::getDeviceCount();
        if (count <= 0) {
            GTEST_SKIP() << "No MPS devices available";
        }
        auto acquired = Provider::BackendOps::getDevice(0);
        if (acquired == nullptr) {
            GTEST_SKIP() << "Unable to acquire MPS device";
        }
        device_ = acquired;
        device_initialized_ = true;
    }

    Manager* manager_{nullptr};
    Context* context_{nullptr};
    ::orteaf::internal::backend::mps::MPSDevice_t device_{nullptr};
    bool device_initialized_{false};
};

}  // namespace orteaf::tests::runtime::mps::testing

#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <orteaf/internal/runtime/mps/platform/wrapper/mps_device.h>
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_fence.h>
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_heap.h>
#include <tests/internal/runtime/mps/manager/testing/backend_mock_expectations.h>

namespace orteaf::tests::runtime::mps::testing {

template <class ManagerT, class Provider> class ManagerAdapter {
public:
  using Manager = ManagerT;
  using Context = typename Provider::Context;

  void bind(Manager &manager, Context &context) {
    manager_ = &manager;
    context_ = &context;
  }

  Manager &manager() { return *manager_; }

  const Manager &manager() const { return *manager_; }

  void expectCreateCommandQueues(
      std::initializer_list<::orteaf::internal::runtime::mps::platform::
                                wrapper::MpsCommandQueue_t>
          handles,
      ::testing::Matcher<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t>
          matcher = ::testing::_) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectCreateCommandQueues(mock, handles,
                                                         matcher);
    }
  }

  void expectCreateEvents(
      std::initializer_list<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsEvent_t>
          handles,
      ::testing::Matcher<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t>
          matcher = ::testing::_) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectCreateEvents(mock, handles, matcher);
    }
  }

  void expectCreateFences(
      std::initializer_list<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsFence_t>
          handles,
      ::testing::Matcher<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t>
          matcher = ::testing::_) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectCreateFences(mock, handles, matcher);
    }
  }

  void expectCreateLibraries(
      std::initializer_list<std::pair<
          std::string,
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t>>
          expectations,
      ::testing::Matcher<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t>
          matcher = ::testing::_) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectCreateLibraries(mock, expectations,
                                                     matcher);
    }
  }

  void expectCreateFunctions(
      std::initializer_list<std::pair<
          std::string,
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsFunction_t>>
          expectations,
      ::testing::Matcher<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t>
          matcher = ::testing::_) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectCreateFunctions(mock, expectations,
                                                     matcher);
    }
  }

  void expectCreateComputePipelineStates(
      std::initializer_list<std::pair<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsFunction_t,
          ::orteaf::internal::runtime::mps::platform::wrapper::
              MpsComputePipelineState_t>>
          expectations,
      ::testing::Matcher<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t>
          matcher = ::testing::_) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectCreateComputePipelineStates(
          mock, expectations, matcher);
    }
  }

  void expectDestroyCommandQueues(
      std::initializer_list<::orteaf::internal::runtime::mps::platform::
                                wrapper::MpsCommandQueue_t>
          handles) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectDestroyCommandQueues(mock, handles);
    }
  }

  void expectDestroyEvents(
      std::initializer_list<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsEvent_t>
          handles) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectDestroyEvents(mock, handles);
    }
  }

  void expectDestroyFences(
      std::initializer_list<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsFence_t>
          handles) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectDestroyFences(mock, handles);
    }
  }

  void expectDestroyLibraries(
      std::initializer_list<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsLibrary_t>
          handles) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectDestroyLibraries(mock, handles);
    }
  }

  void expectDestroyFunctions(
      std::initializer_list<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsFunction_t>
          handles) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectDestroyFunctions(mock, handles);
    }
  }

  void expectDestroyComputePipelineStates(
      std::initializer_list<::orteaf::internal::runtime::mps::platform::
                                wrapper::MpsComputePipelineState_t>
          handles) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectDestroyComputePipelineStates(mock,
                                                                  handles);
    }
  }

  void expectCreateHeaps(
      std::initializer_list<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeap_t>
          handles,
      ::testing::Matcher<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t>
          device_matcher = ::testing::_,
      ::testing::Matcher<::orteaf::internal::runtime::mps::platform::wrapper::
                             MpsHeapDescriptor_t>
          descriptor_matcher = ::testing::_) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectCreateHeaps(mock, handles, device_matcher,
                                                 descriptor_matcher);
    }
  }

  void expectDestroyHeaps(
      std::initializer_list<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeap_t>
          handles) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectDestroyHeaps(mock, handles);
    }
  }

  void expectCreateHeapsInOrder(
      std::initializer_list<std::pair<
          ::orteaf::internal::runtime::mps::platform::wrapper::
              MpsHeapDescriptor_t,
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeap_t>>
          expectations,
      ::testing::Matcher<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t>
          device_matcher = ::testing::_) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectCreateHeapsInOrder(mock, expectations,
                                                        device_matcher);
    }
  }

  void expectCreateHeapDescriptors(
      std::initializer_list<::orteaf::internal::runtime::mps::platform::
                                wrapper::MpsHeapDescriptor_t>
          handles) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectCreateHeapDescriptors(mock, handles);
    }
  }

  void expectDestroyHeapDescriptors(
      std::initializer_list<::orteaf::internal::runtime::mps::platform::
                                wrapper::MpsHeapDescriptor_t>
          handles) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectDestroyHeapDescriptors(mock, handles);
    }
  }

  void expectSetHeapDescriptorSize(
      std::initializer_list<
          std::pair<::orteaf::internal::runtime::mps::platform::wrapper::
                        MpsHeapDescriptor_t,
                    std::size_t>>
          expectations) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      ORTEAF_EXPECT_SET_HEAP_DESCRIPTOR_VALUES(mock, setHeapDescriptorSize,
                                               expectations);
    }
  }

  void expectSetHeapDescriptorResourceOptions(
      std::initializer_list<
          std::pair<::orteaf::internal::runtime::mps::platform::wrapper::
                        MpsHeapDescriptor_t,
                    ::orteaf::internal::runtime::mps::platform::wrapper::
                        MpsResourceOptions_t>>
          expectations) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      ORTEAF_EXPECT_SET_HEAP_DESCRIPTOR_VALUES(
          mock, setHeapDescriptorResourceOptions, expectations);
    }
  }

  void expectSetHeapDescriptorStorageMode(
      std::initializer_list<
          std::pair<::orteaf::internal::runtime::mps::platform::wrapper::
                        MpsHeapDescriptor_t,
                    ::orteaf::internal::runtime::mps::platform::wrapper::
                        MpsStorageMode_t>>
          expectations) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      ORTEAF_EXPECT_SET_HEAP_DESCRIPTOR_VALUES(
          mock, setHeapDescriptorStorageMode, expectations);
    }
  }

  void expectSetHeapDescriptorCPUCacheMode(
      std::initializer_list<
          std::pair<::orteaf::internal::runtime::mps::platform::wrapper::
                        MpsHeapDescriptor_t,
                    ::orteaf::internal::runtime::mps::platform::wrapper::
                        MpsCPUCacheMode_t>>
          expectations) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      ORTEAF_EXPECT_SET_HEAP_DESCRIPTOR_VALUES(
          mock, setHeapDescriptorCPUCacheMode, expectations);
    }
  }

  void expectSetHeapDescriptorHazardTrackingMode(
      std::initializer_list<
          std::pair<::orteaf::internal::runtime::mps::platform::wrapper::
                        MpsHeapDescriptor_t,
                    ::orteaf::internal::runtime::mps::platform::wrapper::
                        MpsHazardTrackingMode_t>>
          expectations) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      ORTEAF_EXPECT_SET_HEAP_DESCRIPTOR_VALUES(
          mock, setHeapDescriptorHazardTrackingMode, expectations);
    }
  }

  void expectSetHeapDescriptorType(
      std::initializer_list<std::pair<
          ::orteaf::internal::runtime::mps::platform::wrapper::
              MpsHeapDescriptor_t,
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsHeapType_t>>
          expectations) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      ORTEAF_EXPECT_SET_HEAP_DESCRIPTOR_VALUES(mock, setHeapDescriptorType,
                                               expectations);
    }
  }

  void expectDestroyCommandQueuesInOrder(
      std::initializer_list<::orteaf::internal::runtime::mps::platform::
                                wrapper::MpsCommandQueue_t>
          handles) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectDestroyCommandQueuesInOrder(mock, handles);
    }
  }

  void expectDestroyEventsInOrder(
      std::initializer_list<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsEvent_t>
          handles) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectDestroyEventsInOrder(mock, handles);
    }
  }

  void expectGetDeviceCount(int count) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectGetDeviceCount(mock, count);
    }
  }

  void expectGetDevices(
      std::initializer_list<std::pair<
          ::orteaf::internal::runtime::mps::platform::wrapper::MPSInt_t,
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t>>
          expectations) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectGetDevices(mock, expectations);
    }
  }

  void expectDetectArchitectures(
      std::initializer_list<
          std::pair<::orteaf::internal::base::DeviceHandle,
                    ::orteaf::internal::architecture::Architecture>>
          expectations) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectDetectArchitectures(mock, expectations);
    }
  }

  void expectReleaseDevices(
      std::initializer_list<
          ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t>
          devices) {
    if constexpr (Provider::is_mock) {
      auto &mock = Provider::mock(*context_);
      BackendMockExpectations::expectReleaseDevices(mock, devices);
    }
  }

  ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device() {
    if (!device_initialized_) {
      acquireDeviceOrSkip();
    }
    return device_;
  }

private:
  static ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t
  mockDeviceHandle() {
    return reinterpret_cast<
        ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t>(0xD1);
  }

  void acquireDeviceOrSkip() {
    auto *ops = Provider::getOps(*context_);
    if constexpr (Provider::is_mock) {
      BackendMockExpectations::expectGetDeviceCount(*ops, 1);
      BackendMockExpectations::expectGetDevices(*ops,
                                                {{0, mockDeviceHandle()}});
    }
    const int count = ops->getDeviceCount();
    if (count <= 0) {
      GTEST_SKIP() << "No MPS devices available";
    }
    auto acquired = ops->getDevice(0);
    if (acquired == nullptr) {
      GTEST_SKIP() << "Unable to acquire MPS device";
    }
    device_ = acquired;
    device_initialized_ = true;
  }

  Manager *manager_{nullptr};
  Context *context_{nullptr};
  ::orteaf::internal::runtime::mps::platform::wrapper::MpsDevice_t device_{
      nullptr};
  bool device_initialized_{false};
};

} // namespace orteaf::tests::runtime::mps::testing

#pragma once

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>

#include "orteaf/internal/backend/mps/mps_command_queue.h"
#include "orteaf/internal/backend/mps/mps_compute_pipeline_state.h"
#include "orteaf/internal/backend/mps/mps_device.h"
#include "orteaf/internal/backend/mps/mps_event.h"
#include "orteaf/internal/backend/mps/mps_function.h"
#include "orteaf/internal/backend/mps/mps_heap.h"
#include "orteaf/internal/backend/mps/mps_library.h"
#include "orteaf/internal/backend/mps/mps_fence.h"
#include "orteaf/internal/base/strong_id.h"
#include "orteaf/internal/architecture/architecture.h"
#include "tests/internal/runtime/mps/testing/backend_mock.h"

namespace orteaf::tests::runtime::mps {

/**
 * Helpers for expressing BackendOps expectations in tests.
 *
 * Each helper mirrors a BackendOps entry point so tests stay concise
 * while remaining explicit about expected call counts and return values.
 */
struct BackendMockExpectations {
    static void expectGetDeviceCount(MpsBackendOpsMock& mock, int value) {
        EXPECT_CALL(mock, getDeviceCount()).WillRepeatedly(::testing::Return(value));
    }

    static void expectGetDevices(
        MpsBackendOpsMock& mock,
        std::initializer_list<std::pair<::orteaf::internal::backend::mps::MPSInt_t,
                                        ::orteaf::internal::backend::mps::MPSDevice_t>> expectations) {
        for (const auto& [index, device] : expectations) {
            EXPECT_CALL(mock, getDevice(index)).WillOnce(::testing::Return(device));
        }
    }

    static void expectReleaseDevices(
        MpsBackendOpsMock& mock,
        std::initializer_list<::orteaf::internal::backend::mps::MPSDevice_t> devices) {
        if (devices.size() == 0) {
            EXPECT_CALL(mock, releaseDevice(::testing::_)).Times(0);
            return;
        }
        for (auto device : devices) {
            EXPECT_CALL(mock, releaseDevice(device)).Times(1);
        }
    }

    static void expectDetectArchitectures(
        MpsBackendOpsMock& mock,
        std::initializer_list<std::pair<::orteaf::internal::base::DeviceId,
                                        ::orteaf::internal::architecture::Architecture>> expectations) {
        for (const auto& [id, arch] : expectations) {
            EXPECT_CALL(mock, detectArchitecture(id)).WillOnce(::testing::Return(arch));
        }
    }

    static void expectCreateCommandQueues(
        MpsBackendOpsMock& mock,
        std::initializer_list<::orteaf::internal::backend::mps::MPSCommandQueue_t> handles,
        ::testing::Matcher<::orteaf::internal::backend::mps::MPSDevice_t> device_matcher = ::testing::_) {
        if (handles.size() == 0) {
            EXPECT_CALL(mock, createCommandQueue(device_matcher)).Times(0);
            return;
        }
        struct State {
            std::vector<::orteaf::internal::backend::mps::MPSCommandQueue_t> values;
            std::size_t next{0};
        };
        auto state = std::make_shared<State>();
        state->values.assign(handles.begin(), handles.end());
        const auto call_count = state->values.size();
        EXPECT_CALL(mock, createCommandQueue(device_matcher))
            .Times(call_count)
            .WillRepeatedly(::testing::Invoke(
                [state](::orteaf::internal::backend::mps::MPSDevice_t /*device*/) mutable {
                    const auto handle = state->values[state->next];
                    ++state->next;
                    return handle;
                }));
    }

    static void expectDestroyCommandQueues(
        MpsBackendOpsMock& mock,
        std::initializer_list<::orteaf::internal::backend::mps::MPSCommandQueue_t> handles) {
        if (handles.size() == 0) {
            EXPECT_CALL(mock, destroyCommandQueue(::testing::_)).Times(0);
            return;
        }
        for (auto handle : handles) {
            EXPECT_CALL(mock, destroyCommandQueue(handle)).Times(1);
        }
    }

    static void expectDestroyCommandQueuesInOrder(
        MpsBackendOpsMock& mock,
        std::initializer_list<::orteaf::internal::backend::mps::MPSCommandQueue_t> handles) {
        if (handles.size() == 0) {
            EXPECT_CALL(mock, destroyCommandQueue(::testing::_)).Times(0);
            return;
        }
        ::testing::InSequence seq;
        for (auto handle : handles) {
            EXPECT_CALL(mock, destroyCommandQueue(handle)).Times(1);
        }
    }

    static void expectCreateEvents(
        MpsBackendOpsMock& mock,
        std::initializer_list<::orteaf::internal::backend::mps::MPSEvent_t> handles,
        ::testing::Matcher<::orteaf::internal::backend::mps::MPSDevice_t> device_matcher = ::testing::_) {
        if (handles.size() == 0) {
            EXPECT_CALL(mock, createEvent(device_matcher)).Times(0);
            return;
        }
        struct State {
            std::vector<::orteaf::internal::backend::mps::MPSEvent_t> values;
            std::size_t next{0};
        };
        auto state = std::make_shared<State>();
        state->values.assign(handles.begin(), handles.end());
        const auto call_count = state->values.size();
        EXPECT_CALL(mock, createEvent(device_matcher))
            .Times(call_count)
            .WillRepeatedly(::testing::Invoke(
                [state](::orteaf::internal::backend::mps::MPSDevice_t /*device*/) mutable {
                    const auto handle = state->values[state->next];
                    ++state->next;
                    return handle;
                }));
    }

    static void expectDestroyEvents(
        MpsBackendOpsMock& mock,
        std::initializer_list<::orteaf::internal::backend::mps::MPSEvent_t> handles) {
        if (handles.size() == 0) {
            EXPECT_CALL(mock, destroyEvent(::testing::_)).Times(0);
            return;
        }
        for (auto handle : handles) {
            EXPECT_CALL(mock, destroyEvent(handle)).Times(1);
        }
    }

    static void expectDestroyEventsInOrder(
        MpsBackendOpsMock& mock,
        std::initializer_list<::orteaf::internal::backend::mps::MPSEvent_t> handles) {
        if (handles.size() == 0) {
            EXPECT_CALL(mock, destroyEvent(::testing::_)).Times(0);
            return;
        }
        ::testing::InSequence seq;
        for (auto handle : handles) {
            EXPECT_CALL(mock, destroyEvent(handle)).Times(1);
        }
    }

    static void expectCreateFences(
        MpsBackendOpsMock& mock,
        std::initializer_list<::orteaf::internal::backend::mps::MPSFence_t> handles,
        ::testing::Matcher<::orteaf::internal::backend::mps::MPSDevice_t> device_matcher = ::testing::_) {
        if (handles.size() == 0) {
            EXPECT_CALL(mock, createFence(device_matcher)).Times(0);
            return;
        }
        struct State {
            std::vector<::orteaf::internal::backend::mps::MPSFence_t> values;
            std::size_t next{0};
        };
        auto state = std::make_shared<State>();
        state->values.assign(handles.begin(), handles.end());
        const auto call_count = state->values.size();
        EXPECT_CALL(mock, createFence(device_matcher))
            .Times(call_count)
            .WillRepeatedly(::testing::Invoke(
                [state](::orteaf::internal::backend::mps::MPSDevice_t /*device*/) mutable {
                    const auto handle = state->values[state->next];
                    ++state->next;
                    return handle;
                }));
    }

    static void expectDestroyFences(
        MpsBackendOpsMock& mock,
        std::initializer_list<::orteaf::internal::backend::mps::MPSFence_t> handles) {
        if (handles.size() == 0) {
            EXPECT_CALL(mock, destroyFence(::testing::_)).Times(0);
            return;
        }
        for (auto handle : handles) {
            EXPECT_CALL(mock, destroyFence(handle)).Times(1);
        }
    }

    static void expectCreateLibraries(
        MpsBackendOpsMock& mock,
        std::initializer_list<std::pair<std::string, ::orteaf::internal::backend::mps::MPSLibrary_t>> expectations,
        ::testing::Matcher<::orteaf::internal::backend::mps::MPSDevice_t> device_matcher = ::testing::_) {
        if (expectations.size() == 0) {
            EXPECT_CALL(mock, createLibraryWithName(device_matcher, ::testing::_)).Times(0);
            return;
        }
        struct State {
            std::vector<::orteaf::internal::backend::mps::MPSLibrary_t> handles;
            std::vector<std::string> names;
            std::size_t next{0};
        };
        auto state = std::make_shared<State>();
        for (const auto& [name, handle] : expectations) {
            state->names.push_back(name);
            state->handles.push_back(handle);
        }
        const auto call_count = state->handles.size();
        EXPECT_CALL(mock, createLibraryWithName(device_matcher, ::testing::_))
            .Times(call_count)
            .WillRepeatedly(::testing::Invoke(
                [state](::orteaf::internal::backend::mps::MPSDevice_t /*device*/,
                        std::string_view requested_name) mutable {
                    const auto index = state->next;
                    EXPECT_LT(index, state->names.size());
                    if (index < state->names.size()) {
                        EXPECT_EQ(requested_name, state->names[index]);
                    }
                    const auto handle = state->handles[index];
                    ++state->next;
                    return handle;
                }));
    }

    static void expectDestroyLibraries(
        MpsBackendOpsMock& mock,
        std::initializer_list<::orteaf::internal::backend::mps::MPSLibrary_t> handles) {
        if (handles.size() == 0) {
            EXPECT_CALL(mock, destroyLibrary(::testing::_)).Times(0);
            return;
        }
        for (auto handle : handles) {
            EXPECT_CALL(mock, destroyLibrary(handle)).Times(1);
        }
    }

    static void expectCreateFunctions(
        MpsBackendOpsMock& mock,
        std::initializer_list<std::pair<std::string, ::orteaf::internal::backend::mps::MPSFunction_t>> expectations,
        ::testing::Matcher<::orteaf::internal::backend::mps::MPSLibrary_t> library_matcher = ::testing::_) {
        if (expectations.size() == 0) {
            EXPECT_CALL(mock, createFunction(library_matcher, ::testing::_)).Times(0);
            return;
        }
        struct State {
            std::vector<::orteaf::internal::backend::mps::MPSFunction_t> handles;
            std::vector<std::string> names;
            std::size_t next{0};
        };
        auto state = std::make_shared<State>();
        for (const auto& [name, handle] : expectations) {
            state->names.push_back(name);
            state->handles.push_back(handle);
        }
        const auto call_count = state->handles.size();
        EXPECT_CALL(mock, createFunction(library_matcher, ::testing::_))
            .Times(call_count)
            .WillRepeatedly(::testing::Invoke(
                [state](::orteaf::internal::backend::mps::MPSLibrary_t /*library*/,
                        std::string_view requested_name) mutable {
                    const auto index = state->next;
                    EXPECT_LT(index, state->names.size());
                    if (index < state->names.size()) {
                        EXPECT_EQ(requested_name, state->names[index]);
                    }
                    const auto handle = state->handles[index];
                    ++state->next;
                    return handle;
                }));
    }

    static void expectDestroyFunctions(
        MpsBackendOpsMock& mock,
        std::initializer_list<::orteaf::internal::backend::mps::MPSFunction_t> handles) {
        if (handles.size() == 0) {
            EXPECT_CALL(mock, destroyFunction(::testing::_)).Times(0);
            return;
        }
        for (auto handle : handles) {
            EXPECT_CALL(mock, destroyFunction(handle)).Times(1);
        }
    }

    static void expectCreateComputePipelineStates(
        MpsBackendOpsMock& mock,
        std::initializer_list<std::pair<
            ::orteaf::internal::backend::mps::MPSFunction_t,
            ::orteaf::internal::backend::mps::MPSComputePipelineState_t>> expectations,
        ::testing::Matcher<::orteaf::internal::backend::mps::MPSDevice_t> device_matcher = ::testing::_) {
        if (expectations.size() == 0) {
            EXPECT_CALL(mock, createComputePipelineState(device_matcher, ::testing::_)).Times(0);
            return;
        }
        struct State {
            std::vector<::orteaf::internal::backend::mps::MPSFunction_t> functions;
            std::vector<::orteaf::internal::backend::mps::MPSComputePipelineState_t> pipelines;
            std::size_t next{0};
        };
        auto state = std::make_shared<State>();
        for (const auto& [function, pipeline] : expectations) {
            state->functions.push_back(function);
            state->pipelines.push_back(pipeline);
        }
        const auto call_count = state->pipelines.size();
        EXPECT_CALL(mock, createComputePipelineState(device_matcher, ::testing::_))
            .Times(call_count)
            .WillRepeatedly(::testing::Invoke(
                [state](::orteaf::internal::backend::mps::MPSDevice_t /*device*/,
                        ::orteaf::internal::backend::mps::MPSFunction_t function) mutable {
                    const auto index = state->next;
                    EXPECT_LT(index, state->functions.size());
                    if (index < state->functions.size()) {
                        EXPECT_EQ(function, state->functions[index]);
                    }
                    const auto pipeline = state->pipelines[index];
                    ++state->next;
                    return pipeline;
                }));
    }

    static void expectDestroyComputePipelineStates(
        MpsBackendOpsMock& mock,
        std::initializer_list<::orteaf::internal::backend::mps::MPSComputePipelineState_t> handles) {
        if (handles.size() == 0) {
            EXPECT_CALL(mock, destroyComputePipelineState(::testing::_)).Times(0);
            return;
        }
        for (auto handle : handles) {
            EXPECT_CALL(mock, destroyComputePipelineState(handle)).Times(1);
        }
    }

    static void expectCreateHeaps(
        MpsBackendOpsMock& mock,
        std::initializer_list<::orteaf::internal::backend::mps::MPSHeap_t> handles,
        ::testing::Matcher<::orteaf::internal::backend::mps::MPSDevice_t> device_matcher = ::testing::_,
        ::testing::Matcher<::orteaf::internal::backend::mps::MPSHeapDescriptor_t> descriptor_matcher = ::testing::_) {
        if (handles.size() == 0) {
            EXPECT_CALL(mock, createHeap(device_matcher, descriptor_matcher)).Times(0);
            return;
        }
        struct State {
            std::vector<::orteaf::internal::backend::mps::MPSHeap_t> values;
            std::size_t next{0};
        };
        auto state = std::make_shared<State>();
        state->values.assign(handles.begin(), handles.end());
        const auto call_count = state->values.size();
        EXPECT_CALL(mock, createHeap(device_matcher, descriptor_matcher))
            .Times(call_count)
            .WillRepeatedly(::testing::Invoke(
                [state](::orteaf::internal::backend::mps::MPSDevice_t,
                        ::orteaf::internal::backend::mps::MPSHeapDescriptor_t) mutable {
                    const auto handle = state->values[state->next];
                    ++state->next;
                    return handle;
                }));
    }

    static void expectCreateHeapsInOrder(
        MpsBackendOpsMock& mock,
        std::initializer_list<std::pair<
            ::orteaf::internal::backend::mps::MPSHeapDescriptor_t,
            ::orteaf::internal::backend::mps::MPSHeap_t>> expectations,
        ::testing::Matcher<::orteaf::internal::backend::mps::MPSDevice_t> device_matcher = ::testing::_) {
        if (expectations.size() == 0) {
            EXPECT_CALL(mock, createHeap(device_matcher, ::testing::_)).Times(0);
            return;
        }
        ::testing::InSequence seq;
        for (const auto& [descriptor, handle] : expectations) {
            EXPECT_CALL(mock, createHeap(device_matcher, descriptor))
                .WillOnce(::testing::Return(handle));
        }
    }

    static void expectDestroyHeaps(
        MpsBackendOpsMock& mock,
        std::initializer_list<::orteaf::internal::backend::mps::MPSHeap_t> handles) {
        if (handles.size() == 0) {
            EXPECT_CALL(mock, destroyHeap(::testing::_)).Times(0);
            return;
        }
        for (auto handle : handles) {
            EXPECT_CALL(mock, destroyHeap(handle)).Times(1);
        }
    }

    static void expectCreateHeapDescriptors(
        MpsBackendOpsMock& mock,
        std::initializer_list<::orteaf::internal::backend::mps::MPSHeapDescriptor_t> handles) {
        if (handles.size() == 0) {
            EXPECT_CALL(mock, createHeapDescriptor()).Times(0);
            return;
        }
        struct State {
            std::vector<::orteaf::internal::backend::mps::MPSHeapDescriptor_t> values;
            std::size_t next{0};
        };
        auto state = std::make_shared<State>();
        state->values.assign(handles.begin(), handles.end());
        const auto call_count = state->values.size();
        EXPECT_CALL(mock, createHeapDescriptor())
            .Times(call_count)
            .WillRepeatedly(::testing::Invoke(
                [state]() mutable {
                    const auto handle = state->values[state->next];
                    ++state->next;
                    return handle;
                }));
    }

    static void expectDestroyHeapDescriptors(
        MpsBackendOpsMock& mock,
        std::initializer_list<::orteaf::internal::backend::mps::MPSHeapDescriptor_t> handles) {
        if (handles.size() == 0) {
            EXPECT_CALL(mock, destroyHeapDescriptor(::testing::_)).Times(0);
            return;
        }
        for (auto handle : handles) {
            EXPECT_CALL(mock, destroyHeapDescriptor(handle)).Times(1);
        }
    }

};

#define ORTEAF_EXPECT_SET_HEAP_DESCRIPTOR_VALUES(mock, MethodName, expectations) \
    do { \
        if ((expectations).size() == 0) { \
            EXPECT_CALL(mock, MethodName(::testing::_, ::testing::_)).Times(0); \
        } else { \
            for (const auto& [descriptor, value] : (expectations)) { \
                EXPECT_CALL(mock, MethodName(descriptor, value)).Times(1); \
            } \
        } \
    } while (0)

}  // namespace orteaf::tests::runtime::mps

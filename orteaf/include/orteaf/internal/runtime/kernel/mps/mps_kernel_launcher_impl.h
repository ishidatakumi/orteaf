#pragma once

#if ORTEAF_ENABLE_MPS

#include <array>
#include <limits>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/heap_vector.h"
#include "orteaf/internal/backend/mps/mps_fast_ops.h"
#include "orteaf/internal/backend/mps/wrapper/mps_compute_command_encorder.h"
#include "orteaf/internal/runtime/ops/mps/common/mps_common_ops.h"

#include "orteaf/internal/runtime/manager/mps/mps_compute_pipeline_state_manager.h"
#include "orteaf/internal/runtime/manager/mps/mps_library_manager.h"

namespace orteaf::internal::runtime::mps {

template <std::size_t N>
class MpsKernelLauncherImpl {
public:
    using PipelineLease = MpsComputePipelineStateManager::PipelineLease;
    using Key = std::pair<LibraryKey, FunctionKey>;
    struct KeyLiteral {
        const char* library;
        const char* function;
    };

    MpsKernelLauncherImpl() = default;

    explicit MpsKernelLauncherImpl(std::initializer_list<KeyLiteral> keys) {
        for (const auto& k : keys) {
            addKey(k.library, k.function);
        }
    }

    MpsKernelLauncherImpl(const MpsKernelLauncherImpl&) = delete;
    MpsKernelLauncherImpl& operator=(const MpsKernelLauncherImpl&) = delete;
    MpsKernelLauncherImpl(MpsKernelLauncherImpl&&) = default;
    MpsKernelLauncherImpl& operator=(MpsKernelLauncherImpl&&) = default;
    ~MpsKernelLauncherImpl() = default;

    bool initialized(::orteaf::internal::base::DeviceHandle device) const noexcept {
        const auto idx = findEntryIndex(device);
        return idx != kInvalidIndex && device_pipelines_[idx].initialized;
    }

    template <typename PrivateOps = ::orteaf::internal::runtime::ops::mps::MpsPrivateOps>
    void initialize(::orteaf::internal::base::DeviceHandle device) {
        auto entry_idx = findEntryIndex(device);
        if (entry_idx == kInvalidIndex) {
            device_pipelines_.pushBack(DevicePipelines{device});
            entry_idx = device_pipelines_.size() - 1;
        }
        auto& entry = device_pipelines_[entry_idx];
        entry.pipelines.clear();
        entry.pipelines.reserve(size_);
        for (std::size_t i = 0; i < size_; ++i) {
            const auto& key = keys_[i];
            entry.pipelines.pushBack(PrivateOps::acquirePipeline(device, key.first, key.second));
        }
        entry.initialized = true;
    }

#if ORTEAF_ENABLE_TEST
    const std::array<Key, N>& keysForTest() const noexcept { return keys_; }
    std::size_t sizeForTest() const noexcept { return size_; }
    PipelineLease& pipelineLeaseForTest(::orteaf::internal::base::DeviceHandle device, std::size_t index) {
        auto idx = findEntryIndex(device);
        return device_pipelines_[idx].pipelines[index];
    }
    std::size_t pipelineCountForTest(::orteaf::internal::base::DeviceHandle device) const noexcept {
        auto idx = findEntryIndex(device);
        return (idx == kInvalidIndex) ? 0 : device_pipelines_[idx].pipelines.size();
    }
#endif

    // Convenience: create a command buffer from a command queue without exposing
    // backend wrapper details to launcher users.
    template <typename FastOps = ::orteaf::internal::runtime::backend_ops::mps::MpsFastOps>
    ::orteaf::internal::backend::mps::MPSCommandBuffer_t createCommandBuffer(
        ::orteaf::internal::backend::mps::MPSCommandQueue_t command_queue) const {
        return FastOps::createCommandBuffer(command_queue);
    }

    // Convenience: create a compute encoder and bind the pipeline in one step.
    template <typename FastOps = ::orteaf::internal::runtime::backend_ops::mps::MpsFastOps>
    ::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t createComputeEncoder(
        ::orteaf::internal::backend::mps::MPSCommandBuffer_t command_buffer,
        ::orteaf::internal::base::DeviceHandle device,
        std::size_t pipeline_index) const {
        const auto entry_idx = findEntryIndex(device);
        if (entry_idx == kInvalidIndex) return nullptr;
        const auto& entry = device_pipelines_[entry_idx];
        if (!entry.initialized || pipeline_index >= entry.pipelines.size()) {
            return nullptr;
        }
        auto* encoder = FastOps::createComputeCommandEncoder(command_buffer);
        FastOps::setPipelineState(encoder, entry.pipelines[pipeline_index].pointer());
        return encoder;
    }

    template <typename FastOps = ::orteaf::internal::runtime::backend_ops::mps::MpsFastOps>
    ::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t createComputeEncoder(
        ::orteaf::internal::backend::mps::MPSCommandBuffer_t command_buffer,
        ::orteaf::internal::base::DeviceHandle device,
        std::string_view library,
        std::string_view function) const {
        const std::size_t idx = findKeyIndex(library, function);
        const auto entry_idx = findEntryIndex(device);
        if (entry_idx == kInvalidIndex) return nullptr;
        const auto& entry = device_pipelines_[entry_idx];
        if (!entry.initialized || idx >= entry.pipelines.size()) {
            return nullptr;
        }
        auto* encoder = FastOps::createComputeCommandEncoder(command_buffer);
        FastOps::setPipelineState(encoder, entry.pipelines[idx].pointer());
        return encoder;
    }

    // Convenience: bind a buffer to the compute encoder.
    template <typename FastOps = ::orteaf::internal::runtime::backend_ops::mps::MpsFastOps>
    void setBuffer(::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t encoder,
                   ::orteaf::internal::backend::mps::MPSBuffer_t buffer,
                   std::size_t offset,
                   std::size_t index) const {
        FastOps::setBuffer(encoder, buffer, offset, index);
    }

    // Convenience: bind raw bytes to the compute encoder.
    template <typename FastOps = ::orteaf::internal::runtime::backend_ops::mps::MpsFastOps>
    void setBytes(::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t encoder,
                  const void* bytes,
                  std::size_t length,
                  std::size_t index) const {
        FastOps::setBytes(encoder, bytes, length, index);
    }

    // Convenience: dispatch threadgroups and finalize/commit.
    template <typename FastOps = ::orteaf::internal::runtime::backend_ops::mps::MpsFastOps>
    void dispatchThreadgroups(::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t encoder,
                              ::orteaf::internal::backend::mps::MPSSize_t threadgroups,
                              ::orteaf::internal::backend::mps::MPSSize_t threads_per_threadgroup) const {
        FastOps::setThreadgroups(encoder, threadgroups, threads_per_threadgroup);
    }

    template <typename FastOps = ::orteaf::internal::runtime::backend_ops::mps::MpsFastOps>
    void endEncoding(::orteaf::internal::backend::mps::MPSComputeCommandEncoder_t encoder) const {
        FastOps::endEncoding(encoder);
    }

    template <typename FastOps = ::orteaf::internal::runtime::backend_ops::mps::MpsFastOps>
    void commit(::orteaf::internal::backend::mps::MPSCommandBuffer_t command_buffer) const {
        FastOps::commit(command_buffer);
    }

    // One-shot dispatch helper: create command buffer/encoder, bind pipeline, invoke a binder
    // functor to set resources, dispatch, end encoding, and commit. Returns the command buffer.
    template <typename FastOps = ::orteaf::internal::runtime::backend_ops::mps::MpsFastOps, typename Binder>
    ::orteaf::internal::backend::mps::MPSCommandBuffer_t dispatchOneShot(
        ::orteaf::internal::backend::mps::MPSCommandQueue_t command_queue,
        ::orteaf::internal::base::DeviceHandle device,
        std::size_t pipeline_index,
        ::orteaf::internal::backend::mps::MPSSize_t threadgroups,
        ::orteaf::internal::backend::mps::MPSSize_t threads_per_threadgroup,
        Binder&& binder) const {
        const auto entry_idx = findEntryIndex(device);
        if (entry_idx == kInvalidIndex) return nullptr;
        const auto& entry = device_pipelines_[entry_idx];
        if (!entry.initialized || pipeline_index >= entry.pipelines.size()) return nullptr;
        auto* command_buffer = FastOps::createCommandBuffer(command_queue);
        auto* encoder = FastOps::createComputeCommandEncoder(command_buffer);
        FastOps::setPipelineState(encoder, entry.pipelines[pipeline_index].pointer());
        static_cast<Binder&&>(binder)(encoder);
        FastOps::setThreadgroups(encoder, threadgroups, threads_per_threadgroup);
        FastOps::endEncoding(encoder);
        FastOps::commit(command_buffer);
        return command_buffer;
    }

    template <typename FastOps = ::orteaf::internal::runtime::backend_ops::mps::MpsFastOps, typename Binder>
    ::orteaf::internal::backend::mps::MPSCommandBuffer_t dispatchOneShot(
        ::orteaf::internal::backend::mps::MPSCommandQueue_t command_queue,
        ::orteaf::internal::base::DeviceHandle device,
        std::string_view library,
        std::string_view function,
        ::orteaf::internal::backend::mps::MPSSize_t threadgroups,
        ::orteaf::internal::backend::mps::MPSSize_t threads_per_threadgroup,
        Binder&& binder) const {
        const std::size_t idx = findKeyIndex(library, function);
        const auto entry_idx = findEntryIndex(device);
        if (entry_idx == kInvalidIndex) return nullptr;
        const auto& entry = device_pipelines_[entry_idx];
        if (!entry.initialized || idx >= entry.pipelines.size()) {
            return nullptr;
        }
        return dispatchOneShot<FastOps>(command_queue, device, idx, threadgroups, threads_per_threadgroup,
                                        static_cast<Binder&&>(binder));
    }

private:
    // Append a key constructed from raw identifiers. Marks the launcher as not initialized.
    void addKey(std::string library_identifier, std::string function_identifier) {
        addKeyInternal(LibraryKey::Named(std::move(library_identifier)),
                       FunctionKey::Named(std::move(function_identifier)));
    }

    void addKeyInternal(const LibraryKey& lib, const FunctionKey& func) {
        if (isDuplicate(lib, func) || size_ >= N) {
            return;
        }
        keys_[size_++] = Key{lib, func};
        for (auto& entry : device_pipelines_) {
            entry.initialized = false;
            entry.pipelines.clear();
        }
    }

    std::size_t findKeyIndex(std::string_view library, std::string_view function) const noexcept {
        for (std::size_t i = 0; i < size_; ++i) {
            const auto& key = keys_[i];
            if (key.first.identifier == library && key.second.identifier == function) {
                return i;
            }
        }
        return std::numeric_limits<std::size_t>::max();
    }

    std::size_t findEntryIndex(::orteaf::internal::base::DeviceHandle device) const noexcept {
        for (std::size_t i = 0; i < device_pipelines_.size(); ++i) {
            if (device_pipelines_[i].device == device) {
                return i;
            }
        }
        return kInvalidIndex;
    }

    bool isDuplicate(const LibraryKey& lib, const FunctionKey& func) const {
        for (std::size_t i = 0; i < size_; ++i) {
            const auto& key = keys_[i];
            if (key.first == lib && key.second == func) {
                return true;
            }
        }
        return false;
    }

    struct DevicePipelines {
        ::orteaf::internal::base::DeviceHandle device{};
        ::orteaf::internal::base::HeapVector<PipelineLease> pipelines{};
        bool initialized{false};
    };

    ::orteaf::internal::base::HeapVector<DevicePipelines> device_pipelines_{};
    std::array<Key, N> keys_{};
    std::size_t size_{0};
    static constexpr std::size_t kInvalidIndex = std::numeric_limits<std::size_t>::max();
};

}  // namespace orteaf::internal::runtime::mps

#endif  // ORTEAF_ENABLE_MPS

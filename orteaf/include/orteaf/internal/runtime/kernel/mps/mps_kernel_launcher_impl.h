#pragma once

#if ORTEAF_ENABLE_MPS

#include <array>
#include <initializer_list>
#include <string>
#include <utility>
#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/base/heap_vector.h"

#include "orteaf/internal/runtime/manager/mps/mps_compute_pipeline_state_manager.h"
#include "orteaf/internal/runtime/manager/mps/mps_library_manager.h"

namespace orteaf::internal::runtime::mps {

template <std::size_t N>
class MpsKernelLauncherImpl {
public:
    using PipelineLease = MpsComputePipelineStateManager::PipelineLease;
    using Key = std::pair<FunctionKey, LibraryKey>;
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

    bool initialized() const noexcept { return initialized_; }

    template <typename PrivateOps>
    void initialize(::orteaf::internal::base::DeviceHandle device,
                    PrivateOps& ops) {
        pipelines_.clear();
        pipelines_.reserve(size_);
        for (std::size_t i = 0; i < size_; ++i) {
            const auto& key = keys_[i];
            pipelines_.pushBack(ops.acquirePipeline(device, key.second, key.first));
        }
        initialized_ = true;
    }

#if ORTEAF_ENABLE_TEST
    const std::array<Key, N>& keysForTest() const noexcept { return keys_; }
    std::size_t sizeForTest() const noexcept { return size_; }
#endif

private:
    // Append a key constructed from raw identifiers. Marks the launcher as not initialized.
    void addKey(std::string library_identifier, std::string function_identifier) {
        addKeyInternal(FunctionKey::Named(std::move(function_identifier)),
                       LibraryKey::Named(std::move(library_identifier)));
    }

    void addKeyInternal(const FunctionKey& func, const LibraryKey& lib) {
        if (isDuplicate(func, lib) || size_ >= N) {
            return;
        }
        keys_[size_++] = Key{func, lib};
        initialized_ = false;
    }

    bool isDuplicate(const FunctionKey& func, const LibraryKey& lib) const {
        for (std::size_t i = 0; i < size_; ++i) {
            const auto& key = keys_[i];
            if (key.first == func && key.second == lib) {
                return true;
            }
        }
        return false;
    }

    ::orteaf::internal::base::HeapVector<PipelineLease> pipelines_{};
    bool initialized_{false};
    std::array<Key, N> keys_{};
    std::size_t size_{0};
};

}  // namespace orteaf::internal::runtime::mps

#endif  // ORTEAF_ENABLE_MPS

#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>

#include "orteaf/internal/backend/mps/mps_buffer_view.h"
#include "orteaf/internal/backend/mps/wrapper/mps_buffer.h"
#include "orteaf/internal/backend/mps/wrapper/mps_heap.h"

namespace orteaf::internal::backend::mps {

// Simple MPS resource that keeps device/heap handles per instance and creates buffers at offset 0.
class MpsResource {
public:
    using BufferView = ::orteaf::internal::backend::mps::MpsBufferView;

    struct Config {
        MPSDevice_t device{nullptr};
        MPSHeap_t heap{nullptr};
        MPSBufferUsage_t usage{kMPSDefaultBufferUsage};
    };

    MpsResource() = default;

    explicit MpsResource(const Config& config) { initialize(config); }

    void initialize(const Config& config);

    BufferView allocate(std::size_t size, std::size_t alignment);

    void deallocate(BufferView view, std::size_t size, std::size_t alignment) noexcept;

    MPSDevice_t device() const noexcept { return device_; }
    MPSHeap_t heap() const noexcept { return heap_; }

private:
    MPSDevice_t device_{nullptr};
    MPSHeap_t heap_{nullptr};
    MPSBufferUsage_t usage_{kMPSDefaultBufferUsage};
    bool initialized_{false};
};

}  // namespace orteaf::internal::backend::mps

#endif  // ORTEAF_ENABLE_MPS

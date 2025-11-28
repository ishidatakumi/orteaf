#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>

#include "orteaf/internal/backend/mps/mps_buffer_view.h"
#include "orteaf/internal/backend/mps/wrapper/mps_buffer.h"
#include "orteaf/internal/backend/mps/wrapper/mps_heap.h"

namespace orteaf::internal::backend::mps {

// Simple MPS resource that keeps device/heap handles and creates buffers at offset 0.
class MpsResource {
public:
    using BufferView = ::orteaf::internal::backend::mps::MpsBufferView;

    struct Config {
        MPSDevice_t device{nullptr};
        MPSHeap_t heap{nullptr};
        MPSBufferUsage_t usage{kMPSDefaultBufferUsage};
    };

    static void initialize(const Config& config) noexcept;

    static BufferView allocate(std::size_t size, std::size_t alignment);

    static void deallocate(BufferView view, std::size_t size, std::size_t alignment) noexcept;

    static MPSDevice_t device() noexcept { return state_.device; }
    static MPSHeap_t heap() noexcept { return state_.heap; }

private:
    struct State {
        MPSDevice_t device{nullptr};
        MPSHeap_t heap{nullptr};
        MPSBufferUsage_t usage{kMPSDefaultBufferUsage};
        bool initialized{false};
    };

    static inline State state_{};
};

}  // namespace orteaf::internal::backend::mps

#endif  // ORTEAF_ENABLE_MPS

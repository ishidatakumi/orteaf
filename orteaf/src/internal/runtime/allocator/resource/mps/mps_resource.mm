#include "orteaf/internal/runtime/allocator/resource/mps/mps_resource.h"
#include "orteaf/internal/backend/mps/wrapper/mps_buffer.h"
#include "orteaf/internal/backend/mps/wrapper/mps_command_buffer.h"

#include "orteaf/internal/diagnostics/error/error_macros.h"

namespace orteaf::internal::backend::mps {

void MpsResource::initialize(const Config& config) {
    ORTEAF_THROW_IF_NULL(config.device, "MpsResource requires non-null device");
    ORTEAF_THROW_IF_NULL(config.heap, "MpsResource requires non-null heap");
    device_ = config.device;
    heap_ = config.heap;
    usage_ = config.usage;
    initialized_ = (device_ != nullptr && heap_ != nullptr);
}

MpsResource::BufferView MpsResource::allocate(std::size_t size, std::size_t /*alignment*/) {
    ORTEAF_THROW_IF(!initialized_, InvalidState, "MpsResource::allocate called before initialize");
    ORTEAF_THROW_IF(size == 0, InvalidParameter, "MpsResource::allocate requires size > 0");

    MPSBuffer_t buffer = createBuffer(heap_, size, usage_);
    if (!buffer) {
        return {};
    }

    return BufferView{buffer, 0, size};
}

void MpsResource::deallocate(BufferView view, std::size_t /*size*/, std::size_t /*alignment*/) noexcept {
    if (!initialized_ || !view) {
        return;
    }
    destroyBuffer(view.raw());
}

bool MpsResource::isCompleted(FenceToken& token) {
    ORTEAF_THROW_IF(!initialized_, InvalidState, "MpsResource::isCompleted called before initialize");
    bool all_completed = true;
    for (auto& ticket : token) {
        if (!ticket.valid()) {
            continue;
        }
        if (::orteaf::internal::backend::mps::isCompleted(ticket.commandBuffer())) {
            ticket.reset();  // mark as invalid so subsequent calls skip it
            continue;
        } else {
            all_completed = false;
            break;
        }
    }

    return all_completed;
}

bool MpsResource::isCompleted(ReuseToken& token) {
    ORTEAF_THROW_IF(!initialized_, InvalidState, "MpsResource::isCompleted called before initialize");
    bool all_completed = true;
    for (auto& ticket : token) {
        if (!ticket.valid()) {
            continue;
        }
        if (::orteaf::internal::backend::mps::isCompleted(ticket.commandBuffer())) {
            ticket.reset();
            continue;
        } else {
            all_completed = false;
            break;
        }
    }

    return all_completed;
}

}  // namespace orteaf::internal::backend::mps

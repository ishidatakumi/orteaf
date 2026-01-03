
#include "orteaf/internal/execution/allocator/resource/mps/mps_resource.h"
#include <orteaf/internal/execution/mps/platform/mps_fast_ops.h>
#include <orteaf/internal/execution/mps/platform/wrapper/mps_buffer.h>

#include "orteaf/internal/diagnostics/error/error_macros.h"
#include <limits>

namespace orteaf::internal::execution::allocator::resource::mps {

void MpsResource::initialize(const Config &config) {
  ORTEAF_THROW_IF_NULL(config.device, "MpsResource requires non-null device");
  ORTEAF_THROW_IF_NULL(config.heap, "MpsResource requires non-null heap");
  ORTEAF_THROW_IF(!config.device_handle.isValid(), InvalidParameter,
                  "MpsResource requires a valid DeviceHandle");
  destroyFreelist();
  device_ = config.device;
  device_handle_ = config.device_handle;
  heap_ = config.heap;
  usage_ = config.usage;
  initialized_ = (device_ != nullptr && heap_ != nullptr);
}

MpsResource::BufferView MpsResource::allocate(std::size_t size,
                                              std::size_t /*alignment*/) {
  ORTEAF_THROW_IF(!initialized_, InvalidState,
                  "MpsResource::allocate called before initialize");
  ORTEAF_THROW_IF(size == 0, InvalidParameter,
                  "MpsResource::allocate requires size > 0");

  ::orteaf::internal::execution::mps::platform::wrapper::MpsBuffer_t buffer =
      ::orteaf::internal::execution::mps::platform::wrapper::createBuffer(
          heap_, size, usage_);
  if (!buffer) {
    return {};
  }

  return BufferView{buffer, 0, size};
}

void MpsResource::deallocate(BufferView view, std::size_t /*size*/,
                             std::size_t /*alignment*/) noexcept {
  if (!initialized_ || !view) {
    return;
  }
  ::orteaf::internal::execution::mps::platform::wrapper::destroyBuffer(
      view.raw());
}

bool MpsResource::isCompleted(FenceToken &token) {
  ORTEAF_THROW_IF(!initialized_, InvalidState,
                  "MpsResource::isCompleted called before initialize");
  bool all_completed = true;
  for (auto &lease : token) {
    auto *payload = lease.payloadPtr();
    if (payload == nullptr) {
      lease.release();
      continue;
    }
    if (payload->isReady<
            ::orteaf::internal::execution::mps::platform::MpsFastOps>()) {
      lease.release();
      continue;
    }
    all_completed = false;
    break;
  }

  return all_completed;
}

bool MpsResource::isCompleted(ReuseToken &token) {
  ORTEAF_THROW_IF(!initialized_, InvalidState,
                  "MpsResource::isCompleted called before initialize");
  bool all_completed = true;
  for (auto &ticket : token) {
    if (!ticket.valid()) {
      continue;
    }
    if (::orteaf::internal::execution::mps::platform::MpsFastOps::isCompleted(
            ticket.commandBuffer())) {
      ticket.reset();
      continue;
    } else {
      all_completed = false;
      break;
    }
  }

  return all_completed;
}

MpsResource::BufferView
MpsResource::makeView(BufferView base, std::size_t offset, std::size_t size) {
  return BufferView{base.raw(), offset, size};
}

void MpsResource::initializeChunkAsFreelist(std::size_t list_index,
                                            BufferView chunk,
                                            std::size_t chunk_size,
                                            std::size_t block_size,
                                            const LaunchParams &launch_params) {
  // this method is still unimplemented
}

MpsResource::BufferView
MpsResource::popFreelistNode(std::size_t list_index,
                             const LaunchParams &launch_params) {
  return {};
}

void MpsResource::pushFreelistNode(std::size_t list_index, BufferView view,
                                   const LaunchParams &launch_params) {
  // this method is still unimplemented
}

void MpsResource::destroyFreelist() {
  // this method is still unimplemented
}

void MpsResource::ensureList(std::size_t list_index) {
  // this method is still unimplemented
}

} // namespace orteaf::internal::execution::allocator::resource::mps

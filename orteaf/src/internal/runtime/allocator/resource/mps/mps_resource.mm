
#include "orteaf/internal/runtime/allocator/resource/mps/mps_resource.h"
#include "orteaf/internal/runtime/mps/manager/mps_compute_pipeline_state_manager.h"
#include <orteaf/internal/runtime/allocator/resource/mps/mps_resource.h>
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_buffer.h>
#include <orteaf/internal/runtime/mps/platform/wrapper/mps_command_buffer.h>

#include "orteaf/internal/diagnostics/error/error_macros.h"
#include <limits>

namespace orteaf::internal::backend::mps {

namespace {
constexpr uint32_t kInvalidIndex = 0xffffffffu;
}

namespace {
class ResourcePrivateOps {
public:
  using DeviceHandle = ::orteaf::internal::base::DeviceHandle;
  using LibraryKey = ::orteaf::internal::runtime::mps::manager::LibraryKey;
  using FunctionKey = ::orteaf::internal::runtime::mps::manager::FunctionKey;
  using PipelineLease = ::orteaf::internal::runtime::mps::manager::
      MpsComputePipelineStateManager::PipelineLease;

  static void setManager(
      ::orteaf::internal::runtime::mps::manager::MpsLibraryManager *mgr) {
    manager_ = mgr;
  }

  static PipelineLease acquirePipeline(DeviceHandle /*device*/,
                                       const LibraryKey &library_key,
                                       const FunctionKey &function_key) {
    ORTEAF_THROW_IF_NULL(manager_,
                         "ResourcePrivateOps requires library manager");
    auto library = manager_->acquire(library_key);
    auto pipeline_mgr = manager_->acquirePipelineManager(library);
    return pipeline_mgr->acquire(function_key);
  }

private:
  static inline ::orteaf::internal::runtime::mps::manager::MpsLibraryManager
      *manager_{nullptr};
};
} // namespace

void MpsResource::initialize(const Config &config) {
  ORTEAF_THROW_IF_NULL(config.device, "MpsResource requires non-null device");
  ORTEAF_THROW_IF_NULL(config.heap, "MpsResource requires non-null heap");
  ORTEAF_THROW_IF_NULL(config.library_manager,
                       "MpsResource requires non-null library_manager");
  ORTEAF_THROW_IF(!config.device_handle.isValid(), InvalidParameter,
                  "MpsResource requires a valid DeviceHandle");
  destroyFreelist();
  device_ = config.device;
  device_handle_ = config.device_handle;
  heap_ = config.heap;
  usage_ = config.usage;
  staging_heap_ = config.staging_heap ? config.staging_heap : heap_;
  staging_usage_ = config.staging_usage;
  library_manager_ = config.library_manager;
  chunks_.reserve(config.chunk_table_capacity);
  chunk_sizes_.reserve(config.chunk_table_capacity);
  chunk_list_index_.reserve(config.chunk_table_capacity);
  chunk_lookup_.reserve(config.chunk_table_capacity);
  initialized_ = (device_ != nullptr && heap_ != nullptr);
}

MpsResource::BufferView MpsResource::allocate(std::size_t size,
                                              std::size_t /*alignment*/) {
  ORTEAF_THROW_IF(!initialized_, InvalidState,
                  "MpsResource::allocate called before initialize");
  ORTEAF_THROW_IF(size == 0, InvalidParameter,
                  "MpsResource::allocate requires size > 0");

  ::orteaf::internal::runtime::mps::platform::wrapper::MPSBuffer_t buffer =
      ::orteaf::internal::runtime::mps::platform::wrapper::createBuffer(
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
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyBuffer(
      view.raw());
}

bool MpsResource::isCompleted(FenceToken &token) {
  ORTEAF_THROW_IF(!initialized_, InvalidState,
                  "MpsResource::isCompleted called before initialize");
  bool all_completed = true;
  for (auto &ticket : token) {
    if (!ticket.valid()) {
      continue;
    }
    if (::orteaf::internal::runtime::mps::platform::wrapper::isCompleted(
            ticket.commandBuffer())) {
      ticket.reset(); // mark as invalid so subsequent calls skip it
      continue;
    } else {
      all_completed = false;
      break;
    }
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
    if (::orteaf::internal::runtime::mps::platform::wrapper::isCompleted(
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
  ORTEAF_THROW_IF(!initialized_, InvalidState,
                  "MpsResource::initializeChunkAsFreelist before initialize");
  ORTEAF_THROW_IF(block_size == 0, InvalidParameter, "block_size must be > 0");

  const std::size_t block_count =
      (block_size == 0) ? 0 : (chunk_size / block_size);
  if (!chunk || block_count == 0) {
    return;
  }

  ensureList(list_index);
  auto &list = freelists_[list_index];
  ORTEAF_THROW_IF(list.block_size != 0 && list.block_size != block_size,
                  InvalidParameter,
                  "block_size must match existing freelist for list_index");
  list.block_size = block_size;
  list.block_count += block_count;

  if (!list.head) {
    list.head =
        ::orteaf::internal::runtime::mps::platform::wrapper::createBuffer(
            staging_heap_, 2 * sizeof(uint32_t), staging_usage_);
    auto *head_ptr = static_cast<uint32_t *>(
        ::orteaf::internal::runtime::mps::platform::wrapper::getBufferContents(
            list.head));
    ORTEAF_THROW_IF(head_ptr == nullptr, InvalidState,
                    "freelist head must be host-visible");
    head_ptr[0] = kInvalidIndex;
    head_ptr[1] = kInvalidIndex;
  }
  if (!list.out) {
    list.out = ::orteaf::internal::runtime::mps::platform::wrapper::createBuffer(
        staging_heap_, 2 * sizeof(uint32_t), staging_usage_);
    ORTEAF_THROW_IF(
        ::orteaf::internal::runtime::mps::platform::wrapper::getBufferContents(
            list.out) == nullptr,
        InvalidState, "freelist out must be host-visible");
  }

  const uint32_t chunk_id = static_cast<uint32_t>(chunks_.size());
  chunks_.pushBack(chunk);
  chunk_sizes_.pushBack(chunk_size);
  chunk_list_index_.pushBack(list_index);
  chunk_lookup_[chunk.raw()] = chunk_id;

  if (!freelist_launcher_.initialized(device_handle_)) {
    ResourcePrivateOps::setManager(library_manager_);
    freelist_launcher_.initialize<ResourcePrivateOps>(device_handle_);
  }

  if (launch_params.device && launch_params.device.pointer() != device_) {
    ORTEAF_THROW(InvalidParameter, "DeviceLease does not match resource device");
  }
  ORTEAF_THROW_IF(!launch_params.command_queue, InvalidParameter,
                  "CommandQueueLease must be provided for MPS freelist ops");
  auto *command_queue = launch_params.command_queue.pointer();
  ORTEAF_THROW_IF_NULL(command_queue, "Failed to create MPS command queue");
  auto *command_buffer = freelist_launcher_.createCommandBuffer(command_queue);
  ORTEAF_THROW_IF_NULL(command_buffer, "Failed to create MPS command buffer");

  auto *encoder = freelist_launcher_.createComputeEncoder(command_buffer,
                                                          device_handle_, 0);
  ORTEAF_THROW_IF_NULL(encoder,
                       "Failed to create compute encoder for freelist_init");

  freelist_launcher_.setBuffer(encoder, list.head, 0, 0); // head_offset
  freelist_launcher_.setBuffer(encoder, list.head, sizeof(uint32_t),
                               1); // head_chunk_id
  freelist_launcher_.setBuffer(encoder, chunk.raw(), chunk.offset(), 2);
  const uint32_t chunk_size32 = static_cast<uint32_t>(chunk_size);
  const uint32_t block_size32 = static_cast<uint32_t>(block_size);
  freelist_launcher_.setBytes(encoder, &chunk_size32, sizeof(uint32_t), 3);
  freelist_launcher_.setBytes(encoder, &block_size32, sizeof(uint32_t), 4);
  freelist_launcher_.setBuffer(encoder, list.out, 0, 5);
  freelist_launcher_.setBytes(encoder, &chunk_id, sizeof(uint32_t), 6);
  uint32_t old_head_offset = kInvalidIndex;
  uint32_t old_head_chunk = kInvalidIndex;
  if (auto *head_ptr = static_cast<uint32_t *>(
          ::orteaf::internal::runtime::mps::platform::wrapper::getBufferContents(
              list.head))) {
    old_head_offset = head_ptr[0];
    old_head_chunk = head_ptr[1];
  }
  freelist_launcher_.setBytes(encoder, &old_head_offset, sizeof(uint32_t), 7);
  freelist_launcher_.setBytes(encoder, &old_head_chunk, sizeof(uint32_t), 8);

  ::orteaf::internal::runtime::mps::platform::wrapper::MPSSize_t tg{1, 1, 1};
  freelist_launcher_.dispatchThreadgroups(encoder, tg, tg);
  freelist_launcher_.endEncoding(encoder);
  freelist_launcher_.commit(command_buffer);
  ::orteaf::internal::runtime::mps::platform::wrapper::waitUntilCompleted(
      command_buffer);
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyCommandBuffer(
      command_buffer);
}

MpsResource::BufferView
MpsResource::popFreelistNode(std::size_t list_index,
                             const LaunchParams &launch_params) {
  ORTEAF_THROW_IF(!initialized_, InvalidState,
                  "MpsResource::popFreelistNode before initialize");
  if (list_index >= freelists_.size()) {
    return {};
  }

  auto &list = freelists_[list_index];
  if (!list.head || !list.out || chunks_.empty() || list.block_size == 0 ||
      list.block_count == 0) {
    return {};
  }

  // Determine current head chunk to bind.
  BufferView current_chunk{};
  if (auto *head_ptr = static_cast<uint32_t *>(
          ::orteaf::internal::runtime::mps::platform::wrapper::getBufferContents(
              list.head))) {
    const uint32_t head_chunk = head_ptr[1];
    if (head_chunk != kInvalidIndex && head_chunk < chunks_.size()) {
      current_chunk = chunks_[head_chunk];
      ORTEAF_THROW_IF(chunk_list_index_[head_chunk] != list_index, InvalidState,
                      "Head chunk does not belong to requested freelist");
    }
  }
  if (!current_chunk) {
    return {};
  }

  if (!freelist_launcher_.initialized(device_handle_)) {
    ResourcePrivateOps::setManager(library_manager_);
    freelist_launcher_.initialize<ResourcePrivateOps>(device_handle_);
  }

  ORTEAF_THROW_IF(!launch_params.command_queue, InvalidParameter,
                  "CommandQueueLease must be provided for MPS freelist ops");
  auto *command_queue = launch_params.command_queue.pointer();
  ORTEAF_THROW_IF_NULL(command_queue, "Failed to create MPS command queue");
  auto *command_buffer = freelist_launcher_.createCommandBuffer(command_queue);
  ORTEAF_THROW_IF_NULL(command_buffer, "Failed to create MPS command buffer");
  auto *encoder = freelist_launcher_.createComputeEncoder(command_buffer,
                                                          device_handle_, 1);
  ORTEAF_THROW_IF_NULL(encoder,
                       "Failed to create compute encoder for freelist_pop");

  freelist_launcher_.setBuffer(encoder, list.head, 0, 0); // head_offset
  freelist_launcher_.setBuffer(encoder, list.head, sizeof(uint32_t),
                               1); // head_chunk_id
  freelist_launcher_.setBuffer(encoder, current_chunk.raw(),
                               current_chunk.offset(), 2);
  freelist_launcher_.setBuffer(encoder, list.out, 0, 3);

  ::orteaf::internal::runtime::mps::platform::wrapper::MPSSize_t tg{1, 1, 1};
  freelist_launcher_.dispatchThreadgroups(encoder, tg, tg);
  freelist_launcher_.endEncoding(encoder);

  freelist_launcher_.commit(command_buffer);
  ::orteaf::internal::runtime::mps::platform::wrapper::waitUntilCompleted(
      command_buffer);
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyCommandBuffer(
      command_buffer);
  if (created_queue) {
    ::orteaf::internal::runtime::mps::platform::wrapper::destroyCommandQueue(
        command_queue);
  }

  auto *out_ptr = static_cast<uint32_t *>(
      ::orteaf::internal::runtime::mps::platform::wrapper::getBufferContents(
          list.out));
  const uint32_t index = out_ptr ? out_ptr[0] : kInvalidIndex;
  const uint32_t chunk_id = out_ptr ? out_ptr[1] : kInvalidIndex;

  if (index == kInvalidIndex) {
    return {};
  }

  ORTEAF_THROW_IF(chunk_id >= chunks_.size(), InvalidState,
                  "Invalid chunk_id returned from freelist");
  ORTEAF_THROW_IF(chunk_list_index_[chunk_id] != list_index, InvalidState,
                  "Popped block belongs to different freelist");
  const BufferView chunk = chunks_[chunk_id];
  const std::size_t offset = chunk.offset() + static_cast<std::size_t>(index);
  return BufferView{chunk.raw(), offset, freelists_[list_index].block_size};
}

void MpsResource::pushFreelistNode(std::size_t list_index, BufferView view,
                                   const LaunchParams &launch_params) {
  ORTEAF_THROW_IF(!initialized_, InvalidState,
                  "MpsResource::pushFreelistNode before initialize");
  if (list_index >= freelists_.size()) {
    return;
  }
  auto &list = freelists_[list_index];
  if (!list.head || !view) {
    return;
  }
  auto it = chunk_lookup_.find(view.raw());
  ORTEAF_THROW_IF(it == chunk_lookup_.end(), InvalidParameter,
                  "View must belong to a registered chunk");
  const uint32_t chunk_id = it->second;
  ORTEAF_THROW_IF(chunk_id >= chunk_list_index_.size(), InvalidState,
                  "Chunk table out of sync");
  ORTEAF_THROW_IF(chunk_list_index_[chunk_id] != list_index, InvalidParameter,
                  "View belongs to different freelist");
  const BufferView chunk = chunks_[chunk_id];
  const std::size_t chunk_size =
      (chunk_id < chunk_sizes_.size()) ? chunk_sizes_[chunk_id] : chunk.size();
  ORTEAF_THROW_IF(!chunk.contains(view, view.size()), InvalidParameter,
                  "View must belong to freelist chunk");
  ORTEAF_THROW_IF(view.offset() + view.size() >
                      chunk.offset() + static_cast<std::size_t>(chunk_size),
                  InvalidParameter, "View exceeds freelist chunk bounds");

  ORTEAF_THROW_IF(list.block_size == 0, InvalidState,
                  "Freelist block size must be set");
  const std::size_t diff = view.offset() - chunk.offset();
  ORTEAF_THROW_IF(diff % list.block_size != 0, InvalidParameter,
                  "View not aligned to block size");
  ORTEAF_THROW_IF(
      diff > std::numeric_limits<uint32_t>::max(), InvalidParameter,
      "Block offset exceeds 32-bit range required by freelist kernels");
  const uint32_t index = static_cast<uint32_t>(diff); // Offset in bytes

  if (!freelist_launcher_.initialized(device_handle_)) {
    ResourcePrivateOps::setManager(library_manager_);
    freelist_launcher_.initialize<ResourcePrivateOps>(device_handle_);
  }

  if (launch_params.device && launch_params.device.pointer() != device_) {
    ORTEAF_THROW(InvalidParameter, "DeviceLease does not match resource device");
  }
  ORTEAF_THROW_IF(!launch_params.command_queue, InvalidParameter,
                  "CommandQueueLease must be provided for MPS freelist ops");
  auto *command_queue = launch_params.command_queue.pointer();
  ORTEAF_THROW_IF_NULL(command_queue, "Failed to create MPS command queue");
  auto *command_buffer = freelist_launcher_.createCommandBuffer(command_queue);
  ORTEAF_THROW_IF_NULL(command_buffer, "Failed to create MPS command buffer");
  auto *encoder = freelist_launcher_.createComputeEncoder(command_buffer,
                                                          device_handle_, 2);
  ORTEAF_THROW_IF_NULL(encoder,
                       "Failed to create compute encoder for freelist_push");

  freelist_launcher_.setBuffer(encoder, list.head, 0, 0); // head_offset
  freelist_launcher_.setBuffer(encoder, list.head, sizeof(uint32_t),
                               1); // head_chunk_id
  freelist_launcher_.setBuffer(encoder, chunk.raw(), chunk.offset(), 2);
  freelist_launcher_.setBytes(encoder, &index, sizeof(uint32_t), 3);
  freelist_launcher_.setBytes(encoder, &chunk_id, sizeof(uint32_t), 4);

  ::orteaf::internal::runtime::mps::platform::wrapper::MPSSize_t tg{1, 1, 1};
  freelist_launcher_.dispatchThreadgroups(encoder, tg, tg);
  freelist_launcher_.endEncoding(encoder);
  freelist_launcher_.commit(command_buffer);
  ::orteaf::internal::runtime::mps::platform::wrapper::waitUntilCompleted(
      command_buffer);
  ::orteaf::internal::runtime::mps::platform::wrapper::destroyCommandBuffer(
      command_buffer);
}

void MpsResource::destroyFreelist() {
  for (auto &list : freelists_) {
    if (list.head) {
      ::orteaf::internal::runtime::mps::platform::wrapper::destroyBuffer(
          list.head);
      list.head = nullptr;
    }
    if (list.out) {
      ::orteaf::internal::runtime::mps::platform::wrapper::destroyBuffer(
          list.out);
      list.out = nullptr;
    }
    list.block_size = 0;
    list.block_count = 0;
  }
  freelists_.clear();
  chunks_.clear();
  chunk_sizes_.clear();
  chunk_list_index_.clear();
  chunk_lookup_.clear();
}

void MpsResource::ensureList(std::size_t list_index) {
  if (list_index >= freelists_.size()) {
    freelists_.resize(list_index + 1);
  }
}

} // namespace orteaf::internal::backend::mps

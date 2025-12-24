#pragma once

#if ORTEAF_ENABLE_MPS

#include <cstddef>
#include <cstdint>

#include "orteaf/internal/execution/mps/platform/wrapper/mps_types.h"

namespace orteaf::internal::execution::mps::platform::wrapper {

enum class MpsGraphDataType : std::uint32_t {
  kInvalid = 0,
  kFloat16,
  kFloat32,
  kInt32,
  kInt64,
  kBool,
};

struct MpsGraphFeed {
  MpsGraphTensor_t tensor{nullptr};
  MpsGraphTensorData_t data{nullptr};
};

MpsGraph_t createGraph();

void destroyGraph(MpsGraph_t graph);

MpsGraphTensorData_t
createGraphTensorDataFromBuffer(MpsBuffer_t buffer, const std::int64_t* shape,
                                std::size_t shape_rank,
                                MpsGraphDataType data_type);

void destroyGraphTensorData(MpsGraphTensorData_t data);

MpsGraphExecutable_t compileGraph(
    MpsGraph_t graph, MpsDevice_t device, const MpsGraphFeed* feeds,
    std::size_t feed_count, const MpsGraphTensor_t* target_tensors,
    std::size_t target_tensor_count,
    const MpsGraphOperation_t* target_operations,
    std::size_t target_operation_count);

std::size_t runGraphExecutable(
    MpsGraphExecutable_t executable, MpsCommandQueue_t command_queue,
    const MpsGraphFeed* feeds, std::size_t feed_count,
    const MpsGraphTensor_t* target_tensors, std::size_t target_tensor_count,
    const MpsGraphOperation_t* target_operations,
    std::size_t target_operation_count,
    MpsGraphTensorData_t* out_tensor_data, std::size_t out_capacity);

void destroyGraphExecutable(MpsGraphExecutable_t executable);

}  // namespace orteaf::internal::execution::mps::platform::wrapper

#endif  // ORTEAF_ENABLE_MPS

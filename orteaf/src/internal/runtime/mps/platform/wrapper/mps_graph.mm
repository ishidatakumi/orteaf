#ifndef __OBJC__
#error                                                                         \
    "mps_graph.mm must be compiled with an Objective-C++ compiler (__OBJC__ not defined)"
#endif

#include "orteaf/internal/runtime/mps/platform/wrapper/mps_graph.h"

#if ORTEAF_ENABLE_MPS

#import <Metal/Metal.h>
#import <MetalPerformanceShadersGraph/MetalPerformanceShadersGraph.h>

#include <vector>

#include "orteaf/internal/diagnostics/error/error.h"
#include "orteaf/internal/runtime/mps/platform/wrapper/mps_objc_bridge.h"

namespace orteaf::internal::runtime::mps::platform::wrapper {

namespace {

MPSDataType toMpsDataType(MpsGraphDataType type) {
  switch (type) {
  case MpsGraphDataType::kFloat16:
    return MPSDataTypeFloat16;
  case MpsGraphDataType::kFloat32:
    return MPSDataTypeFloat32;
  case MpsGraphDataType::kInt32:
    return MPSDataTypeInt32;
  case MpsGraphDataType::kInt64:
    return MPSDataTypeInt64;
  case MpsGraphDataType::kBool:
    return MPSDataTypeBool;
  case MpsGraphDataType::kInvalid:
  default:
    break;
  }
  using namespace ::orteaf::internal::diagnostics::error;
  throwError(OrteafErrc::InvalidArgument, "Unsupported MPSGraph data type");
}

NSArray<NSNumber *> *toShapeArray(const std::int64_t *shape, std::size_t rank) {
  if (shape == nullptr && rank != 0) {
    using namespace ::orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::InvalidArgument, "Shape pointer is null");
  }
  NSMutableArray<NSNumber *> *dims =
      [[NSMutableArray alloc] initWithCapacity:rank];
  for (std::size_t i = 0; i < rank; ++i) {
    [dims addObject:@(shape[i])];
  }
  return dims;
}

NSDictionary<MPSGraphTensor *, MPSGraphTensorData *> *
buildFeedsDictionary(const MpsGraphFeed *feeds, std::size_t feed_count) {
  if (feeds == nullptr && feed_count != 0) {
    using namespace ::orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::InvalidArgument, "Feeds pointer is null");
  }
  NSMutableDictionary<MPSGraphTensor *, MPSGraphTensorData *> *dict =
      [[NSMutableDictionary alloc] initWithCapacity:feed_count];
  for (std::size_t i = 0; i < feed_count; ++i) {
    const auto &feed = feeds[i];
    if (feed.tensor == nullptr || feed.data == nullptr) {
      using namespace ::orteaf::internal::diagnostics::error;
      throwError(OrteafErrc::InvalidArgument,
                 "Feed tensor or data cannot be null");
    }
    MPSGraphTensor *key = objcFromOpaqueNoown<MPSGraphTensor *>(feed.tensor);
    MPSGraphTensorData *value =
        objcFromOpaqueNoown<MPSGraphTensorData *>(feed.data);
    dict[key] = value;
  }
  return dict;
}

NSArray<MPSGraphTensor *> *buildTensorArray(const MpsGraphTensor_t *tensors,
                                            std::size_t count) {
  NSMutableArray<MPSGraphTensor *> *array =
      [[NSMutableArray alloc] initWithCapacity:count];
  for (std::size_t i = 0; i < count; ++i) {
    if (tensors[i] == nullptr) {
      using namespace ::orteaf::internal::diagnostics::error;
      throwError(OrteafErrc::InvalidArgument, "Target tensor cannot be null");
    }
    [array addObject:objcFromOpaqueNoown<MPSGraphTensor *>(tensors[i])];
  }
  return array;
}

NSArray<MPSGraphOperation *> *
buildOperationArray(const MpsGraphOperation_t *operations, std::size_t count) {
  NSMutableArray<MPSGraphOperation *> *array =
      [[NSMutableArray alloc] initWithCapacity:count];
  for (std::size_t i = 0; i < count; ++i) {
    if (operations[i] == nullptr) {
      using namespace ::orteaf::internal::diagnostics::error;
      throwError(OrteafErrc::InvalidArgument,
                 "Target operation cannot be null");
    }
    [array addObject:objcFromOpaqueNoown<MPSGraphOperation *>(operations[i])];
  }
  return array;
}

} // namespace

MpsGraph_t createGraph() {
  MPSGraph *graph = [[MPSGraph alloc] init];
  return (MpsGraph_t)opaqueFromObjcRetained(graph);
}

void destroyGraph(MpsGraph_t graph) {
  if (graph == nullptr) {
    return;
  }
  opaqueReleaseRetained(graph);
}

MpsGraphTensorData_t
createGraphTensorDataFromBuffer(MpsBuffer_t buffer, const std::int64_t *shape,
                                std::size_t shape_rank,
                                MpsGraphDataType data_type) {
  if (buffer == nullptr) {
    using namespace ::orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::InvalidArgument,
               "Cannot create tensor data from null buffer");
  }
  id<MTLBuffer> objc_buffer = objcFromOpaqueNoown<id<MTLBuffer>>(buffer);
  NSArray<NSNumber *> *shape_array = toShapeArray(shape, shape_rank);
  MPSGraphTensorData *tensor_data =
      [[MPSGraphTensorData alloc] initWithMTLBuffer:objc_buffer
                                              shape:shape_array
                                           dataType:toMpsDataType(data_type)];
  [shape_array release];
  return (MpsGraphTensorData_t)opaqueFromObjcRetained(tensor_data);
}

void destroyGraphTensorData(MpsGraphTensorData_t data) {
  if (data == nullptr) {
    return;
  }
  opaqueReleaseRetained(data);
}

MpsGraphExecutable_t compileGraph(MpsGraph_t graph, MpsDevice_t device,
                                  const MpsGraphFeed *feeds,
                                  std::size_t feed_count,
                                  const MpsGraphTensor_t *target_tensors,
                                  std::size_t target_tensor_count,
                                  const MpsGraphOperation_t *target_operations,
                                  std::size_t target_operation_count) {
  if (graph == nullptr || device == nullptr) {
    using namespace ::orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::InvalidArgument,
               "Graph and device must be non-null for compilation");
  }
  MPSGraph *objc_graph = objcFromOpaqueNoown<MPSGraph *>(graph);
  id<MTLDevice> objc_device = objcFromOpaqueNoown<id<MTLDevice>>(device);

  NSArray<MPSGraphTensor *> *targets =
      buildTensorArray(target_tensors, target_tensor_count);
  NSArray<MPSGraphOperation *> *operations =
      buildOperationArray(target_operations, target_operation_count);
  NSDictionary<MPSGraphTensor *, MPSGraphTensorData *> *objc_feeds =
      buildFeedsDictionary(feeds, feed_count);

  NSError *error = nil;
  MPSGraphExecutable *executable = [objc_graph compileWithDevice:objc_device
                                                           feeds:objc_feeds
                                                   targetTensors:targets
                                                targetOperations:operations
                                                           error:&error];
  [targets release];
  [operations release];
  [objc_feeds release];

  if (error != nil || executable == nil) {
    using namespace ::orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::OperationFailed,
               "Failed to compile MPSGraph executable");
  }
  return (MpsGraphExecutable_t)opaqueFromObjcRetained(executable);
}

std::size_t runGraphExecutable(
    MpsGraphExecutable_t executable, MpsCommandQueue_t command_queue,
    const MpsGraphFeed *feeds, std::size_t feed_count,
    const MpsGraphTensor_t *target_tensors, std::size_t target_tensor_count,
    const MpsGraphOperation_t *target_operations,
    std::size_t target_operation_count, MpsGraphTensorData_t *out_tensor_data,
    std::size_t out_capacity) {
  if (executable == nullptr || command_queue == nullptr) {
    using namespace ::orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::InvalidArgument,
               "Executable and command queue must be non-null for run");
  }
  if (out_tensor_data == nullptr || out_capacity < target_tensor_count) {
    using namespace ::orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::InvalidArgument,
               "Output buffer is null or too small for targets");
  }

  MPSGraphExecutable *objc_executable =
      objcFromOpaqueNoown<MPSGraphExecutable *>(executable);
  id<MTLCommandQueue> objc_queue =
      objcFromOpaqueNoown<id<MTLCommandQueue>>(command_queue);

  NSArray<MPSGraphTensor *> *targets =
      buildTensorArray(target_tensors, target_tensor_count);
  NSArray<MPSGraphOperation *> *operations =
      buildOperationArray(target_operations, target_operation_count);
  NSDictionary<MPSGraphTensor *, MPSGraphTensorData *> *objc_feeds =
      buildFeedsDictionary(feeds, feed_count);

  NSError *error = nil;
  NSArray<MPSGraphTensorData *> *results =
      [objc_executable runWithMTLCommandQueue:objc_queue
                                        feeds:objc_feeds
                                targetTensors:targets
                             targetOperations:operations
                          executionDescriptor:nil
                                        error:&error];

  [targets release];
  [operations release];
  [objc_feeds release];

  if (error != nil || results == nil || [results count] < target_tensor_count) {
    using namespace ::orteaf::internal::diagnostics::error;
    throwError(OrteafErrc::OperationFailed,
               "Failed to run MPSGraph executable: insufficient outputs");
  }

  const std::size_t copy_count =
      std::min<std::size_t>([results count], target_tensor_count);
  for (std::size_t i = 0; i < copy_count; ++i) {
    MPSGraphTensorData *tensor_data = [results objectAtIndex:i];
    out_tensor_data[i] =
        (MpsGraphTensorData_t)opaqueFromObjcRetained(tensor_data);
  }
  return copy_count;
}

void destroyGraphExecutable(MpsGraphExecutable_t executable) {
  if (executable == nullptr) {
    return;
  }
  opaqueReleaseRetained(executable);
}

} // namespace orteaf::internal::runtime::mps::platform::wrapper

#endif // ORTEAF_ENABLE_MPS

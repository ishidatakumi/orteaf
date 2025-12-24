#pragma once

namespace orteaf::internal::execution::cuda::resource {

struct FenceToken {
  void *value;
};

struct ReuseToken {
  void *value;
};

} // namespace orteaf::internal::execution::cuda::resource

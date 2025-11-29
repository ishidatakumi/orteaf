#pragma once

namespace orteaf::internal::backend::cpu {

// CPU tokens are no-ops; they exist so the allocator interface can stay consistent.
struct FenceToken {};
struct ReuseToken {};

}  // namespace orteaf::internal::backend::cpu

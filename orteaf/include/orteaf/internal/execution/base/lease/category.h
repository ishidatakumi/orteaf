#pragma once

namespace orteaf::internal::execution::base::lease_category {

/// @brief Weak category - weak references only (no strong ownership)
struct Weak {};

/// @brief Strong category - shared ownership with strong count
struct Strong {};

/// @brief Shared category - shared ownership with weak reference support
struct Shared {};

} // namespace orteaf::internal::execution::base::lease_category

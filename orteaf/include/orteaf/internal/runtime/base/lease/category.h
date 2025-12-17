#pragma once

namespace orteaf::internal::runtime::base::lease_category {

/// @brief Raw category - no reference counting, payload only
struct Raw {};

/// @brief Unique category - single ownership with in_use flag
struct Unique {};

/// @brief Shared category - shared ownership with strong count
struct Shared {};

/// @brief WeakUnique category - unique ownership with weak reference support
struct WeakUnique {};

/// @brief WeakShared category - shared ownership with weak reference support
struct WeakShared {};

} // namespace orteaf::internal::runtime::base::lease_category

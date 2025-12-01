#pragma once

#include <utility>

#include "orteaf/internal/backend/backend_traits.h"
#include "orteaf/internal/base/handle.h"

namespace orteaf::internal::runtime::allocator {

using BufferId = ::orteaf::internal::base::BufferId;

// Non-owning view of a buffer with an associated strong ID.
template <backend::Backend B>
struct MemoryBlock {
    using BufferView = typename backend::BackendTraits<B>::BufferView;

    BufferId id{};
    BufferView view{};

    MemoryBlock() = default;
    MemoryBlock(BufferId id, BufferView view)
        : id(id), view(std::move(view)) {}

    bool valid() const { return id.isValid() && static_cast<bool>(view); }
};

}  // namespace orteaf::internal::runtime::allocator

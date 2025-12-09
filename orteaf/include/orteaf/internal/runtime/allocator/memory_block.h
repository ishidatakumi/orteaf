#pragma once

#include <utility>

#include "orteaf/internal/base/handle.h"
#include "orteaf/internal/runtime/base/backend_traits.h"

namespace orteaf::internal::runtime::allocator {

using BufferViewHandle = ::orteaf::internal::base::BufferViewHandle;

// Non-owning view of a buffer with an associated strong ID.
template <backend::Backend B> struct BufferResource {
  using BufferView = typename base::BackendTraits<B>::BufferView;

  BufferViewHandle handle{};
  BufferView view{};

  BufferResource() = default;
  BufferResource(BufferViewHandle handle, BufferView view)
      : handle(handle), view(std::move(view)) {}

  bool valid() const { return handle.isValid() && static_cast<bool>(view); }
};

} // namespace orteaf::internal::runtime::allocator

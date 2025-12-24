#pragma once

#include <concepts>
#include <cstddef>

namespace orteaf::internal::execution::base::pool {

/**
 * @brief PayloadPoolの共通APIを定義するConcept
 *
 * SlotPoolとFixedSlotStoreの両方がこのConceptを満たすことで、
 * BasePoolManagerCoreから統一的に使用できる。
 */
template <typename Pool>
concept PayloadPoolConcept =
    requires(Pool &pool, const Pool &const_pool,
             const typename Pool::Request &request,
             const typename Pool::Context &context,
             typename Pool::Handle handle, std::size_t size) {
      // Type requirements
      typename Pool::Payload;
      typename Pool::Handle;
      typename Pool::Request;
      typename Pool::Context;
      typename Pool::SlotRef;

      // SlotRef requirements
      { typename Pool::SlotRef{}.valid() } -> std::convertible_to<bool>;
      {
        typename Pool::SlotRef{}.handle
      } -> std::convertible_to<typename Pool::Handle>;
      {
        typename Pool::SlotRef{}.payload_ptr
      } -> std::convertible_to<typename Pool::Payload *>;

      // Size and state operations
      { const_pool.size() } -> std::convertible_to<std::size_t>;
      { pool.resize(size) } -> std::convertible_to<std::size_t>;
      { const_pool.isValid(handle) } -> std::convertible_to<bool>;
      { const_pool.isCreated(handle) } -> std::convertible_to<bool>;

      // Payload access
      { pool.get(handle) } -> std::same_as<typename Pool::Payload *>;
      {
        const_pool.get(handle)
      } -> std::same_as<const typename Pool::Payload *>;

      // Creation and destruction
      { pool.emplace(handle, request, context) } -> std::convertible_to<bool>;
    };

/**
 * @brief 作成済みスロットの取得をサポートするPoolのConcept
 */
template <typename Pool>
concept CreatedSlotAcquirable =
    PayloadPoolConcept<Pool> &&
    requires(Pool &pool, const typename Pool::Request &request,
             const typename Pool::Context &context) {
      {
        pool.tryAcquireCreated(request, context)
      } -> std::same_as<typename Pool::SlotRef>;
    };

/**
 * @brief 未作成スロットの予約をサポートするPoolのConcept
 */
template <typename Pool>
concept UncreatedSlotReservable =
    PayloadPoolConcept<Pool> &&
    requires(Pool &pool, const typename Pool::Request &request,
             const typename Pool::Context &context) {
      {
        pool.tryReserveUncreated(request, context)
      } -> std::same_as<typename Pool::SlotRef>;
    };

} // namespace orteaf::internal::execution::base::pool

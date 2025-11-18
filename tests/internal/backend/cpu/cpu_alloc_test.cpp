/**
 * @file cpu_alloc_test.cpp
 * @brief Tests for CPU memory allocation functions.
 */

#include "orteaf/internal/backend/cpu/cpu_alloc.h"

#include "tests/internal/testing/error_assert.h"

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

namespace cpu = orteaf::internal::backend::cpu;

/**
 * @brief Test that alloc returns valid pointer.
 */
TEST(CpuAlloc, AllocReturnsValidPointer) {
    void* ptr = cpu::alloc(1024);
    EXPECT_NE(ptr, nullptr);
    
    cpu::dealloc(ptr, 1024);
}

/**
 * @brief Test that alloc with zero size throws.
 */
TEST(CpuAlloc, AllocZeroSizeReturnsNullptr) {
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
        [] { cpu::alloc(0); });
}

/**
 * @brief Test that allocated memory is writable.
 */
TEST(CpuAlloc, AllocatedMemoryIsWritable) {
    constexpr size_t size = sizeof(int);
    void* ptr = cpu::alloc(size);
    ASSERT_NE(ptr, nullptr);
    
    int* int_ptr = static_cast<int*>(ptr);
    *int_ptr = 42;
    EXPECT_EQ(*int_ptr, 42);
    
    cpu::dealloc(ptr, size);
}

/**
 * @brief Test that allocated memory is aligned correctly.
 */
TEST(CpuAlloc, AllocatedMemoryIsAligned) {
    constexpr size_t size = 1024;
    void* ptr = cpu::alloc(size);
    ASSERT_NE(ptr, nullptr);
    
    // Check alignment (should be aligned to kCpuDefaultAlign)
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    EXPECT_EQ(addr % cpu::kCpuDefaultAlign, 0);
    
    cpu::dealloc(ptr, size);
}

/**
 * @brief Test that alloc_aligned works with various alignments.
 */
TEST(CpuAlloc, AllocAlignedWorksWithVariousAlignments) {
    std::vector<size_t> alignments = {8, 16, 32, 64, 128, 256, 512, 1024};
    
    for (size_t alignment : alignments) {
        void* ptr = cpu::allocAligned(1024, alignment);
        ASSERT_NE(ptr, nullptr);
        
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        EXPECT_EQ(addr % alignment, 0);
        
        cpu::dealloc(ptr, 1024);
    }
}

/**
 * @brief Test that alloc_aligned adjusts non-power-of-2 alignment.
 */
TEST(CpuAlloc, AllocAlignedAdjustsNonPowerOfTwoAlignment) {
    // Non-power-of-2 alignment should be adjusted to next power of 2
    void* ptr = cpu::allocAligned(1024, 9);  // 9 -> 16
    ASSERT_NE(ptr, nullptr);
    
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    EXPECT_EQ(addr % 16, 0);  // Should be aligned to 16 (next pow2 of 9)
    
    cpu::dealloc(ptr, 1024);
}

/**
 * @brief Test that alloc_aligned adjusts alignment smaller than max_align_t.
 */
TEST(CpuAlloc, AllocAlignedAdjustsSmallAlignment) {
    // Alignment smaller than max_align_t should be adjusted
    void* ptr = cpu::allocAligned(1024, 1);  // Should be adjusted to max_align_t
    ASSERT_NE(ptr, nullptr);
    
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    EXPECT_EQ(addr % alignof(std::max_align_t), 0);
    
    cpu::dealloc(ptr, 1024);
}

/**
 * @brief Test that alloc_aligned with zero size throws.
 */
TEST(CpuAlloc, AllocAlignedZeroSizeReturnsNullptr) {
    ::orteaf::tests::ExpectError(
        ::orteaf::internal::diagnostics::error::OrteafErrc::InvalidParameter,
        [] { cpu::allocAligned(0, 16); });
}

/**
 * @brief Test that dealloc works correctly.
 */
TEST(CpuAlloc, DeallocWorksCorrectly) {
    void* ptr = cpu::alloc(1024);
    ASSERT_NE(ptr, nullptr);
    
    EXPECT_NO_THROW(cpu::dealloc(ptr, 1024));
}

/**
 * @brief Test that dealloc with nullptr is a no-op.
 */
TEST(CpuAlloc, DeallocNullptrIsNoOp) {
    EXPECT_NO_THROW(cpu::dealloc(nullptr, 0));
    EXPECT_NO_THROW(cpu::dealloc(nullptr, 1024));
}

/**
 * @brief Test that multiple allocations work.
 */
TEST(CpuAlloc, MultipleAllocationsWork) {
    constexpr size_t size = 256;
    constexpr int num_allocs = 10;
    
    std::vector<void*> ptrs;
    for (int i = 0; i < num_allocs; ++i) {
        void* ptr = cpu::alloc(size);
        EXPECT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }
    
    // Verify all pointers are unique
    for (size_t i = 0; i < ptrs.size(); ++i) {
        for (size_t j = i + 1; j < ptrs.size(); ++j) {
            EXPECT_NE(ptrs[i], ptrs[j]);
        }
    }
    
    // Deallocate all
    for (auto ptr : ptrs) {
        cpu::dealloc(ptr, size);
    }
}

/**
 * @brief Test that allocations with different sizes work.
 */
TEST(CpuAlloc, AllocationsWithDifferentSizesWork) {
    std::vector<size_t> sizes = {1, 4, 16, 64, 256, 1024, 4096, 16384};
    
    for (size_t size : sizes) {
        void* ptr = cpu::alloc(size);
        EXPECT_NE(ptr, nullptr);
        cpu::dealloc(ptr, size);
    }
}

/**
 * @brief Test that memory can be written and read correctly.
 */
TEST(CpuAlloc, MemoryWriteAndRead) {
    constexpr size_t size = 1024;
    void* ptr = cpu::alloc(size);
    ASSERT_NE(ptr, nullptr);
    
    // Write pattern
    uint8_t* byte_ptr = static_cast<uint8_t*>(ptr);
    for (size_t i = 0; i < size; ++i) {
        byte_ptr[i] = static_cast<uint8_t>(i % 256);
    }
    
    // Read and verify
    for (size_t i = 0; i < size; ++i) {
        EXPECT_EQ(byte_ptr[i], static_cast<uint8_t>(i % 256));
    }
    
    cpu::dealloc(ptr, size);
}

/**
 * @brief Test that large allocations work.
 */
TEST(CpuAlloc, LargeAllocationsWork) {
    constexpr size_t large_size = 10 * 1024 * 1024;  // 10 MB
    void* ptr = cpu::alloc(large_size);
    ASSERT_NE(ptr, nullptr);
    
    // Verify it's writable
    memset(ptr, 0xAB, large_size);
    
    uint8_t* byte_ptr = static_cast<uint8_t*>(ptr);
    EXPECT_EQ(byte_ptr[0], 0xAB);
    EXPECT_EQ(byte_ptr[large_size - 1], 0xAB);
    
    cpu::dealloc(ptr, large_size);
}

/**
 * @brief Test that very large alignment works.
 */
TEST(CpuAlloc, VeryLargeAlignmentWorks) {
    constexpr size_t alignment = 4096;  // Page size
    constexpr size_t size = 1024;
    
    void* ptr = cpu::allocAligned(size, alignment);
    ASSERT_NE(ptr, nullptr);
    
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    EXPECT_EQ(addr % alignment, 0);
    
    cpu::dealloc(ptr, size);
}

/**
 * @brief Test that statistics are updated correctly (indirect check).
 */
TEST(CpuAlloc, StatisticsUpdated) {
    constexpr size_t size = 1024;
    
    // Allocate (should update stats)
    void* ptr = cpu::alloc(size);
    ASSERT_NE(ptr, nullptr);
    
    // Deallocate (should update stats)
    cpu::dealloc(ptr, size);
    
    // Statistics should be updated (specific checks in cpu_stats_test.cpp)
}

/**
 * @brief Test that allocations can be reused after deallocation.
 */
TEST(CpuAlloc, AllocationsCanBeReused) {
    constexpr size_t size = 1024;
    
    // Allocate and deallocate multiple times
    for (int i = 0; i < 10; ++i) {
        void* ptr = cpu::alloc(size);
        ASSERT_NE(ptr, nullptr);
        cpu::dealloc(ptr, size);
    }
}

#include <gtest/gtest.h>
#include <random>
#include <thread>
#include <vector>
#include "atom/macro.hpp"
#include "atom/memory/memory.hpp"

using namespace atom::memory;

class MemoryPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

TEST_F(MemoryPoolTest, Constructor) {
    MemoryPool<int> pool;
    EXPECT_EQ(pool.getTotalAllocated(), 0);
    EXPECT_EQ(pool.getTotalAvailable(), 0);
}

TEST_F(MemoryPoolTest, AllocateAndDeallocate) {
    MemoryPool<int> pool;
    int* ptr = pool.allocate(10);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(pool.getTotalAllocated(), 10 * sizeof(int));
    EXPECT_EQ(pool.getTotalAvailable(), 4096 - 10 * sizeof(int));

    pool.deallocate(ptr, 10);
    EXPECT_EQ(pool.getTotalAllocated(), 0);
    EXPECT_EQ(pool.getTotalAvailable(), 4096);
}

TEST_F(MemoryPoolTest, AllocateExceedingBlockSize) {
    MemoryPool<int> pool;
    EXPECT_THROW(ATOM_UNUSED_RESULT(pool.allocate(4097)), MemoryPoolException);
}

TEST_F(MemoryPoolTest, Reset) {
    MemoryPool<int> pool;
    int* ptr = pool.allocate(10);
    EXPECT_NE(ptr, nullptr);
    pool.reset();
    EXPECT_EQ(pool.getTotalAllocated(), 0);
    EXPECT_EQ(pool.getTotalAvailable(), 0);
}

TEST_F(MemoryPoolTest, AllocateFromPool) {
    MemoryPool<int> pool;
    int* ptr1 = pool.allocate(10);
    int* ptr2 = pool.allocate(20);
    EXPECT_NE(ptr1, nullptr);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_EQ(pool.getTotalAllocated(), 30 * sizeof(int));
    EXPECT_EQ(pool.getTotalAvailable(), 4096 - 30 * sizeof(int));

    pool.deallocate(ptr1, 10);
    pool.deallocate(ptr2, 20);
    EXPECT_EQ(pool.getTotalAllocated(), 0);
    EXPECT_EQ(pool.getTotalAvailable(), 4096);
}

TEST_F(MemoryPoolTest, AllocateFromChunk) {
    MemoryPool<int> pool;
    int* ptr1 = pool.allocate(1024);
    int* ptr2 = pool.allocate(1024);
    EXPECT_NE(ptr1, nullptr);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_EQ(pool.getTotalAllocated(), 2048 * sizeof(int));
    EXPECT_EQ(pool.getTotalAvailable(), 4096 - 2048 * sizeof(int));

    pool.deallocate(ptr1, 1024);
    pool.deallocate(ptr2, 1024);
    EXPECT_EQ(pool.getTotalAllocated(), 0);
    EXPECT_EQ(pool.getTotalAvailable(), 4096);
}

TEST_F(MemoryPoolTest, ThreadSafety) {
    MemoryPool<int> pool;
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&pool]() {
            for (int j = 0; j < 100; ++j) {
                int* ptr = pool.allocate(10);
                pool.deallocate(ptr, 10);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(pool.getTotalAllocated(), 0);
    EXPECT_EQ(pool.getTotalAvailable(), 4096);
}

// Test for tagged allocations
TEST_F(MemoryPoolTest, TaggedAllocations) {
    MemoryPool<int> pool;
    int* ptr = pool.allocateTagged(5, "TestTag", "test_file.cpp", 42);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(pool.getTotalAllocated(), 5 * sizeof(int));

    auto tag = pool.findTag(ptr);
    ASSERT_TRUE(tag.has_value());
    EXPECT_EQ(tag->name, "TestTag");
    EXPECT_EQ(tag->file, "test_file.cpp");
    EXPECT_EQ(tag->line, 42);

    // Check if tag is removed after deallocation
    pool.deallocate(ptr, 5);
    auto tag_after = pool.findTag(ptr);
    EXPECT_FALSE(tag_after.has_value());
}

// Test for retrieving all tagged allocations
TEST_F(MemoryPoolTest, GetTaggedAllocations) {
    MemoryPool<int> pool;
    int* ptr1 = pool.allocateTagged(5, "Tag1", "file1.cpp", 10);
    int* ptr2 = pool.allocateTagged(10, "Tag2", "file2.cpp", 20);

    auto tags = pool.getTaggedAllocations();
    EXPECT_EQ(tags.size(), 2);
    EXPECT_EQ(tags[ptr1].name, "Tag1");
    EXPECT_EQ(tags[ptr2].name, "Tag2");

    pool.deallocate(ptr1, 5);
    pool.deallocate(ptr2, 10);
}

// Test for compacting memory to reduce fragmentation
TEST_F(MemoryPoolTest, Compact) {
    MemoryPool<int> pool;
    int* ptr1 = pool.allocate(10);
    int* ptr2 = pool.allocate(10);
    int* ptr3 = pool.allocate(10);

    // Deallocate ptr2 to create a gap
    pool.deallocate(ptr2, 10);

    // Compact the memory
    [[maybe_unused]] size_t bytes_compacted = pool.compact();

    // Verify we can still allocate and use the memory
    int* ptr4 = pool.allocate(10);
    EXPECT_NE(ptr4, nullptr);

    pool.deallocate(ptr1, 10);
    pool.deallocate(ptr3, 10);
    pool.deallocate(ptr4, 10);
}

// Test for fragmentation ratio calculation
TEST_F(MemoryPoolTest, FragmentationRatio) {
    MemoryPool<int> pool;

    // Initially no fragmentation
    EXPECT_DOUBLE_EQ(pool.getFragmentationRatio(), 0.0);

    // Allocate several blocks to create potential fragmentation
    int* ptr1 = pool.allocate(100);
    int* ptr2 = pool.allocate(200);
    int* ptr3 = pool.allocate(300);

    // Deallocate the middle block to create fragmentation
    pool.deallocate(ptr2, 200);

    // Check fragmentation ratio (should be non-zero)
    double ratio = pool.getFragmentationRatio();
    EXPECT_GE(ratio, 0.0);
    EXPECT_LE(ratio, 1.0);

    pool.deallocate(ptr1, 100);
    pool.deallocate(ptr3, 300);
}

// Test memory pool reserve functionality
TEST_F(MemoryPoolTest, Reserve) {
    MemoryPool<int> pool;
    size_t initial_available = pool.getTotalAvailable();

    // Reserve space for 1000 ints
    pool.reserve(1000);

    // Check that total available has increased
    EXPECT_GT(pool.getTotalAvailable(), initial_available);

    // Verify we can allocate reserved amount
    int* ptr = pool.allocate(1000);
    EXPECT_NE(ptr, nullptr);
    pool.deallocate(ptr, 1000);
}

// Test for memory block size strategy
TEST_F(MemoryPoolTest, BlockSizeStrategy) {
    // Define a custom strategy
    class ConstantSizeStrategy : public BlockSizeStrategy {
    public:
        [[nodiscard]] size_t calculate(size_t) const noexcept override {
            return 8192;  // Always return 8KB
        }
    };

    MemoryPool<int> pool(std::make_unique<ConstantSizeStrategy>());

    // Allocate a large block that should trigger a new chunk
    int* ptr = pool.allocate(1000);
    EXPECT_NE(ptr, nullptr);

    // Allocate a second large block to force another chunk
    int* ptr2 = pool.allocate(1000);
    EXPECT_NE(ptr2, nullptr);

    pool.deallocate(ptr, 1000);
    pool.deallocate(ptr2, 1000);
}

// Test for memory resource interface (std::pmr)
TEST_F(MemoryPoolTest, MemoryResourceInterface) {
    MemoryPool<std::byte, 4096> pool;
    std::pmr::memory_resource* mr = &pool;

    // Allocate through memory resource interface
    void* ptr = mr->allocate(100, alignof(std::max_align_t));
    EXPECT_NE(ptr, nullptr);

    // Check if allocation was tracked
    EXPECT_EQ(pool.getTotalAllocated(), 100);

    // Deallocate through memory resource interface
    mr->deallocate(ptr, 100, alignof(std::max_align_t));
    EXPECT_EQ(pool.getTotalAllocated(), 0);
}

// Test large allocations that require new chunks
TEST_F(MemoryPoolTest, LargeAllocations) {
    MemoryPool<int, 4096> pool;

    // Allocate close to block size to force new chunk allocation
    int* ptr1 = pool.allocate(1000);  // 4000 bytes (assuming sizeof(int)=4)
    EXPECT_NE(ptr1, nullptr);

    // This should require a new chunk
    int* ptr2 = pool.allocate(1000);
    EXPECT_NE(ptr2, nullptr);

    // Verify different chunks
    EXPECT_NE(reinterpret_cast<uintptr_t>(ptr1) / 4096,
              reinterpret_cast<uintptr_t>(ptr2) / 4096);

    pool.deallocate(ptr1, 1000);
    pool.deallocate(ptr2, 1000);
}

// Test custom alignment
TEST_F(MemoryPoolTest, CustomAlignment) {
    // Use 64-byte alignment
    MemoryPool<int, 4096, 64> pool;

    int* ptr = pool.allocate(10);
    EXPECT_NE(ptr, nullptr);

    // Check pointer is 64-byte aligned
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 64, 0);

    pool.deallocate(ptr, 10);
}

// Test move semantics
TEST_F(MemoryPoolTest, MoveConstructor) {
    MemoryPool<int> pool1;
    int* ptr = pool1.allocate(10);
    EXPECT_NE(ptr, nullptr);

    // Move construct another pool
    MemoryPool<int> pool2(std::move(pool1));

    // Original allocation should be tracked by the new pool
    EXPECT_EQ(pool2.getTotalAllocated(), 10 * sizeof(int));

    // Deallocate using the new pool
    pool2.deallocate(ptr, 10);
    EXPECT_EQ(pool2.getTotalAllocated(), 0);
}

// Test move assignment
TEST_F(MemoryPoolTest, MoveAssignment) {
    MemoryPool<int> pool1;
    int* ptr = pool1.allocate(10);

    MemoryPool<int> pool2;
    pool2 = std::move(pool1);

    // Original allocation should be tracked by the new pool
    EXPECT_EQ(pool2.getTotalAllocated(), 10 * sizeof(int));

    pool2.deallocate(ptr, 10);
}

// Test high load scenario
TEST_F(MemoryPoolTest, HighLoad) {
    MemoryPool<int> pool;
    std::vector<std::pair<int*, size_t>> allocations;

    // Make many small and medium allocations
    for (int i = 0; i < 100; ++i) {
        size_t size = 1 + (i % 20);  // Sizes from 1 to 20
        int* ptr = pool.allocate(size);
        EXPECT_NE(ptr, nullptr);
        allocations.emplace_back(ptr, size);
    }

    // Deallocate randomly to create fragmentation
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(allocations.begin(), allocations.end(), g);

    // Deallocate half of allocations
    for (size_t i = 0; i < allocations.size() / 2; ++i) {
        pool.deallocate(allocations[i].first, allocations[i].second);
    }

    // Try to make new allocations with varying sizes
    for (int i = 0; i < 50; ++i) {
        size_t size = 1 + (i % 30);  // Sizes from 1 to 30
        int* ptr = pool.allocate(size);
        EXPECT_NE(ptr, nullptr);
        allocations.push_back({ptr, size});
    }

    // Deallocate everything
    for (size_t i = allocations.size() / 2; i < allocations.size(); ++i) {
        pool.deallocate(allocations[i].first, allocations[i].second);
    }

    // Verify pool is empty
    EXPECT_EQ(pool.getTotalAllocated(), 0);
}

// Test exception safety
TEST_F(MemoryPoolTest, ExceptionSafety) {
    MemoryPool<int> pool;

    // Test exception is thrown for too large allocation
    EXPECT_THROW(
        {
            int* ptr = pool.allocate(2000);  // Should exceed default block size
            (void)ptr;                       // Avoid unused variable warning
        },
        atom::memory::MemoryPoolException);

    // Verify pool is in consistent state after exception
    int* ptr = pool.allocate(10);
    EXPECT_NE(ptr, nullptr);
    pool.deallocate(ptr, 10);
    EXPECT_EQ(pool.getTotalAllocated(), 0);
}

// Mock class for testing with complex types
class MockObject {
public:
    static size_t constructor_calls;
    static size_t destructor_calls;

    int data[100];

    MockObject() {
        constructor_calls++;
        std::fill_n(data, 100, 42);
    }

    ~MockObject() { destructor_calls++; }
};

size_t MockObject::constructor_calls = 0;
size_t MockObject::destructor_calls = 0;

// Test with complex object types
TEST_F(MemoryPoolTest, ComplexObjectAllocation) {
    // Reset static counters
    MockObject::constructor_calls = 0;
    MockObject::destructor_calls = 0;

    {
        MemoryPool<MockObject> pool;

        // Allocate memory for objects (doesn't call constructor)
        MockObject* obj = pool.allocate(5);
        EXPECT_NE(obj, nullptr);
        EXPECT_EQ(MockObject::constructor_calls, 0);

        // Construct objects manually
        for (size_t i = 0; i < 5; ++i) {
            new (&obj[i]) MockObject();
        }
        EXPECT_EQ(MockObject::constructor_calls, 5);

        // Check correct construction
        for (size_t i = 0; i < 5; ++i) {
            EXPECT_EQ(obj[i].data[50], 42);
        }

        // Destroy objects manually
        for (size_t i = 0; i < 5; ++i) {
            obj[i].~MockObject();
        }
        EXPECT_EQ(MockObject::destructor_calls, 5);

        // Deallocate memory
        pool.deallocate(obj, 5);
    }
}

// Test for memory leaks
TEST_F(MemoryPoolTest, MemoryLeakCheck) {
    // Create a scope to test destruction
    size_t expected_allocations = 0;
    {
        MemoryPool<int> pool;

        // Make several allocations without deallocating
        for (int i = 0; i < 10; ++i) {
            int* ptr = pool.allocate(10);
            expected_allocations += 10 * sizeof(int);
            // Purposefully not deallocating to test destructor cleanup
            (void)ptr;  // Avoid unused variable warning
        }

        // All memory should be allocated at this point
        EXPECT_EQ(pool.getTotalAllocated(), expected_allocations);

        // Pool destructor should clean up everything
    }

    // No way to directly verify cleanup after destruction, but we can
    // at least verify the test ran without memory errors
}

// Additional edge case tests
TEST_F(MemoryPoolTest, PMRInterfaceEdgeCases) {
    MemoryPool<std::byte, 4096> pool;
    std::pmr::memory_resource* mr = &pool;

    // Test with zero size allocation
    void* ptr1 = mr->allocate(0, alignof(std::max_align_t));
    EXPECT_NE(ptr1, nullptr);  // Should still return valid pointer
    mr->deallocate(ptr1, 0, alignof(std::max_align_t));

    // Test with very large alignment
    void* ptr2 = mr->allocate(64, 64);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr2) % 64, 0);
    mr->deallocate(ptr2, 64, 64);

    // Test equality comparison
    MemoryPool<std::byte, 4096> pool2;
    std::pmr::memory_resource* mr2 = &pool2;

    EXPECT_TRUE(mr->is_equal(*mr));   // Same resource
    EXPECT_FALSE(mr->is_equal(*mr2)); // Different resource
}

TEST_F(MemoryPoolTest, BlockSizeStrategyEdgeCases) {
    // Test with strategy that returns very small size
    class MinimalSizeStrategy : public BlockSizeStrategy {
    public:
        [[nodiscard]] size_t calculate(size_t requested_size) const noexcept override {
            return std::max(requested_size, static_cast<size_t>(64));
        }
    };

    MemoryPool<int> pool(std::make_unique<MinimalSizeStrategy>());

    // Should still work with minimal strategy
    int* ptr = pool.allocate(10);
    EXPECT_NE(ptr, nullptr);
    pool.deallocate(ptr, 10);

    // Test with strategy that returns very large size
    class LargeSizeStrategy : public BlockSizeStrategy {
    public:
        [[nodiscard]] size_t calculate(size_t) const noexcept override {
            return 1024 * 1024;  // 1MB
        }
    };

    MemoryPool<int> large_pool(std::make_unique<LargeSizeStrategy>());
    int* large_ptr = large_pool.allocate(10);
    EXPECT_NE(large_ptr, nullptr);
    large_pool.deallocate(large_ptr, 10);
}

TEST_F(MemoryPoolTest, TaggedAllocationEdgeCases) {
    MemoryPool<int> pool;

    // Test with empty tag information
    int* ptr1 = pool.allocateTagged(5, "", "", 0);
    EXPECT_NE(ptr1, nullptr);

    auto tag1 = pool.findTag(ptr1);
    ASSERT_TRUE(tag1.has_value());
    EXPECT_EQ(tag1->name, "");
    EXPECT_EQ(tag1->file, "");
    EXPECT_EQ(tag1->line, 0);

    // Test with very long tag information
    std::string long_name(1000, 'A');
    std::string long_file(1000, 'B');
    int* ptr2 = pool.allocateTagged(5, long_name, long_file, 999999);
    EXPECT_NE(ptr2, nullptr);

    auto tag2 = pool.findTag(ptr2);
    ASSERT_TRUE(tag2.has_value());
    EXPECT_EQ(tag2->name, long_name);
    EXPECT_EQ(tag2->file, long_file);
    EXPECT_EQ(tag2->line, 999999);

    pool.deallocate(ptr1, 5);
    pool.deallocate(ptr2, 5);
}

TEST_F(MemoryPoolTest, FragmentationEdgeCases) {
    MemoryPool<int> pool;

    // Test fragmentation with single allocation
    int* ptr = pool.allocate(10);
    double ratio1 = pool.getFragmentationRatio();
    EXPECT_DOUBLE_EQ(ratio1, 0.0);  // No fragmentation with single allocation

    pool.deallocate(ptr, 10);
    double ratio2 = pool.getFragmentationRatio();
    EXPECT_GE(ratio2, 0.0);
    EXPECT_LE(ratio2, 1.0);

    // Test with empty pool
    pool.reset();
    double ratio3 = pool.getFragmentationRatio();
    EXPECT_DOUBLE_EQ(ratio3, 0.0);  // No fragmentation in empty pool
}

TEST_F(MemoryPoolTest, ReserveEdgeCases) {
    MemoryPool<int> pool;

    // Reserve zero space
    pool.reserve(0);
    EXPECT_EQ(pool.getTotalAvailable(), 0);

    // Reserve space smaller than current available
    int* ptr = pool.allocate(10);
    size_t available_before = pool.getTotalAvailable();
    pool.reserve(5);  // Smaller than what we need
    size_t available_after = pool.getTotalAvailable();
    EXPECT_EQ(available_before, available_after);  // Should not change

    pool.deallocate(ptr, 10);
}

TEST_F(MemoryPoolTest, CompactEdgeCases) {
    MemoryPool<int> pool;

    // Compact empty pool
    size_t compacted1 = pool.compact();
    EXPECT_EQ(compacted1, 0);

    // Compact pool with single allocation
    int* ptr = pool.allocate(10);
    size_t compacted2 = pool.compact();
    EXPECT_EQ(compacted2, 0);  // Nothing to compact

    pool.deallocate(ptr, 10);

    // Compact pool with single free block
    size_t compacted3 = pool.compact();
    EXPECT_EQ(compacted3, 0);  // Single block, nothing to merge
}

// Error condition and exception tests
TEST_F(MemoryPoolTest, AllocationFailureScenarios) {
    MemoryPool<int> pool;

    // Test allocation that exceeds maximum block size
    EXPECT_THROW([[maybe_unused]] auto temp1 = pool.allocate(10000), atom::memory::MemoryPoolException);

    // Pool should remain functional after exception
    int* ptr = pool.allocate(10);
    EXPECT_NE(ptr, nullptr);
    pool.deallocate(ptr, 10);
}

TEST_F(MemoryPoolTest, PMRAllocationFailures) {
    MemoryPool<std::byte, 1024> pool;
    std::pmr::memory_resource* mr = &pool;

    // Test allocation that exceeds pool capacity
    EXPECT_THROW([[maybe_unused]] auto temp2 = mr->allocate(2000, alignof(std::max_align_t)),
                 atom::memory::MemoryPoolException);

    // Test with invalid alignment (very large)
    EXPECT_THROW([[maybe_unused]] auto temp3 = mr->allocate(100, 4096),
                 atom::memory::MemoryPoolException);

    // Pool should remain functional
    void* ptr = mr->allocate(100, alignof(std::max_align_t));
    EXPECT_NE(ptr, nullptr);
    mr->deallocate(ptr, 100, alignof(std::max_align_t));
}

TEST_F(MemoryPoolTest, TaggedAllocationFailures) {
    MemoryPool<int> pool;

    // Test tagged allocation that exceeds block size
    EXPECT_THROW([[maybe_unused]] auto temp4 = pool.allocateTagged(10000, "test", "file.cpp", 42),
                 atom::memory::MemoryPoolException);

    // Pool should remain functional
    int* ptr = pool.allocateTagged(10, "test", "file.cpp", 42);
    EXPECT_NE(ptr, nullptr);
    pool.deallocate(ptr, 10);
}

TEST_F(MemoryPoolTest, InvalidDeallocations) {
    MemoryPool<int> pool;

    // Deallocate null pointer (should not crash)
    pool.deallocate(nullptr, 10);

    // Deallocate with zero size (should not crash)
    int* ptr = pool.allocate(10);
    pool.deallocate(ptr, 0);  // This might be undefined behavior
    pool.deallocate(ptr, 10); // Proper deallocation
}

TEST_F(MemoryPoolTest, BlockSizeStrategyExceptions) {
    // Test with strategy that might throw (though it shouldn't)
    class ThrowingStrategy : public BlockSizeStrategy {
    public:
        [[nodiscard]] size_t calculate(size_t requested_size) const noexcept override {
            // Even though marked noexcept, test robustness
            return requested_size * 2;
        }
    };

    MemoryPool<int> pool(std::make_unique<ThrowingStrategy>());

    // Should work normally
    int* ptr = pool.allocate(10);
    EXPECT_NE(ptr, nullptr);
    pool.deallocate(ptr, 10);
}

TEST_F(MemoryPoolTest, ConcurrentExceptionSafety) {
    MemoryPool<int> pool;
    std::vector<std::thread> threads;
    std::atomic<int> exception_count{0};

    // Create threads that might cause exceptions
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&pool, &exception_count]() {
            for (int j = 0; j < 100; ++j) {
                try {
                    // Mix of valid and invalid allocations
                    if (j % 10 == 0) {
                        // This should throw
                        int* ptr = pool.allocate(10000);
                        pool.deallocate(ptr, 10000);
                    } else {
                        // This should succeed
                        int* ptr = pool.allocate(10);
                        pool.deallocate(ptr, 10);
                    }
                } catch (const atom::memory::MemoryPoolException&) {
                    exception_count++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should have caught some exceptions
    EXPECT_GT(exception_count.load(), 0);

    // Pool should still be functional
    int* ptr = pool.allocate(10);
    EXPECT_NE(ptr, nullptr);
    pool.deallocate(ptr, 10);
}

// Advanced thread safety and concurrency tests
TEST_F(MemoryPoolTest, ConcurrentAllocationDeallocation) {
    MemoryPool<int> pool;
    constexpr int numThreads = 8;
    constexpr int operationsPerThread = 500;
    std::vector<std::thread> threads;
    std::atomic<int> totalAllocations{0};
    std::atomic<int> totalDeallocations{0};

    // Create threads that continuously allocate and deallocate
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&pool, &totalAllocations, &totalDeallocations, operationsPerThread]() {
            std::vector<int*> localPtrs;
            localPtrs.reserve(operationsPerThread / 2);

            for (int j = 0; j < operationsPerThread; ++j) {
                if (j % 2 == 0) {
                    // Allocate
                    int* ptr = pool.allocate(10);
                    if (ptr) {
                        localPtrs.push_back(ptr);
                        totalAllocations++;
                    }
                } else if (!localPtrs.empty()) {
                    // Deallocate
                    int* ptr = localPtrs.back();
                    localPtrs.pop_back();
                    pool.deallocate(ptr, 10);
                    totalDeallocations++;
                }
            }

            // Clean up remaining allocations
            for (int* ptr : localPtrs) {
                pool.deallocate(ptr, 10);
                totalDeallocations++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all allocations were deallocated
    EXPECT_EQ(totalAllocations.load(), totalDeallocations.load());
    EXPECT_EQ(pool.getTotalAllocated(), 0);
}

TEST_F(MemoryPoolTest, ConcurrentTaggedOperations) {
    MemoryPool<int> pool;
    constexpr int numThreads = 4;
    constexpr int operationsPerThread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> taggedAllocations{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&pool, &taggedAllocations, operationsPerThread, i]() {
            std::vector<int*> localPtrs;

            for (int j = 0; j < operationsPerThread; ++j) {
                std::string tag = "Thread_" + std::to_string(i) + "_Alloc_" + std::to_string(j);
                std::string file = "test_file_" + std::to_string(i) + ".cpp";

                int* ptr = pool.allocateTagged(5, tag, file, j);
                if (ptr) {
                    localPtrs.push_back(ptr);
                    taggedAllocations++;

                    // Verify tag information
                    auto tagInfo = pool.findTag(ptr);
                    EXPECT_TRUE(tagInfo.has_value());
                    if (tagInfo) {
                        EXPECT_EQ(tagInfo->name, tag);
                        EXPECT_EQ(tagInfo->file, file);
                        EXPECT_EQ(tagInfo->line, j);
                    }
                }
            }

            // Clean up
            for (int* ptr : localPtrs) {
                pool.deallocate(ptr, 5);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(taggedAllocations.load(), numThreads * operationsPerThread);
    EXPECT_EQ(pool.getTotalAllocated(), 0);
}

TEST_F(MemoryPoolTest, ConcurrentCompaction) {
    MemoryPool<int> pool;
    constexpr int numThreads = 4;
    std::vector<std::thread> threads;
    std::atomic<bool> stopFlag{false};

    // Thread that continuously allocates and deallocates to create fragmentation
    threads.emplace_back([&pool, &stopFlag]() {
        std::vector<int*> ptrs;
        while (!stopFlag.load()) {
            // Allocate several blocks
            for (int i = 0; i < 10; ++i) {
                int* ptr = pool.allocate(10);
                if (ptr) {
                    ptrs.push_back(ptr);
                }
            }

            // Deallocate every other block to create fragmentation
            for (size_t i = 0; i < ptrs.size(); i += 2) {
                pool.deallocate(ptrs[i], 10);
                ptrs[i] = nullptr;
            }

            // Clean up remaining blocks
            for (int* ptr : ptrs) {
                if (ptr) {
                    pool.deallocate(ptr, 10);
                }
            }
            ptrs.clear();

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Threads that continuously compact the pool
    for (int i = 0; i < numThreads - 1; ++i) {
        threads.emplace_back([&pool, &stopFlag]() {
            while (!stopFlag.load()) {
                pool.compact();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    // Let threads run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stopFlag = true;

    for (auto& thread : threads) {
        thread.join();
    }

    // Pool should be in a consistent state
    int* ptr = pool.allocate(10);
    EXPECT_NE(ptr, nullptr);
    pool.deallocate(ptr, 10);
}

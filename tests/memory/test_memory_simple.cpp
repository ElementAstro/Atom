#include <gtest/gtest.h>
#include "atom/memory/memory.hpp"
#include "atom/memory/memory_pool.hpp"

using namespace atom::memory;

// Simplified memory test that avoids complex initialization
class SimpleMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Minimal setup without file I/O or complex initialization
    }

    void TearDown() override {
        // Minimal cleanup
    }
};

// Test basic memory pool functionality
TEST_F(SimpleMemoryTest, BasicMemoryPool) {
    // Use the simpler fixed-block memory pool
    atom::memory::FixedBlockPool<64> pool;

    // Test basic allocation
    void* ptr = pool.allocate();
    EXPECT_NE(ptr, nullptr);

    // Test deallocation
    pool.deallocate(ptr);

    // Test statistics
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.first, 0);  // allocated blocks should be 0
}

// Test memory pool with custom types using SimpleObjectPool
TEST_F(SimpleMemoryTest, CustomTypeMemoryPool) {
    struct TestStruct {
        int value;
        double data;
        TestStruct(int v = 0, double d = 0.0) : value(v), data(d) {}
    };

    atom::memory::SimpleObjectPool<TestStruct> pool;

    TestStruct* obj = pool.allocate(42, 3.14);
    EXPECT_NE(obj, nullptr);

    // Verify the object was initialized correctly
    EXPECT_EQ(obj->value, 42);
    EXPECT_DOUBLE_EQ(obj->data, 3.14);

    // Cleanup
    pool.deallocate(obj);
}

// Test multiple allocations
TEST_F(SimpleMemoryTest, MultipleAllocations) {
    atom::memory::MemoryPool<64> pool;
    std::vector<void*> ptrs;

    // Allocate multiple blocks
    for (int i = 0; i < 10; ++i) {
        void* ptr = pool.allocate();
        EXPECT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    // Deallocate all blocks
    for (size_t i = 0; i < ptrs.size(); ++i) {
        pool.deallocate(ptrs[i]);
    }

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.first, 0);  // allocated blocks should be 0
}

// Test exception handling without complex setup
TEST_F(SimpleMemoryTest, BasicExceptionHandling) {
    atom::memory::MemoryPool<64> pool;

    // Pool should handle null pointer deallocations gracefully
    EXPECT_NO_THROW(pool.deallocate(nullptr));

    // Pool should still be functional
    void* ptr = pool.allocate();
    EXPECT_NE(ptr, nullptr);
    pool.deallocate(ptr);
}

// Test basic object pool functionality
TEST_F(SimpleMemoryTest, BasicObjectPool) {
    struct SimpleObject {
        int id;
        SimpleObject(int i = 0) : id(i) {}
    };

    SimpleObjectPool<SimpleObject> objPool;  // Object pool

    // Get object from pool
    auto obj = objPool.allocate();
    EXPECT_NE(obj, nullptr);

    // Use the object
    obj->id = 123;
    EXPECT_EQ(obj->id, 123);

    // Return object to pool
    objPool.deallocate(obj);

    // Test that the pool can allocate again
    auto obj2 = objPool.allocate();
    EXPECT_NE(obj2, nullptr);
    objPool.deallocate(obj2);
}

// Test thread safety basics (without complex threading)
TEST_F(SimpleMemoryTest, BasicThreadSafety) {
    atom::memory::MemoryPool<64> pool;

    // Simple test that allocation/deallocation works
    // This doesn't test actual thread safety but ensures basic functionality
    std::vector<void*> ptrs;

    for (int i = 0; i < 5; ++i) {
        void* ptr = pool.allocate();
        EXPECT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    for (void* ptr : ptrs) {
        pool.deallocate(ptr);
    }

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.first, 0);  // allocated blocks should be 0
}

// Test memory alignment
TEST_F(SimpleMemoryTest, MemoryAlignment) {
    atom::memory::MemoryPool<64> pool;  // 64-byte blocks should be well-aligned

    void* ptr = pool.allocate();
    EXPECT_NE(ptr, nullptr);

    // Check alignment (should be aligned to max_align_t)
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignof(std::max_align_t), 0);

    pool.deallocate(ptr);
}

// Test pool statistics
TEST_F(SimpleMemoryTest, PoolStatistics) {
    atom::memory::MemoryPool<64> pool;

    auto initial_stats = pool.get_stats();
    EXPECT_EQ(initial_stats.first, 0);  // no allocated blocks initially
    EXPECT_EQ(initial_stats.second, 0); // no total blocks initially

    void* ptr1 = pool.allocate();
    auto stats_after_alloc = pool.get_stats();
    EXPECT_EQ(stats_after_alloc.first, 1);  // 1 allocated block
    EXPECT_GT(stats_after_alloc.second, 0); // some total blocks

    void* ptr2 = pool.allocate();
    auto stats_after_two = pool.get_stats();
    EXPECT_EQ(stats_after_two.first, 2);  // 2 allocated blocks

    pool.deallocate(ptr1);
    pool.deallocate(ptr2);

    auto final_stats = pool.get_stats();
    EXPECT_EQ(final_stats.first, 0);  // no allocated blocks after deallocation
}

#include <gtest/gtest.h>
#include "atom/memory/memory.hpp"

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
    MemoryPool<int> pool;

    // Test basic allocation
    int* ptr = pool.allocate(10);
    EXPECT_NE(ptr, nullptr);

    // Test deallocation
    pool.deallocate(ptr, 10);

    // Test statistics
    EXPECT_EQ(pool.getTotalAllocated(), 0);
}

// Test memory pool with custom types
TEST_F(SimpleMemoryTest, CustomTypeMemoryPool) {
    struct TestStruct {
        int value;
        double data;
        TestStruct(int v = 0, double d = 0.0) : value(v), data(d) {}
    };

    MemoryPool<TestStruct> pool;

    TestStruct* obj = pool.allocate(1);
    EXPECT_NE(obj, nullptr);

    // Initialize the object
    new (obj) TestStruct(42, 3.14);
    EXPECT_EQ(obj->value, 42);
    EXPECT_DOUBLE_EQ(obj->data, 3.14);

    // Cleanup
    obj->~TestStruct();
    pool.deallocate(obj, 1);
}

// Test multiple allocations
TEST_F(SimpleMemoryTest, MultipleAllocations) {
    MemoryPool<int> pool;
    std::vector<int*> ptrs;

    // Allocate multiple blocks
    for (int i = 0; i < 10; ++i) {
        int* ptr = pool.allocate(5);
        EXPECT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    // Deallocate all blocks
    for (size_t i = 0; i < ptrs.size(); ++i) {
        pool.deallocate(ptrs[i], 5);
    }

    EXPECT_EQ(pool.getTotalAllocated(), 0);
}

// Test exception handling without complex setup
TEST_F(SimpleMemoryTest, BasicExceptionHandling) {
    MemoryPool<int> pool;

    // Test that very large allocations throw exceptions
    EXPECT_THROW({
        int* ptr = pool.allocate(1000000);  // Very large allocation
        (void)ptr;  // Avoid unused variable warning
    }, std::exception);

    // Pool should still be functional after exception
    int* ptr = pool.allocate(10);
    EXPECT_NE(ptr, nullptr);
    pool.deallocate(ptr, 10);
}

// Test basic object pool functionality
TEST_F(SimpleMemoryTest, BasicObjectPool) {
    struct SimpleObject {
        int id;
        SimpleObject(int i = 0) : id(i) {}
    };

    ObjectPool<SimpleObject> objPool(5);  // Pool of 5 objects

    // Get object from pool
    auto obj = objPool.acquire();
    EXPECT_NE(obj, nullptr);

    // Use the object
    obj->id = 123;
    EXPECT_EQ(obj->id, 123);

    // Return object to pool
    objPool.release(std::move(obj));

    // Pool should have objects available
    EXPECT_GT(objPool.available(), 0);
}

// Test thread safety basics (without complex threading)
TEST_F(SimpleMemoryTest, BasicThreadSafety) {
    MemoryPool<int> pool;

    // Simple test that allocation/deallocation works
    // This doesn't test actual thread safety but ensures basic functionality
    std::vector<int*> ptrs;

    for (int i = 0; i < 5; ++i) {
        int* ptr = pool.allocate(1);
        EXPECT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    for (int* ptr : ptrs) {
        pool.deallocate(ptr, 1);
    }

    EXPECT_EQ(pool.getTotalAllocated(), 0);
}

// Test memory alignment
TEST_F(SimpleMemoryTest, MemoryAlignment) {
    MemoryPool<double> pool;  // double requires 8-byte alignment

    double* ptr = pool.allocate(1);
    EXPECT_NE(ptr, nullptr);

    // Check alignment (should be aligned to sizeof(double))
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % alignof(double), 0);

    pool.deallocate(ptr, 1);
}

// Test pool statistics
TEST_F(SimpleMemoryTest, PoolStatistics) {
    MemoryPool<int> pool;

    EXPECT_EQ(pool.getTotalAllocated(), 0);
    EXPECT_EQ(pool.getTotalAvailable(), 0);

    int* ptr1 = pool.allocate(10);
    EXPECT_GT(pool.getTotalAllocated(), 0);

    int* ptr2 = pool.allocate(5);
    size_t allocated_after_two = pool.getTotalAllocated();
    EXPECT_GT(allocated_after_two, 0);

    pool.deallocate(ptr1, 10);
    pool.deallocate(ptr2, 5);

    EXPECT_EQ(pool.getTotalAllocated(), 0);
}

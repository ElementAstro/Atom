#include <gtest/gtest.h>
#include <stdexcept>
#include <thread>
#include <vector>
#include "atom/memory/memory_pool.hpp"

using namespace atom::memory;

// Test object for SimpleObjectPool tests
class TestObject {
public:
    TestObject() : value_(0), constructed_(true) {}
    explicit TestObject(int value) : value_(value), constructed_(true) {}
    TestObject(int value, const std::string& name)
        : value_(value), name_(name), constructed_(true) {}

    ~TestObject() { constructed_ = false; }

    int getValue() const { return value_; }
    const std::string& getName() const { return name_; }
    bool isConstructed() const { return constructed_; }

    void setValue(int value) { value_ = value; }
    void setName(const std::string& name) { name_ = name; }

private:
    int value_;
    std::string name_;
    bool constructed_;
};

class MemoryPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// FixedBlockPool Tests
TEST_F(MemoryPoolTest, DefaultConstructor) {
    FixedBlockPool<64, 1024> pool;
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.first, 0);   // allocated_blocks
    EXPECT_EQ(stats.second, 0);  // total_blocks (no chunks allocated yet)
    EXPECT_TRUE(pool.is_empty());
}

TEST_F(MemoryPoolTest, BasicAllocation) {
    FixedBlockPool<64, 1024> pool;

    void* ptr = pool.allocate();
    EXPECT_NE(ptr, nullptr);

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.first, 1);      // allocated_blocks
    EXPECT_EQ(stats.second, 1024);  // total_blocks (one chunk allocated)
    EXPECT_FALSE(pool.is_empty());

    pool.deallocate(ptr);
    stats = pool.get_stats();
    EXPECT_EQ(stats.first, 0);      // allocated_blocks
    EXPECT_EQ(stats.second, 1024);  // total_blocks (chunk still exists)
    EXPECT_TRUE(pool.is_empty());
}

TEST_F(MemoryPoolTest, MultipleAllocations) {
    FixedBlockPool<64, 1024> pool;
    std::vector<void*> ptrs;

    // Allocate multiple blocks
    for (int i = 0; i < 10; ++i) {
        void* ptr = pool.allocate();
        EXPECT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.first, 10);     // allocated_blocks
    EXPECT_EQ(stats.second, 1024);  // total_blocks
    EXPECT_FALSE(pool.is_empty());

    // Deallocate all blocks
    for (void* ptr : ptrs) {
        pool.deallocate(ptr);
    }

    stats = pool.get_stats();
    EXPECT_EQ(stats.first, 0);  // allocated_blocks
    EXPECT_TRUE(pool.is_empty());
}

TEST_F(MemoryPoolTest, AllocationExceedsChunk) {
    FixedBlockPool<64, 10> pool;  // Small chunk size
    std::vector<void*> ptrs;

    // Allocate more than one chunk
    for (int i = 0; i < 15; ++i) {
        void* ptr = pool.allocate();
        EXPECT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.first, 15);   // allocated_blocks
    EXPECT_EQ(stats.second, 20);  // total_blocks (2 chunks)

    // Cleanup
    for (void* ptr : ptrs) {
        pool.deallocate(ptr);
    }
}

TEST_F(MemoryPoolTest, DeallocateNullptr) {
    FixedBlockPool<64, 1024> pool;

    // Should not crash or affect statistics
    pool.deallocate(nullptr);

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.first, 0);
    EXPECT_EQ(stats.second, 0);
    EXPECT_TRUE(pool.is_empty());
}

TEST_F(MemoryPoolTest, MemoryPoolReset) {
    FixedBlockPool<64, 1024> pool;
    std::vector<void*> ptrs;

    // Allocate some blocks
    for (int i = 0; i < 5; ++i) {
        ptrs.push_back(pool.allocate());
    }

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.first, 5);
    EXPECT_FALSE(pool.is_empty());

    // Reset the pool
    pool.reset();

    stats = pool.get_stats();
    EXPECT_EQ(stats.first, 0);      // allocated_blocks reset
    EXPECT_EQ(stats.second, 1024);  // total_blocks unchanged
    EXPECT_TRUE(pool.is_empty());

    // Should be able to allocate again
    void* ptr = pool.allocate();
    EXPECT_NE(ptr, nullptr);
    pool.deallocate(ptr);
}

TEST_F(MemoryPoolTest, MemoryPoolThreadSafety) {
    FixedBlockPool<64, 1024> pool;
    std::vector<std::thread> threads;
    std::vector<std::vector<void*>> thread_ptrs(4);

    // Create multiple threads that allocate and deallocate
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&pool, &thread_ptrs, i]() {
            for (int j = 0; j < 100; ++j) {
                void* ptr = pool.allocate();
                thread_ptrs[i].push_back(ptr);
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.first, 400);  // 4 threads * 100 allocations

    // Deallocate all pointers
    for (auto& ptrs : thread_ptrs) {
        for (void* ptr : ptrs) {
            pool.deallocate(ptr);
        }
    }

    EXPECT_TRUE(pool.is_empty());
}

// SimpleObjectPool Tests
TEST_F(MemoryPoolTest, SimpleObjectPoolDefaultConstructor) {
    SimpleObjectPool<TestObject> pool;
    auto stats = pool.get_stats();
    EXPECT_EQ(stats.first, 0);
    EXPECT_TRUE(pool.is_empty());
}

TEST_F(MemoryPoolTest, SimpleObjectPoolBasicAllocation) {
    SimpleObjectPool<TestObject> pool;

    TestObject* obj = pool.allocate();
    EXPECT_NE(obj, nullptr);
    EXPECT_TRUE(obj->isConstructed());
    EXPECT_EQ(obj->getValue(), 0);

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.first, 1);
    EXPECT_FALSE(pool.is_empty());

    pool.deallocate(obj);
    EXPECT_TRUE(pool.is_empty());
}

TEST_F(MemoryPoolTest, SimpleObjectPoolConstructorArgs) {
    SimpleObjectPool<TestObject> pool;

    TestObject* obj1 = pool.allocate(42);
    EXPECT_NE(obj1, nullptr);
    EXPECT_EQ(obj1->getValue(), 42);

    TestObject* obj2 = pool.allocate(100, "test");
    EXPECT_NE(obj2, nullptr);
    EXPECT_EQ(obj2->getValue(), 100);
    EXPECT_EQ(obj2->getName(), "test");

    pool.deallocate(obj1);
    pool.deallocate(obj2);
}

TEST_F(MemoryPoolTest, SimpleObjectPoolDeallocateNullptr) {
    SimpleObjectPool<TestObject> pool;

    // Should not crash
    pool.deallocate(nullptr);
    EXPECT_TRUE(pool.is_empty());
}

TEST_F(MemoryPoolTest, SimpleObjectPoolReset) {
    SimpleObjectPool<TestObject> pool;

    [[maybe_unused]] TestObject* obj = pool.allocate(42);
    EXPECT_FALSE(pool.is_empty());

    pool.reset();
    EXPECT_TRUE(pool.is_empty());

    // obj pointer is now invalid, but we can allocate new objects
    TestObject* new_obj = pool.allocate(100);
    EXPECT_NE(new_obj, nullptr);
    EXPECT_EQ(new_obj->getValue(), 100);

    pool.deallocate(new_obj);
}

TEST_F(MemoryPoolTest, SimpleObjectPoolExceptionSafety) {
    // Test exception safety during construction
    class ThrowingObject {
    public:
        ThrowingObject(bool should_throw) {
            if (should_throw) {
                throw std::runtime_error("Construction failed");
            }
        }
    };

    SimpleObjectPool<ThrowingObject> pool;

    // This should not throw
    ThrowingObject* obj1 = pool.allocate(false);
    EXPECT_NE(obj1, nullptr);

    // This should throw and not leak memory
    EXPECT_THROW([[maybe_unused]] auto* temp = pool.allocate(true),
                 std::runtime_error);

    // Pool should still be functional
    ThrowingObject* obj2 = pool.allocate(false);
    EXPECT_NE(obj2, nullptr);

    pool.deallocate(obj1);
    pool.deallocate(obj2);
}

TEST_F(MemoryPoolTest, SimpleObjectPoolThreadSafety) {
    SimpleObjectPool<TestObject> pool;
    std::vector<std::thread> threads;
    std::vector<std::vector<TestObject*>> thread_objs(4);

    // Create multiple threads that allocate and deallocate
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&pool, &thread_objs, i]() {
            for (int j = 0; j < 50; ++j) {
                TestObject* obj = pool.allocate(i * 100 + j);
                thread_objs[i].push_back(obj);
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.first, 200);  // 4 threads * 50 allocations

    // Verify objects were constructed correctly
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 50; ++j) {
            EXPECT_EQ(thread_objs[i][j]->getValue(), i * 100 + j);
        }
    }

    // Deallocate all objects
    for (auto& objs : thread_objs) {
        for (TestObject* obj : objs) {
            pool.deallocate(obj);
        }
    }

    EXPECT_TRUE(pool.is_empty());
}

// PoolPtr Tests
TEST_F(MemoryPoolTest, PoolPtrDefaultConstructor) {
    PoolPtr<TestObject> ptr;
    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.get(), nullptr);
}

TEST_F(MemoryPoolTest, PoolPtrBasicUsage) {
    SimpleObjectPool<TestObject> pool;
    TestObject* obj = pool.allocate(42);

    PoolPtr<TestObject> ptr(obj, &pool);
    EXPECT_TRUE(ptr);
    EXPECT_EQ(ptr.get(), obj);
    EXPECT_EQ(ptr->getValue(), 42);
    EXPECT_EQ((*ptr).getValue(), 42);

    // PoolPtr should automatically deallocate when destroyed
}

TEST_F(MemoryPoolTest, PoolPtrMoveSemantics) {
    SimpleObjectPool<TestObject> pool;
    TestObject* obj = pool.allocate(42);

    PoolPtr<TestObject> ptr1(obj, &pool);
    EXPECT_TRUE(ptr1);

    PoolPtr<TestObject> ptr2 = std::move(ptr1);
    EXPECT_FALSE(ptr1);
    EXPECT_TRUE(ptr2);
    EXPECT_EQ(ptr2->getValue(), 42);

    PoolPtr<TestObject> ptr3;
    ptr3 = std::move(ptr2);
    EXPECT_FALSE(ptr2);
    EXPECT_TRUE(ptr3);
    EXPECT_EQ(ptr3->getValue(), 42);
}

TEST_F(MemoryPoolTest, PoolPtrReset) {
    SimpleObjectPool<TestObject> pool;
    TestObject* obj1 = pool.allocate(42);
    TestObject* obj2 = pool.allocate(100);

    PoolPtr<TestObject> ptr(obj1, &pool);
    EXPECT_EQ(ptr->getValue(), 42);

    ptr.reset(obj2, &pool);
    EXPECT_EQ(ptr->getValue(), 100);

    ptr.reset();
    EXPECT_FALSE(ptr);
}

TEST_F(MemoryPoolTest, PoolPtrRelease) {
    SimpleObjectPool<TestObject> pool;
    TestObject* obj = pool.allocate(42);

    PoolPtr<TestObject> ptr(obj, &pool);
    EXPECT_TRUE(ptr);

    TestObject* released = ptr.release();
    EXPECT_FALSE(ptr);
    EXPECT_EQ(released, obj);
    EXPECT_EQ(released->getValue(), 42);

    // Must manually deallocate since we released ownership
    pool.deallocate(released);
}

TEST_F(MemoryPoolTest, PoolPtrSwap) {
    SimpleObjectPool<TestObject> pool;
    TestObject* obj1 = pool.allocate(42);
    TestObject* obj2 = pool.allocate(100);

    PoolPtr<TestObject> ptr1(obj1, &pool);
    PoolPtr<TestObject> ptr2(obj2, &pool);

    EXPECT_EQ(ptr1->getValue(), 42);
    EXPECT_EQ(ptr2->getValue(), 100);

    ptr1.swap(ptr2);

    EXPECT_EQ(ptr1->getValue(), 100);
    EXPECT_EQ(ptr2->getValue(), 42);
}

TEST_F(MemoryPoolTest, MakePoolPtr) {
    SimpleObjectPool<TestObject> pool;

    auto ptr = make_pool_ptr(pool, 42, "test");
    EXPECT_TRUE(ptr);
    EXPECT_EQ(ptr->getValue(), 42);
    EXPECT_EQ(ptr->getName(), "test");
}

// Edge case and boundary tests
TEST_F(MemoryPoolTest, MemoryPoolDifferentBlockSizes) {
    // Test with minimum aligned block size
    constexpr size_t min_size =
        std::max(sizeof(void*), alignof(std::max_align_t));
    FixedBlockPool<min_size, 10> small_pool;
    void* ptr1 = small_pool.allocate();
    EXPECT_NE(ptr1, nullptr);
    small_pool.deallocate(ptr1);

    // Test with large block size
    FixedBlockPool<1024, 10> large_pool;
    void* ptr2 = large_pool.allocate();
    EXPECT_NE(ptr2, nullptr);
    large_pool.deallocate(ptr2);
}

TEST_F(MemoryPoolTest, MemoryPoolAlignment) {
    FixedBlockPool<128, 10> pool;

    // Allocate multiple blocks and check alignment
    for (int i = 0; i < 5; ++i) {
        void* ptr = pool.allocate();
        EXPECT_NE(ptr, nullptr);

        // Check alignment (should be aligned to std::max_align_t)
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        EXPECT_EQ(addr % alignof(std::max_align_t), 0);

        pool.deallocate(ptr);
    }
}

TEST_F(MemoryPoolTest, SimpleObjectPoolWithComplexTypes) {
    // Test with objects that have non-trivial constructors/destructors
    class ComplexObject {
    public:
        ComplexObject() : data_(std::make_unique<int>(42)) {}
        explicit ComplexObject(int value)
            : data_(std::make_unique<int>(value)) {}

        int getValue() const { return *data_; }

    private:
        std::unique_ptr<int> data_;
    };

    SimpleObjectPool<ComplexObject> pool;

    ComplexObject* obj1 = pool.allocate();
    EXPECT_EQ(obj1->getValue(), 42);

    ComplexObject* obj2 = pool.allocate(100);
    EXPECT_EQ(obj2->getValue(), 100);

    pool.deallocate(obj1);
    pool.deallocate(obj2);
}

// Additional edge cases and boundary tests
TEST_F(MemoryPoolTest, PoolPtrSelfAssignment) {
    SimpleObjectPool<TestObject> pool;
    TestObject* obj = pool.allocate(42);

    PoolPtr<TestObject> ptr(obj, &pool);
    PoolPtr<TestObject> ptr_copy(obj, &pool);

    // Test that assignment works correctly
    ptr.reset();
    EXPECT_FALSE(ptr);
    EXPECT_TRUE(ptr_copy);

    // Clean up
    pool.deallocate(obj);
}

TEST_F(MemoryPoolTest, PoolPtrNullOperations) {
    PoolPtr<TestObject> ptr;

    // Operations on null PoolPtr should be safe
    ptr.reset();
    EXPECT_FALSE(ptr);

    TestObject* released = ptr.release();
    EXPECT_EQ(released, nullptr);

    PoolPtr<TestObject> ptr2;
    ptr.swap(ptr2);
    EXPECT_FALSE(ptr);
    EXPECT_FALSE(ptr2);
}

TEST_F(MemoryPoolTest, MemoryPoolStressTest) {
    FixedBlockPool<128, 100> pool;
    std::vector<void*> ptrs;

    // Allocate many blocks
    for (int i = 0; i < 500; ++i) {
        void* ptr = pool.allocate();
        EXPECT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.first, 500);
    EXPECT_GE(stats.second, 500);  // Should have multiple chunks

    // Deallocate in reverse order
    for (auto it = ptrs.rbegin(); it != ptrs.rend(); ++it) {
        pool.deallocate(*it);
    }

    EXPECT_TRUE(pool.is_empty());
}

TEST_F(MemoryPoolTest, SimpleObjectPoolStressTest) {
    SimpleObjectPool<TestObject> pool;
    std::vector<TestObject*> objs;

    // Allocate many objects
    for (int i = 0; i < 200; ++i) {
        TestObject* obj = pool.allocate(i);
        EXPECT_NE(obj, nullptr);
        EXPECT_EQ(obj->getValue(), i);
        objs.push_back(obj);
    }

    auto stats = pool.get_stats();
    EXPECT_EQ(stats.first, 200);

    // Deallocate all objects
    for (TestObject* obj : objs) {
        pool.deallocate(obj);
    }

    EXPECT_TRUE(pool.is_empty());
}

TEST_F(MemoryPoolTest, MemoryPoolFragmentation) {
    FixedBlockPool<64, 20> pool;
    std::vector<void*> ptrs;

    // Allocate blocks
    for (int i = 0; i < 15; ++i) {
        ptrs.push_back(pool.allocate());
    }

    // Deallocate every other block to create fragmentation
    for (size_t i = 0; i < ptrs.size(); i += 2) {
        pool.deallocate(ptrs[i]);
        ptrs[i] = nullptr;
    }

    // Allocate new blocks (should reuse freed space)
    for (size_t i = 0; i < ptrs.size(); i += 2) {
        if (ptrs[i] == nullptr) {
            ptrs[i] = pool.allocate();
            EXPECT_NE(ptrs[i], nullptr);
        }
    }

    // Clean up
    for (void* ptr : ptrs) {
        if (ptr != nullptr) {
            pool.deallocate(ptr);
        }
    }
}

TEST_F(MemoryPoolTest, PoolPtrExceptionSafety) {
    // Test that PoolPtr properly handles exceptions during construction
    class ThrowingObject {
    public:
        ThrowingObject(bool should_throw) {
            if (should_throw) {
                throw std::runtime_error("Construction failed");
            }
            value_ = 42;
        }

        int getValue() const { return value_; }

    private:
        int value_ = 0;
    };

    SimpleObjectPool<ThrowingObject> pool;

    // This should work
    auto ptr1 = make_pool_ptr(pool, false);
    EXPECT_TRUE(ptr1);
    EXPECT_EQ(ptr1->getValue(), 42);

    // This should throw and not leak memory
    EXPECT_THROW([[maybe_unused]] auto temp = make_pool_ptr(pool, true),
                 std::runtime_error);

    // Pool should still be functional
    auto ptr2 = make_pool_ptr(pool, false);
    EXPECT_TRUE(ptr2);
    EXPECT_EQ(ptr2->getValue(), 42);
}

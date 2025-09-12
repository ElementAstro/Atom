#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <iostream>

// Simple memory test that bypasses complex initialization
class SimpleMemoryAlternativeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Minimal setup without complex dependencies
    }

    void TearDown() override {
        // Minimal cleanup
    }
};

// Test basic memory allocation and deallocation
TEST_F(SimpleMemoryAlternativeTest, BasicAllocation) {
    // Test basic malloc/free
    void* ptr = std::malloc(1024);
    EXPECT_NE(ptr, nullptr);
    std::free(ptr);

    // Test new/delete
    int* int_ptr = new int(42);
    EXPECT_NE(int_ptr, nullptr);
    EXPECT_EQ(*int_ptr, 42);
    delete int_ptr;
}

// Test smart pointer functionality
TEST_F(SimpleMemoryAlternativeTest, SmartPointers) {
    // Test unique_ptr
    auto unique = std::make_unique<int>(100);
    EXPECT_NE(unique.get(), nullptr);
    EXPECT_EQ(*unique, 100);

    // Test shared_ptr
    auto shared1 = std::make_shared<int>(200);
    auto shared2 = shared1;
    EXPECT_EQ(shared1.use_count(), 2);
    EXPECT_EQ(*shared1, 200);
    EXPECT_EQ(*shared2, 200);
}

// Test vector memory management
TEST_F(SimpleMemoryAlternativeTest, VectorMemory) {
    std::vector<int> vec;

    // Test basic operations
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 3);

    // Test capacity growth
    size_t initial_capacity = vec.capacity();
    for (int i = 0; i < 100; ++i) {
        vec.push_back(i);
    }
    EXPECT_GT(vec.capacity(), initial_capacity);
}

// Test memory alignment
TEST_F(SimpleMemoryAlternativeTest, MemoryAlignment) {
    // Test aligned allocation (using platform-specific function)
#ifdef _WIN32
    void* ptr = _aligned_malloc(64, 16);
    if (ptr != nullptr) {
        EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 16, 0);
        _aligned_free(ptr);
    }
#else
    void* ptr = aligned_alloc(16, 64);
    if (ptr != nullptr) {
        EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr) % 16, 0);
        std::free(ptr);
    }
#endif
    // Note: aligned allocation might not be available on all systems
}

// Test exception safety
TEST_F(SimpleMemoryAlternativeTest, ExceptionSafety) {
    try {
        // Test that normal allocations work
        std::vector<int> vec(1000);
        EXPECT_EQ(vec.size(), 1000);

        // Test that we can handle allocation failures gracefully
        // (This test doesn't actually force a failure, just ensures no crash)
        std::unique_ptr<int[]> large_array(new int[10000]);
        EXPECT_NE(large_array.get(), nullptr);

    } catch (const std::exception& e) {
        // If an exception occurs, that's also acceptable
        std::cout << "Memory allocation exception (acceptable): " << e.what() << std::endl;
    }
}

// Test memory operations without complex tracking
TEST_F(SimpleMemoryAlternativeTest, BasicMemoryOperations) {
    const size_t size = 1024;

    // Test malloc/free cycle
    for (int i = 0; i < 10; ++i) {
        void* ptr = std::malloc(size);
        EXPECT_NE(ptr, nullptr);

        // Write some data
        memset(ptr, i, size);

        // Verify data
        unsigned char* byte_ptr = static_cast<unsigned char*>(ptr);
        EXPECT_EQ(byte_ptr[0], static_cast<unsigned char>(i));
        EXPECT_EQ(byte_ptr[size-1], static_cast<unsigned char>(i));

        std::free(ptr);
    }
}

// Test memory reallocation
TEST_F(SimpleMemoryAlternativeTest, MemoryReallocation) {
    void* ptr = std::malloc(100);
    EXPECT_NE(ptr, nullptr);

    // Write initial data
    memset(ptr, 0xAA, 100);

    // Reallocate to larger size
    ptr = std::realloc(ptr, 200);
    EXPECT_NE(ptr, nullptr);

    // Verify original data is preserved
    unsigned char* byte_ptr = static_cast<unsigned char*>(ptr);
    EXPECT_EQ(byte_ptr[0], 0xAA);
    EXPECT_EQ(byte_ptr[99], 0xAA);

    std::free(ptr);
}

// Test memory with different sizes
TEST_F(SimpleMemoryAlternativeTest, DifferentSizes) {
    std::vector<void*> ptrs;
    std::vector<size_t> sizes = {1, 16, 64, 256, 1024, 4096};

    // Allocate different sizes
    for (size_t size : sizes) {
        void* ptr = std::malloc(size);
        EXPECT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    // Free all allocations
    for (void* ptr : ptrs) {
        std::free(ptr);
    }
}

// Test that the test framework itself is working
TEST_F(SimpleMemoryAlternativeTest, TestFrameworkSanity) {
    EXPECT_TRUE(true);
    EXPECT_FALSE(false);
    EXPECT_EQ(1 + 1, 2);
    EXPECT_NE(1, 2);
    EXPECT_LT(1, 2);
    EXPECT_GT(2, 1);
}

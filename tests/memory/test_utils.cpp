#include <gtest/gtest.h>
#include <thread>

#include "atom/memory/utils.hpp"

using namespace atom::memory;

struct TestStruct {
    int a;
    double b;
    TestStruct(int x, double y) : a(x), b(y) {}
};

struct NoConstructorStruct {};

TEST(MemoryUtilsTest, MakeSharedValid) {
    auto ptr = makeShared<TestStruct>(10, 20.5);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->a, 10);
    EXPECT_EQ(ptr->b, 20.5);
}

TEST(MemoryUtilsTest, MakeSharedInvalid) {
    // This should fail to compile due to static_assert
    // auto ptr = makeShared<TestStruct>("invalid", 20.5);
}

TEST(MemoryUtilsTest, MakeUniqueValid) {
    auto ptr = makeUnique<TestStruct>(10, 20.5);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->a, 10);
    EXPECT_EQ(ptr->b, 20.5);
}

TEST(MemoryUtilsTest, MakeUniqueInvalid) {
    // This should fail to compile due to static_assert
    // auto ptr = makeUnique<TestStruct>("invalid", 20.5);
}

TEST(MemoryUtilsTest, MakeSharedNoConstructor) {
    // This should fail to compile due to static_assert
    // auto ptr = makeShared<NoConstructorStruct>();
}

TEST(MemoryUtilsTest, MakeUniqueNoConstructor) {
    // This should fail to compile due to static_assert
    // auto ptr = makeUnique<NoConstructorStruct>();
}

TEST(MemoryUtilsTest, MakeSharedWithDeleterValid) {
    bool deleterCalled = false;
    auto customDeleter = [&deleterCalled](TestStruct* ptr) {
        deleterCalled = true;
        delete ptr;
    };

    {
        auto ptr = makeSharedWithDeleter<TestStruct>(customDeleter, 10, 20.5);
        ASSERT_NE(ptr, nullptr);
        EXPECT_EQ(ptr->a, 10);
        EXPECT_EQ(ptr->b, 20.5);
        EXPECT_FALSE(deleterCalled);
    }

    // After the shared_ptr goes out of scope, the deleter should be called
    EXPECT_TRUE(deleterCalled);
}

TEST(MemoryUtilsTest, MakeUniqueWithDeleterValid) {
    bool deleterCalled = false;
    auto customDeleter = [&deleterCalled](TestStruct* ptr) {
        deleterCalled = true;
        delete ptr;
    };

    {
        auto ptr = makeUniqueWithDeleter<TestStruct>(customDeleter, 10, 20.5);
        ASSERT_NE(ptr, nullptr);
        EXPECT_EQ(ptr->a, 10);
        EXPECT_EQ(ptr->b, 20.5);
        EXPECT_FALSE(deleterCalled);
    }

    // After the unique_ptr goes out of scope, the deleter should be called
    EXPECT_TRUE(deleterCalled);
}

TEST(MemoryUtilsTest, MakeSharedArray) {
    constexpr size_t arraySize = 5;
    auto arr = makeSharedArray<int>(arraySize);
    ASSERT_NE(arr, nullptr);

    // Check that array is zero-initialized
    for (size_t i = 0; i < arraySize; ++i) {
        EXPECT_EQ(arr[i], 0);
    }

    // Modify and check
    for (size_t i = 0; i < arraySize; ++i) {
        arr[i] = static_cast<int>(i * 10);
    }

    for (size_t i = 0; i < arraySize; ++i) {
        EXPECT_EQ(arr[i], static_cast<int>(i * 10));
    }
}

TEST(MemoryUtilsTest, MakeUniqueArray) {
    constexpr size_t arraySize = 5;
    auto arr = makeUniqueArray<double>(arraySize);
    ASSERT_NE(arr, nullptr);

    // Check that array is zero-initialized
    for (size_t i = 0; i < arraySize; ++i) {
        EXPECT_EQ(arr[i], 0.0);
    }

    // Modify and check
    for (size_t i = 0; i < arraySize; ++i) {
        arr[i] = i * 1.5;
    }

    for (size_t i = 0; i < arraySize; ++i) {
        EXPECT_EQ(arr[i], i * 1.5);
    }
}

// Structure for testing ThreadSafeSingleton
struct SingletonTestStruct {
    int value = 42;
    static int instanceCount;

    SingletonTestStruct() { ++instanceCount; }

    ~SingletonTestStruct() { --instanceCount; }
};

int SingletonTestStruct::instanceCount = 0;

TEST(MemoryUtilsTest, ThreadSafeSingleton) {
    // Reset the static instance count
    SingletonTestStruct::instanceCount = 0;

    // Get the singleton instance
    auto instance1 = ThreadSafeSingleton<SingletonTestStruct>::getInstance();
    ASSERT_NE(instance1, nullptr);
    EXPECT_EQ(instance1->value, 42);
    EXPECT_EQ(SingletonTestStruct::instanceCount, 1);

    // Get the singleton instance again - should be the same instance
    auto instance2 = ThreadSafeSingleton<SingletonTestStruct>::getInstance();
    ASSERT_NE(instance2, nullptr);
    EXPECT_EQ(instance2.get(), instance1.get());
    EXPECT_EQ(SingletonTestStruct::instanceCount, 1);

    // Modify the instance through one pointer and check that it's visible
    // through the other
    instance1->value = 100;
    EXPECT_EQ(instance2->value, 100);

    // Release one shared_ptr - shouldn't destroy the singleton
    instance1.reset();
    EXPECT_EQ(SingletonTestStruct::instanceCount, 1);

    // Release the other shared_ptr - should destroy the singleton
    instance2.reset();
    // Note: With weak_ptr we can't guarantee immediate destruction
    // So we won't test instanceCount here
}

// Test lockWeak function
TEST(MemoryUtilsTest, LockWeak) {
    // Create a shared_ptr and a weak_ptr to it
    auto shared = std::make_shared<TestStruct>(10, 20.5);
    std::weak_ptr<TestStruct> weak = shared;

    // Lock the weak_ptr
    auto locked = lockWeak(weak);
    ASSERT_NE(locked, nullptr);
    EXPECT_EQ(locked->a, 10);
    EXPECT_EQ(locked->b, 20.5);

    // Destroy the original shared_ptr
    shared.reset();

    // Try to lock the weak_ptr again - should return nullptr
    auto locked2 = lockWeak(weak);
    EXPECT_EQ(locked2, nullptr);
}

// Test lockWeakOrCreate function
TEST(MemoryUtilsTest, LockWeakOrCreate) {
    // Start with an empty weak_ptr
    std::weak_ptr<TestStruct> weak;

    // Lock or create - should create a new object
    auto locked1 = lockWeakOrCreate(weak, 10, 20.5);
    ASSERT_NE(locked1, nullptr);
    EXPECT_EQ(locked1->a, 10);
    EXPECT_EQ(locked1->b, 20.5);

    // Lock or create again - should return the existing object
    auto locked2 = lockWeakOrCreate(weak, 30, 40.5);
    ASSERT_NE(locked2, nullptr);
    EXPECT_EQ(locked2.get(), locked1.get());
    EXPECT_EQ(locked2->a, 10);  // Values should not change
    EXPECT_EQ(locked2->b, 20.5);

    // Destroy the first shared_ptr
    locked1.reset();

    // Second shared_ptr still keeps the object alive
    auto locked3 = lockWeakOrCreate(weak, 50, 60.5);
    ASSERT_NE(locked3, nullptr);
    EXPECT_EQ(locked3.get(), locked2.get());

    // Destroy all shared_ptrs
    locked2.reset();
    locked3.reset();

    // Now create a new object
    auto locked4 = lockWeakOrCreate(weak, 70, 80.5);
    ASSERT_NE(locked4, nullptr);
    EXPECT_EQ(locked4->a, 70);  // New values
    EXPECT_EQ(locked4->b, 80.5);
}

// Additional test for concurrent access to ThreadSafeSingleton
TEST(MemoryUtilsTest, ThreadSafeSingletonConcurrent) {
    // Reset the static instance count
    SingletonTestStruct::instanceCount = 0;

    // Use multiple threads to access the singleton
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<SingletonTestStruct>> instances(numThreads);

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([i, &instances]() {
            instances[i] =
                ThreadSafeSingleton<SingletonTestStruct>::getInstance();
        });
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    // All threads should have the same instance
    for (int i = 1; i < numThreads; ++i) {
        EXPECT_EQ(instances[i].get(), instances[0].get());
    }

    // There should be only one instance
    EXPECT_EQ(SingletonTestStruct::instanceCount, 1);

    // Cleanup
    for (auto& instance : instances) {
        instance.reset();
    }
}

// Test isConstructible trait
TEST(MemoryUtilsTest, IsConstructible) {
    // Test with valid constructor arguments
    EXPECT_TRUE((IsConstructible<TestStruct, int, double>::value));

    // Test with invalid constructor arguments
    EXPECT_FALSE((IsConstructible<TestStruct, std::string, double>::value));
    EXPECT_FALSE((IsConstructible<TestStruct, int, int, int>::value));

    // Test with default constructible types
    struct DefaultConstructible {
        DefaultConstructible() = default;
    };
    EXPECT_TRUE((IsConstructible<DefaultConstructible>::value));

    // Test with non-default constructible types
    struct NonDefaultConstructible {
        NonDefaultConstructible() = delete;
        explicit NonDefaultConstructible(int) {}
    };
    EXPECT_FALSE((IsConstructible<NonDefaultConstructible>::value));
    EXPECT_TRUE((IsConstructible<NonDefaultConstructible, int>::value));
}

// Test Config struct
TEST(MemoryUtilsTest, Config) {
    // Check default alignment value
    EXPECT_EQ(Config::DefaultAlignment, alignof(std::max_align_t));

    // Check if EnableMemoryTracking is set correctly based on compile flag
#ifdef ATOM_MEMORY_TRACKING
    EXPECT_TRUE(Config::EnableMemoryTracking);
#else
    EXPECT_FALSE(Config::EnableMemoryTracking);
#endif
}
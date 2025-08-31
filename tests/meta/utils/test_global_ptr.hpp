/*!
 * \file test_global_ptr.hpp
 * \brief Unit tests for GlobalSharedPtrManager
 * \author Max Qian <lightapt.com>
 * \date 2024-03-25
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_TEST_GLOBAL_PTR_HPP
#define ATOM_TEST_GLOBAL_PTR_HPP

#include <gtest/gtest.h>
#include "atom/meta/global_ptr.hpp"

#include <chrono>
#include <string>
#include <thread>

namespace atom::test {

// Test classes
class SimpleClass {
public:
    SimpleClass(int v = 0) : value(v) {}
    int value;
    int getValue() const { return value; }
    void setValue(int v) { value = v; }
};

class DerivedClass : public SimpleClass {
public:
    DerivedClass(int v = 0) : SimpleClass(v), extra(v * 2) {}
    int extra;
};

class CustomDeletionTracker {
public:
    inline static int deleteCount = 0;
    ~CustomDeletionTracker() = default;
};

// Custom deleter function
void customDeleter(CustomDeletionTracker* ptr) {
    CustomDeletionTracker::deleteCount++;
    delete ptr;
}

// Test fixture
class GlobalPtrTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear the manager before each test
        GlobalSharedPtrManager::getInstance().clearAll();
        CustomDeletionTracker::deleteCount = 0;
    }

    void TearDown() override {
        // Clear the manager after each test
        GlobalSharedPtrManager::getInstance().clearAll();
    }
};

// Test basic shared pointer storage and retrieval
TEST_F(GlobalPtrTest, BasicSharedPtrStorageAndRetrieval) {
    // Store a shared pointer
    auto ptr1 = std::make_shared<SimpleClass>(42);
    AddPtr("test1", ptr1);

    // Retrieve the pointer
    auto retrieved = GetPtr<SimpleClass>("test1");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved.value()->getValue(), 42);

    // Change value through retrieved pointer
    retrieved.value()->setValue(100);

    // Check that original pointer reflects the change
    EXPECT_EQ(ptr1->getValue(), 100);
}

// Test get or create shared pointer
TEST_F(GlobalPtrTest, GetOrCreateSharedPtr) {
    // Get or create a new pointer
    std::shared_ptr<SimpleClass> ptr1;
    GET_OR_CREATE_PTR(ptr1, SimpleClass, "test2", 50);
    EXPECT_EQ(ptr1->getValue(), 50);

    // Try to get or create with same key (should get existing)
    std::shared_ptr<SimpleClass> ptr2;
    GET_OR_CREATE_PTR(ptr2, SimpleClass, "test2", 999);  // Different value
    EXPECT_EQ(ptr2->getValue(), 50);  // Should still be 50 from first creation

    // Confirm they point to the same object
    EXPECT_EQ(ptr1.get(), ptr2.get());
}

// Test weak pointer functionality
TEST_F(GlobalPtrTest, WeakPtrFunctionality) {
    // Create and store a shared pointer
    auto ptr1 = std::make_shared<SimpleClass>(42);
    AddPtr("test3", ptr1);

    // Get a weak pointer from it
    auto weakPtr = GetWeakPtr<SimpleClass>("test3");
    EXPECT_FALSE(weakPtr.expired());

    // Lock the weak pointer and verify
    auto lockedPtr = weakPtr.lock();
    ASSERT_TRUE(lockedPtr);
    EXPECT_EQ(lockedPtr->getValue(), 42);

    // Reset original shared pointer and check weak pointer is expired
    ptr1.reset();

    // Remove from manager to simulate complete cleanup
    RemovePtr("test3");

    // Get the weak pointer again (should now be expired)
    auto weakPtr2 = GetWeakPtr<SimpleClass>("test3");
    EXPECT_TRUE(weakPtr2.expired());
}

// Test creating weak pointer directly
TEST_F(GlobalPtrTest, CreateWeakPtrDirectly) {
    // Create a shared pointer locally
    auto sharedPtr = std::make_shared<SimpleClass>(100);

    // Create a weak pointer in the manager
    std::weak_ptr<SimpleClass> weakPtr = sharedPtr;
    GlobalSharedPtrManager::getInstance().addWeakPtr("test4", weakPtr);

    // Get the weak pointer from manager
    auto retrievedWeakPtr =
        GlobalSharedPtrManager::getInstance().getWeakPtr<SimpleClass>("test4");
    EXPECT_FALSE(retrievedWeakPtr.expired());

    // Lock the weak pointer
    auto lockedPtr = retrievedWeakPtr.lock();
    ASSERT_TRUE(lockedPtr);
    EXPECT_EQ(lockedPtr->getValue(), 100);

    // Reset the original shared pointer to expire the weak pointers
    sharedPtr.reset();

    // Verify the weak pointer is now expired
    EXPECT_TRUE(retrievedWeakPtr.expired());
}

// Test get shared pointer from weak pointer
TEST_F(GlobalPtrTest, GetSharedPtrFromWeakPtr) {
    // Create and store a shared pointer
    auto ptr1 = std::make_shared<SimpleClass>(42);

    // Store a weak pointer in the manager
    std::weak_ptr<SimpleClass> weakPtr = ptr1;
    GlobalSharedPtrManager::getInstance().addWeakPtr("test5", weakPtr);

    // Retrieve a shared pointer from the weak pointer
    auto retrievedPtr = GlobalSharedPtrManager::getInstance()
                            .getSharedPtrFromWeakPtr<SimpleClass>("test5");
    ASSERT_TRUE(retrievedPtr);
    EXPECT_EQ(retrievedPtr->getValue(), 42);

    // Reset original to test expiration
    ptr1.reset();

    // Try to get shared ptr from now-expired weak ptr
    auto nullPtr = GlobalSharedPtrManager::getInstance()
                       .getSharedPtrFromWeakPtr<SimpleClass>("test5");
    EXPECT_FALSE(nullPtr);
}

// Test removing pointers
TEST_F(GlobalPtrTest, RemovePointers) {
    // Add a few pointers
    AddPtr("ptr1", std::make_shared<SimpleClass>(1));
    AddPtr("ptr2", std::make_shared<SimpleClass>(2));
    AddPtr("ptr3", std::make_shared<SimpleClass>(3));

    // Check initial size
    EXPECT_EQ(GlobalSharedPtrManager::getInstance().size(), 3);

    // Remove middle pointer
    RemovePtr("ptr2");
    EXPECT_EQ(GlobalSharedPtrManager::getInstance().size(), 2);

    // Check ptr2 is gone
    auto ptr2 = GetPtr<SimpleClass>("ptr2");
    EXPECT_FALSE(ptr2.has_value());

    // Check other pointers still exist
    auto ptr1 = GetPtr<SimpleClass>("ptr1");
    EXPECT_TRUE(ptr1.has_value());
    auto ptr3 = GetPtr<SimpleClass>("ptr3");
    EXPECT_TRUE(ptr3.has_value());

    // Clear all pointers
    GlobalSharedPtrManager::getInstance().clearAll();
    EXPECT_EQ(GlobalSharedPtrManager::getInstance().size(), 0);
}

// Test custom deleter functionality
TEST_F(GlobalPtrTest, CustomDeleter) {
    // Create an object with custom deleter
    auto tracker = new CustomDeletionTracker();
    std::shared_ptr<CustomDeletionTracker> ptr1(tracker);

    // Add the pointer to the manager
    AddPtr("tracker", ptr1);

    // Add a custom deleter
    AddDeleter<CustomDeletionTracker>("tracker", customDeleter);

    // Get pointer info to verify custom deleter is registered
    auto info = GetPtrInfo("tracker");
    ASSERT_TRUE(info.has_value());
    EXPECT_TRUE(info->has_custom_deleter);

    // Remove the pointer to trigger deletion
    RemovePtr("tracker");

    // Check the custom deleter was called
    EXPECT_EQ(CustomDeletionTracker::deleteCount, 1);
}

// Test pointer metadata
TEST_F(GlobalPtrTest, PointerMetadata) {
    // Add a pointer
    AddPtr("meta_test", std::make_shared<SimpleClass>(42));

    // Get metadata
    auto info = GetPtrInfo("meta_test");
    ASSERT_TRUE(info.has_value());

    // Check type name contains "SimpleClass"
    EXPECT_TRUE(info->type_name.find("SimpleClass") != std::string::npos);

    // Check it's not a weak pointer
    EXPECT_FALSE(info->is_weak);

    // Check access count (should be at least 1 from our GetPtrInfo call)
    EXPECT_GE(info->access_count, 1);

    // Add a weak pointer and check its metadata
    std::weak_ptr<SimpleClass> weakPtr = std::make_shared<SimpleClass>(99);
    GlobalSharedPtrManager::getInstance().addWeakPtr("weak_meta", weakPtr);

    auto weakInfo = GetPtrInfo("weak_meta");
    ASSERT_TRUE(weakInfo.has_value());
    EXPECT_TRUE(weakInfo->is_weak);
}

// Test removing expired weak pointers
TEST_F(GlobalPtrTest, RemoveExpiredWeakPtrs) {
    // Create some pointers that will expire
    {
        auto ptr1 = std::make_shared<SimpleClass>(1);
        auto ptr2 = std::make_shared<SimpleClass>(2);

        // Add weak pointers to manager
        std::weak_ptr<SimpleClass> weak1 = ptr1;
        std::weak_ptr<SimpleClass> weak2 = ptr2;

        GlobalSharedPtrManager::getInstance().addWeakPtr("weak1", weak1);
        GlobalSharedPtrManager::getInstance().addWeakPtr("weak2", weak2);

        // ptr1 and ptr2 will go out of scope and expire the weak pointers
    }

    // Add a non-expiring pointer
    auto ptr3 = std::make_shared<SimpleClass>(3);
    std::weak_ptr<SimpleClass> weak3 = ptr3;
    GlobalSharedPtrManager::getInstance().addWeakPtr("weak3", weak3);

    // Initial size should be 3
    EXPECT_EQ(GlobalSharedPtrManager::getInstance().size(), 3);

    // Remove expired weak pointers
    size_t removed =
        GlobalSharedPtrManager::getInstance().removeExpiredWeakPtrs();
    EXPECT_EQ(removed, 2);  // weak1 and weak2 should be removed

    // Size should now be 1
    EXPECT_EQ(GlobalSharedPtrManager::getInstance().size(), 1);

    // Check weak3 still exists
    auto retrievedWeak3 =
        GlobalSharedPtrManager::getInstance().getWeakPtr<SimpleClass>("weak3");
    EXPECT_FALSE(retrievedWeak3.expired());
}

// Test cleaning old pointers
TEST_F(GlobalPtrTest, CleanOldPointers) {
    // Add some pointers
    AddPtr("old1", std::make_shared<SimpleClass>(1));
    AddPtr("old2", std::make_shared<SimpleClass>(2));

    // Wait a moment to create age difference
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Add a newer pointer
    AddPtr("new", std::make_shared<SimpleClass>(3));

    // Access old2 to update its access time
    auto old2 = GetPtr<SimpleClass>("old2");

    // Clean pointers older than 50ms and not accessed recently
    size_t removed = GlobalSharedPtrManager::getInstance().cleanOldPointers(
        std::chrono::seconds(0));

    // Should have removed old1, but not old2 (recently accessed) or new (too
    // new)
    EXPECT_EQ(removed, 1);

    // Check which pointers remain
    EXPECT_FALSE(GetPtr<SimpleClass>("old1").has_value());
    EXPECT_TRUE(GetPtr<SimpleClass>("old2").has_value());
    EXPECT_TRUE(GetPtr<SimpleClass>("new").has_value());
}

// Test thread safety
/*
TODO: Uncomment this test when thread safety is implemented
TEST_F(GlobalPtrTest, ThreadSafety) {
    const int numThreads = 10;
    const int numOperationsPerThread = 100;

    std::vector<std::thread> threads;
    std::atomic<int> successCount(0);

    // Launch threads to concurrently access the pointer manager
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([i, &successCount] {
            for (int j = 0; j < numOperationsPerThread; ++j) {
                try {
                    // Create a unique key for this thread iteration
                    std::string key =
                        "thread" + std::to_string(i) + "_" + std::to_string(j);

                    // Randomly choose between add/get/remove operations
                    int op = j % 3;

                    if (op == 0) {
                        // Add a pointer
                        AddPtr(key,
                               std::make_shared<SimpleClass>(i * 1000 + j));
                        successCount++;
                    } else if (op == 1) {
                        // Get or create a pointer
                        std::shared_ptr<SimpleClass> ptr;
                        int value =
                            i * 1000 +
                            j;  // Calculate the value before passing to macro
                        GET_OR_CREATE_PTR_WITH_CAPTURE(ptr, SimpleClass, key,
value); if (ptr) successCount++; } else {
                        // Remove a pointer (might not exist)
                        RemovePtr(key);
                        successCount++;
                    }
                } catch (...) {
                    // Ignore exceptions for this test
                }
            }
        });
    }

    // Join threads
    for (auto& thread : threads) {
        thread.join();
    }

    // If we got here without crashes or deadlocks, the test passes
    // The success count helps verify that operations completed
    EXPECT_GT(successCount, 0);

    // Clean up
    GlobalSharedPtrManager::getInstance().clearAll();
}
*/

// Test type safety
TEST_F(GlobalPtrTest, TypeSafety) {
    // Add a SimpleClass instance
    auto simplePtr = std::make_shared<SimpleClass>(42);
    AddPtr("type_test", simplePtr);

    // Try to retrieve as wrong type (should return nullopt)
    auto wrongTypePtr = GetPtr<DerivedClass>("type_test");
    EXPECT_FALSE(wrongTypePtr.has_value());

    // Retrieve with correct type
    auto correctTypePtr = GetPtr<SimpleClass>("type_test");
    EXPECT_TRUE(correctTypePtr.has_value());

    // Add a derived class
    auto derivedPtr = std::make_shared<DerivedClass>(100);
    AddPtr("derived", derivedPtr);

    // Can retrieve with base class type
    auto retrievedAsBase = GetPtr<SimpleClass>("derived");
    EXPECT_TRUE(retrievedAsBase.has_value());
    EXPECT_EQ(retrievedAsBase.value()->getValue(), 100);

    // But not vice versa
    auto retrievedAsDerived = GetPtr<DerivedClass>("type_test");
    EXPECT_FALSE(retrievedAsDerived.has_value());
}

// Test weak pointer creation from GET_OR_CREATE_WEAK_PTR macro
TEST_F(GlobalPtrTest, WeakPtrCreationMacro) {
    // Create a weak pointer
    std::weak_ptr<SimpleClass> weakPtr;
    GET_OR_CREATE_WEAK_PTR(weakPtr, SimpleClass, "weak_macro_test", 123);

    // Lock the pointer and verify it works
    auto lockedPtr = weakPtr.lock();
    ASSERT_TRUE(lockedPtr);
    EXPECT_EQ(lockedPtr->getValue(), 123);

    // Try to retrieve the same pointer with another weak pointer
    std::weak_ptr<SimpleClass> anotherWeakPtr;
    GET_OR_CREATE_WEAK_PTR(anotherWeakPtr, SimpleClass, "weak_macro_test", 456);

    // Lock and verify it's the same object (value should still be 123, not 456)
    auto anotherLocked = anotherWeakPtr.lock();
    ASSERT_TRUE(anotherLocked);
    EXPECT_EQ(anotherLocked->getValue(), 123);

    // Verify both point to the same object
    EXPECT_EQ(lockedPtr.get(), anotherLocked.get());
}

// Test get pointer info for nonexistent key
TEST_F(GlobalPtrTest, GetPtrInfoNonexistentKey) {
    // Try to get info for a key that doesn't exist
    auto info = GetPtrInfo("nonexistent");
    EXPECT_FALSE(info.has_value());
}

// Test GET_OR_CREATE_PTR_THIS macro
TEST_F(GlobalPtrTest, GetOrCreatePtrThisMacro) {
    // Define a test class with a method that uses GET_OR_CREATE_PTR_THIS
    class TestWithThis {
    public:
        TestWithThis(int val) : testValue(val) {}

        void createPtr() {
            std::shared_ptr<SimpleClass> ptr;
            GET_OR_CREATE_PTR_THIS(ptr, SimpleClass, "this_test", testValue);
            created = true;
        }

        int testValue;
        bool created = false;
    };

    // Test the macro
    TestWithThis test(42);
    test.createPtr();
    EXPECT_TRUE(test.created);

    // Verify the pointer was created with the correct value
    auto retrievedPtr = GetPtr<SimpleClass>("this_test");
    ASSERT_TRUE(retrievedPtr.has_value());
    EXPECT_EQ(retrievedPtr.value()->getValue(), 42);
}

// Test GET_OR_CREATE_PTR_WITH_DELETER macro
TEST_F(GlobalPtrTest, GetOrCreatePtrWithDeleterMacro) {
    // Create a pointer with custom deleter
    std::shared_ptr<CustomDeletionTracker> ptr;
    auto deleterFunc = customDeleter;  // Create a local variable to capture
    GET_OR_CREATE_PTR_WITH_DELETER(ptr, CustomDeletionTracker, "deleter_test",
                                   deleterFunc);

    // Verify the pointer exists
    ASSERT_TRUE(ptr);

    // Check the metadata
    auto info = GetPtrInfo("deleter_test");
    ASSERT_TRUE(info.has_value());
    EXPECT_TRUE(info->has_custom_deleter);

    // Clear the manager to trigger deletion
    GlobalSharedPtrManager::getInstance().clearAll();

    // Check that our custom deleter was called
    EXPECT_EQ(CustomDeletionTracker::deleteCount, 1);
}

// Test pointer reference count tracking
TEST_F(GlobalPtrTest, ReferenceCountTracking) {
    // Create a pointer with initial ref count of 1
    auto originalPtr = std::make_shared<SimpleClass>(42);
    AddPtr("ref_count_test", originalPtr);

    // Get initial metadata
    auto initialInfo = GetPtrInfo("ref_count_test");
    ASSERT_TRUE(initialInfo.has_value());
    size_t initialRefCount = initialInfo->ref_count;
    EXPECT_GE(initialRefCount, 2);  // Original + stored in manager

    // Create more references
    {
        auto ref1 = GetPtr<SimpleClass>("ref_count_test");
        auto ref2 = GetPtr<SimpleClass>("ref_count_test");

        // Check increased ref count
        auto updatedInfo = GetPtrInfo("ref_count_test");
        ASSERT_TRUE(updatedInfo.has_value());
        EXPECT_GT(updatedInfo->ref_count, initialRefCount);
    }

    // References go out of scope, check count decreases
    auto finalInfo = GetPtrInfo("ref_count_test");
    ASSERT_TRUE(finalInfo.has_value());
    EXPECT_EQ(finalInfo->ref_count, initialRefCount);
}

// Test GET_WEAK_PTR macro (can't test directly due to THROW_OBJ_NOT_EXIST)
// We need to exclude this test in environments where throwing is problematic
#ifndef NO_EXCEPTION_TESTS
class ComponentException : public std::exception {
    std::string message;

public:
    ComponentException(const std::string& msg) : message(msg) {}
    const char* what() const noexcept override { return message.c_str(); }
};

// Mock implementation of THROW_OBJ_NOT_EXIST macro for testing
#define TEST_THROW_OBJ_NOT_EXIST(msg, id) \
    throw ComponentException(std::string(msg) + id)

TEST_F(GlobalPtrTest, GetWeakPtrMacroSimulated) {
    static constexpr const char* id = "test_component";

    // First test with a non-existent pointer (should throw)
    std::weak_ptr<SimpleClass> nonExistentPtr;
    bool thrown = false;

    try {
        // Manually simulate GET_WEAK_PTR behavior
        nonExistentPtr = GetWeakPtr<SimpleClass>(id);
        auto ptr = nonExistentPtr.lock();
        if (!ptr) {
            TEST_THROW_OBJ_NOT_EXIST("Component: ", id);
        }
    } catch (const ComponentException&) {
        thrown = true;
    }

    EXPECT_TRUE(thrown);

    // Now create a pointer and test again
    AddPtr(id, std::make_shared<SimpleClass>(42));

    std::weak_ptr<SimpleClass> existingPtr;
    thrown = false;

    try {
        // Manually simulate GET_WEAK_PTR behavior
        existingPtr = GetWeakPtr<SimpleClass>(id);
        auto ptr = existingPtr.lock();
        if (!ptr) {
            TEST_THROW_OBJ_NOT_EXIST("Component: ", id);
        }
        // Should not throw now
        EXPECT_EQ(ptr->getValue(), 42);
    } catch (const ComponentException&) {
        thrown = true;
    }

    EXPECT_FALSE(thrown);
}
#endif

}  // namespace atom::test

#endif  // ATOM_TEST_GLOBAL_PTR_HPP

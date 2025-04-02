// filepath: /home/max/Atom-1/atom/type/test_no_offset_ptr.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "no_offset_ptr.hpp"

using namespace atom;
using ::testing::Eq;

class SimpleTestClass {
public:
    // 合并两个冲突的构造函数，解决歧义问题
    SimpleTestClass(int val = 0) : value(val) { instances++; }

    int getValue() const { return value; }
    void setValue(int val) { value = val; }

    // Track object lifecycle
    static int instances;

    SimpleTestClass(const SimpleTestClass& other) : value(other.value) {
        instances++;
    }
    
    // 修复未使用的参数警告
    SimpleTestClass(SimpleTestClass&& other) noexcept : value(other.value) {
        other.value = 0;
        instances++;
    }
    
    ~SimpleTestClass() { instances--; }

private:
    int value;
};

int SimpleTestClass::instances = 0;

class ThrowingClass {
public:
    // Constructor that throws
    explicit ThrowingClass(bool shouldThrow) {
        if (shouldThrow) {
            throw std::runtime_error("Test exception");
        }
    }

    // 修复未使用的参数警告
    ThrowingClass(ThrowingClass&& /*other*/) {
        throw std::runtime_error("Move failed");
    }
};

class NoOffsetPtrTest : public ::testing::Test {
protected:
    void SetUp() override { SimpleTestClass::instances = 0; }

    void TearDown() override {
        // Ensure all instances are cleaned up
        EXPECT_EQ(SimpleTestClass::instances, 0);
    }
};

// Test basic NoOffsetPtr functionality
TEST_F(NoOffsetPtrTest, DefaultConstruction) {
    UnshiftedPtr<SimpleTestClass> ptr{};
    EXPECT_TRUE(ptr.has_value());
    EXPECT_EQ(ptr->getValue(), 0);
    EXPECT_EQ(SimpleTestClass::instances, 1);
}

TEST_F(NoOffsetPtrTest, ValueConstruction) {
    UnshiftedPtr<SimpleTestClass> ptr{42};
    EXPECT_TRUE(ptr.has_value());
    EXPECT_EQ(ptr->getValue(), 42);
    EXPECT_EQ(SimpleTestClass::instances, 1);
}

TEST_F(NoOffsetPtrTest, AccessOperators) {
    UnshiftedPtr<SimpleTestClass> ptr{42};

    // Test operator->
    EXPECT_EQ(ptr->getValue(), 42);
    ptr->setValue(100);
    EXPECT_EQ(ptr->getValue(), 100);

    // Test operator*
    (*ptr).setValue(200);
    EXPECT_EQ((*ptr).getValue(), 200);
}

TEST_F(NoOffsetPtrTest, Reset) {
    UnshiftedPtr<SimpleTestClass> ptr{42};
    EXPECT_EQ(ptr->getValue(), 42);

    // Reset with new value
    ptr.reset(100);
    EXPECT_EQ(ptr->getValue(), 100);
    EXPECT_EQ(SimpleTestClass::instances, 1);
}

TEST_F(NoOffsetPtrTest, Emplace) {
    UnshiftedPtr<SimpleTestClass> ptr{};
    ptr.emplace(42);
    EXPECT_EQ(ptr->getValue(), 42);
    EXPECT_EQ(SimpleTestClass::instances, 1);

    // Emplace again
    ptr.emplace(100);
    EXPECT_EQ(ptr->getValue(), 100);
    EXPECT_EQ(SimpleTestClass::instances, 1);
}

TEST_F(NoOffsetPtrTest, BoolConversion) {
    UnshiftedPtr<SimpleTestClass> ptr{};
    EXPECT_TRUE(static_cast<bool>(ptr));

    // We can't easily test false case since we can't construct empty
    // NoOffsetPtr directly
}

TEST_F(NoOffsetPtrTest, GetSafe) {
    UnshiftedPtr<SimpleTestClass> ptr{42};

    SimpleTestClass* rawPtr = ptr.get_safe();
    EXPECT_NE(rawPtr, nullptr);
    EXPECT_EQ(rawPtr->getValue(), 42);

    const UnshiftedPtr<SimpleTestClass> constPtr{100};
    const SimpleTestClass* constRawPtr = constPtr.get_safe();
    EXPECT_NE(constRawPtr, nullptr);
    EXPECT_EQ(constRawPtr->getValue(), 100);
}

TEST_F(NoOffsetPtrTest, Release) {
    UnshiftedPtr<SimpleTestClass> ptr{42};

    // 修复 nodiscard 警告，保存返回值
    SimpleTestClass* rawPtr = ptr.release();
    EXPECT_NE(rawPtr, nullptr);
    EXPECT_EQ(rawPtr->getValue(), 42);

    // After release, the NoOffsetPtr doesn't own the object anymore
    EXPECT_FALSE(ptr.has_value());

    // But the object still exists
    EXPECT_EQ(SimpleTestClass::instances, 1);

    // Clean up manually
    rawPtr->~SimpleTestClass();
    SimpleTestClass::instances =
        0;  // Reset manually since we called destructor directly
}

TEST_F(NoOffsetPtrTest, MoveConstructor) {
    UnshiftedPtr<SimpleTestClass> ptr1{42};
    UnshiftedPtr<SimpleTestClass> ptr2(std::move(ptr1));

    // ptr2 should now have the value
    EXPECT_TRUE(ptr2.has_value());
    EXPECT_EQ(ptr2->getValue(), 42);

    // ptr1 should no longer have a value
    EXPECT_FALSE(ptr1.has_value());

    // Only one instance should exist
    EXPECT_EQ(SimpleTestClass::instances, 1);

    // Trying to access ptr1 should throw
    EXPECT_THROW(ptr1->getValue(), unshifted_ptr_error);
}

TEST_F(NoOffsetPtrTest, MoveAssignment) {
    UnshiftedPtr<SimpleTestClass> ptr1{42};
    UnshiftedPtr<SimpleTestClass> ptr2{100};

    ptr2 = std::move(ptr1);

    // ptr2 should now have ptr1's value
    EXPECT_TRUE(ptr2.has_value());
    EXPECT_EQ(ptr2->getValue(), 42);

    // ptr1 should no longer have a value
    EXPECT_FALSE(ptr1.has_value());

    // Only one instance should exist
    EXPECT_EQ(SimpleTestClass::instances, 1);
}

TEST_F(NoOffsetPtrTest, ApplyIf) {
    UnshiftedPtr<SimpleTestClass> ptr{42};

    // Apply function if has value
    ptr.apply_if([](SimpleTestClass& obj) { obj.setValue(100); });

    EXPECT_EQ(ptr->getValue(), 100);

    // Release ownership and try to apply
    SimpleTestClass* rawPtr = ptr.release();
    bool called = false;
    ptr.apply_if([&called](SimpleTestClass& /*obj*/) { called = true; });

    // Should not have been called
    EXPECT_FALSE(called);

    // Clean up manually
    rawPtr->~SimpleTestClass();
    SimpleTestClass::instances = 0;
}

// Test thread safety variations
TEST(NoOffsetPtrThreadSafetyTest, MutexSafety) {
    // Create a thread-safe pointer
    ThreadSafeUnshiftedPtr<SimpleTestClass> ptr{42};

    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int iterations = 100;

    std::atomic<int> successCount = 0;

    // Create multiple threads that read and write
    for (int i = 0; i < numThreads; i++) {
        // 修复Lambda捕获问题，移除未使用的变量
        threads.emplace_back([&ptr, &successCount]() {
            for (int j = 0; j < 100; j++) {
                try {
                    // Read
                    int val = ptr->getValue();

                    // Write
                    ptr->setValue(val + 1);

                    // Success if no exception
                    successCount++;
                } catch (const unshifted_ptr_error& e) {
                    // Might happen if another thread has released ownership
                }
            }
        });
    }

    // Join all threads
    for (auto& t : threads) {
        t.join();
    }

    // If all is well, there should have been successful operations
    EXPECT_GT(successCount, 0);

    // Only one instance should exist at the end
    EXPECT_EQ(SimpleTestClass::instances, 1);
}

TEST(NoOffsetPtrThreadSafetyTest, AtomicSafety) {
    // Create a lock-free pointer
    LockFreeUnshiftedPtr<SimpleTestClass> ptr{42};

    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int iterations = 100;

    std::atomic<int> successCount = 0;

    // Create multiple threads that read and write
    for (int i = 0; i < numThreads; i++) {
        // 修复Lambda捕获问题，移除未使用的变量
        threads.emplace_back([&ptr, &successCount]() {
            for (int j = 0; j < 100; j++) {
                try {
                    // Read
                    int val = ptr->getValue();

                    // Write
                    ptr->setValue(val + 1);

                    // Success if no exception
                    successCount++;
                } catch (const unshifted_ptr_error& e) {
                    // Might happen if another thread has released ownership
                }
            }
        });
    }

    // Join all threads
    for (auto& t : threads) {
        t.join();
    }

    // If all is well, there should have been successful operations
    EXPECT_GT(successCount, 0);

    // Only one instance should exist at the end
    EXPECT_EQ(SimpleTestClass::instances, 1);
}

// Test exception safety
TEST(NoOffsetPtrExceptionTest, ConstructorException) {
    // Test that if constructor throws, ownership is not set
    EXPECT_THROW(UnshiftedPtr<ThrowingClass>{true}, std::runtime_error);

    // Test with non-throwing constructor
    EXPECT_NO_THROW(UnshiftedPtr<ThrowingClass>{false});
}

TEST(NoOffsetPtrExceptionTest, MoveException) {
    // Since our ThrowingClass throws in the move constructor,
    // we need a custom test class that can be constructed but throws on move
    struct MoveThrowOnlySecond {
        bool moved = false;
        MoveThrowOnlySecond() = default;
        MoveThrowOnlySecond(MoveThrowOnlySecond&& other) {
            if (other.moved) {
                throw std::runtime_error("Move failed");
            }
            moved = true;
        }
    };

    // First move should succeed
    UnshiftedPtr<MoveThrowOnlySecond> ptr1{};
    EXPECT_NO_THROW({
        UnshiftedPtr<MoveThrowOnlySecond> ptr2(std::move(ptr1));
        // Mark as moved for the next test
        ptr2->moved = true;

        // Second move should throw
        EXPECT_THROW(
            { UnshiftedPtr<MoveThrowOnlySecond> ptr3(std::move(ptr2)); },
            std::runtime_error);
    });
}

TEST(NoOffsetPtrExceptionTest, ResetException) {
    UnshiftedPtr<ThrowingClass> ptr{false};

    // Reset with throwing constructor should maintain existing object
    EXPECT_THROW(ptr.reset(true), std::runtime_error);

    // Pointer should still be valid
    EXPECT_TRUE(ptr.has_value());
}

// Test with complex types
TEST(NoOffsetPtrComplexTypeTest, StdString) {
    UnshiftedPtr<std::string> ptr("Hello, World!");

    EXPECT_EQ(*ptr, "Hello, World!");
    EXPECT_EQ(ptr->size(), 13);

    ptr->append(" More text.");
    EXPECT_EQ(*ptr, "Hello, World! More text.");
}

/*
TODO: Fix std::vector test
TEST(NoOffsetPtrComplexTypeTest, StdVector) {
    UnshiftedPtr<std::vector<int>> ptr({1, 2, 3, 4, 5});

    EXPECT_EQ(ptr->size(), 5);
    EXPECT_EQ((*ptr)[2], 3);

    ptr->push_back(6);
    EXPECT_EQ(ptr->size(), 6);
    EXPECT_EQ(ptr->back(), 6);
}
*/


// Test with different thread safety policies
TEST(NoOffsetPtrPolicyTest, DefaultPolicy) {
    // Default is None
    UnshiftedPtr<SimpleTestClass> ptr;
    EXPECT_TRUE(ptr.has_value());
}

TEST(NoOffsetPtrPolicyTest, MutexPolicy) {
    UnshiftedPtr<SimpleTestClass, ThreadSafetyPolicy::Mutex> ptr;
    EXPECT_TRUE(ptr.has_value());
}

TEST(NoOffsetPtrPolicyTest, AtomicPolicy) {
    UnshiftedPtr<SimpleTestClass, ThreadSafetyPolicy::Atomic> ptr;
    EXPECT_TRUE(ptr.has_value());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
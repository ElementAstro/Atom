// filepath: /home/max/Atom-1/atom/type/test_pointer.cpp

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "pointer.hpp"

using ::testing::ThrowsMessage;

class PointerSentinelTest : public ::testing::Test {
protected:
    // Test class with methods to verify pointer invocation
    class TestClass {
    public:
        TestClass() : value_(0) {}
        explicit TestClass(int value) : value_(value) {}
        TestClass(const TestClass& other) : value_(other.value_) {}

        int getValue() const { return value_; }
        void setValue(int value) { value_ = value; }
        int add(int a, int b) const { return a + b + value_; }
        std::string toString() const { return std::to_string(value_); }

    private:
        int value_;
    };

    // Helper functions for SIMD operations
    static void testSimdOperation(TestClass* ptr, size_t size) {
        // Simulate a SIMD operation by setting values
        for (size_t i = 0; i < size; ++i) {
            ptr[i].setValue(static_cast<int>(i));
        }
    }

    // Helper function to validate type concept
    template <typename T>
    bool isPointerType() {
        return PointerType<T>;
    }

    void SetUp() override {
        // Create some test objects for reuse
        rawPtr_ = new TestClass(100);
        sharedPtr_ = std::make_shared<TestClass>(200);
        uniquePtr_ = std::make_unique<TestClass>(300);
        weakPtr_ = std::weak_ptr<TestClass>(sharedPtr_);
    }

    void TearDown() override { delete rawPtr_; }

    TestClass* rawPtr_;
    std::shared_ptr<TestClass> sharedPtr_;
    std::unique_ptr<TestClass> uniquePtr_;
    std::weak_ptr<TestClass> weakPtr_;
};

// Test PointerType concept
TEST_F(PointerSentinelTest, PointerTypeConcept) {
    EXPECT_TRUE(isPointerType<TestClass*>());
    EXPECT_TRUE(isPointerType<std::shared_ptr<TestClass>>());
    EXPECT_TRUE(isPointerType<std::unique_ptr<TestClass>>());
    EXPECT_TRUE(isPointerType<std::weak_ptr<TestClass>>());

    // Non-pointer types should not satisfy the concept
    EXPECT_FALSE(isPointerType<TestClass>());
    EXPECT_FALSE(isPointerType<int>());
    EXPECT_FALSE(isPointerType<std::string>());
}

// Test constructors
TEST_F(PointerSentinelTest, Constructor) {
    // Default constructor
    PointerSentinel<TestClass> defaultSentinel;
    EXPECT_FALSE(defaultSentinel.is_valid());

    // Raw pointer constructor
    PointerSentinel<TestClass> rawSentinel(rawPtr_);
    EXPECT_TRUE(rawSentinel.is_valid());
    EXPECT_EQ(rawSentinel.get(), rawPtr_);

    // Shared pointer constructor
    PointerSentinel<TestClass> sharedSentinel(sharedPtr_);
    EXPECT_TRUE(sharedSentinel.is_valid());
    EXPECT_EQ(sharedSentinel.get(), sharedPtr_.get());

    // Unique pointer constructor
    auto uniquePtr = std::make_unique<TestClass>(400);
    PointerSentinel<TestClass> uniqueSentinel(std::move(uniquePtr));
    EXPECT_TRUE(uniqueSentinel.is_valid());
    EXPECT_NE(uniqueSentinel.get(), nullptr);

    // Weak pointer constructor
    PointerSentinel<TestClass> weakSentinel(weakPtr_);
    EXPECT_TRUE(weakSentinel.is_valid());
    EXPECT_EQ(weakSentinel.get(), sharedPtr_.get());
}

// Test constructor error cases
TEST_F(PointerSentinelTest, ConstructorErrors) {
    // Null raw pointer
    EXPECT_THROW(
        { PointerSentinel<TestClass> sentinel((TestClass*)nullptr); },
        PointerException);

    // Null shared pointer
    EXPECT_THROW(
        {
            std::shared_ptr<TestClass> nullPtr;
            PointerSentinel<TestClass> sentinel(nullPtr);
        },
        PointerException);

    // Null unique pointer
    EXPECT_THROW(
        {
            std::unique_ptr<TestClass> nullPtr;
            PointerSentinel<TestClass> sentinel(std::move(nullPtr));
        },
        PointerException);

    // Expired weak pointer
    EXPECT_THROW(
        {
            std::weak_ptr<TestClass> expiredPtr;
            PointerSentinel<TestClass> sentinel(expiredPtr);
        },
        PointerException);
}

// Test copy constructor
TEST_F(PointerSentinelTest, CopyConstructor) {
    // Create original sentinels
    PointerSentinel<TestClass> rawSentinel(rawPtr_);
    PointerSentinel<TestClass> sharedSentinel(sharedPtr_);

    // Copy construct
    PointerSentinel<TestClass> rawCopy(rawSentinel);
    PointerSentinel<TestClass> sharedCopy(sharedSentinel);

    // Validate copies
    EXPECT_TRUE(rawCopy.is_valid());
    EXPECT_NE(rawCopy.get(), rawPtr_);  // Raw pointer should be deep copied
    EXPECT_EQ(rawCopy.get()->getValue(), rawPtr_->getValue());

    EXPECT_TRUE(sharedCopy.is_valid());
    EXPECT_EQ(sharedCopy.get(),
              sharedPtr_.get());  // Shared_ptr should share ownership
}

// Test move constructor
TEST_F(PointerSentinelTest, MoveConstructor) {
    // Create original sentinels
    PointerSentinel<TestClass> rawSentinel(new TestClass(500));
    PointerSentinel<TestClass> sharedSentinel(std::make_shared<TestClass>(600));

    // Store pointers for comparison
    TestClass* rawPtr = rawSentinel.get();
    TestClass* sharedPtr = sharedSentinel.get();

    // Move construct
    PointerSentinel<TestClass> rawMoved(std::move(rawSentinel));
    PointerSentinel<TestClass> sharedMoved(std::move(sharedSentinel));

    // Validate moves
    EXPECT_TRUE(rawMoved.is_valid());
    EXPECT_EQ(rawMoved.get(), rawPtr);

    EXPECT_TRUE(sharedMoved.is_valid());
    EXPECT_EQ(sharedMoved.get(), sharedPtr);

    // Original should be invalid
    EXPECT_FALSE(rawSentinel.is_valid());
    EXPECT_FALSE(sharedSentinel.is_valid());
}

// Test copy assignment
TEST_F(PointerSentinelTest, CopyAssignment) {
    PointerSentinel<TestClass> rawSentinel(rawPtr_);
    PointerSentinel<TestClass> sharedSentinel(sharedPtr_);

    // Target sentinels
    PointerSentinel<TestClass> rawTarget;
    PointerSentinel<TestClass> sharedTarget;

    // Copy assign
    rawTarget = rawSentinel;
    sharedTarget = sharedSentinel;

    // Validate copies
    EXPECT_TRUE(rawTarget.is_valid());
    EXPECT_NE(rawTarget.get(), rawPtr_);  // Raw pointer should be deep copied
    EXPECT_EQ(rawTarget.get()->getValue(), rawPtr_->getValue());

    EXPECT_TRUE(sharedTarget.is_valid());
    EXPECT_EQ(sharedTarget.get(),
              sharedPtr_.get());  // Shared_ptr should share ownership

    // Self assignment
    rawTarget = rawTarget;
    EXPECT_TRUE(rawTarget.is_valid());
}

// Test move assignment
TEST_F(PointerSentinelTest, MoveAssignment) {
    PointerSentinel<TestClass> rawSentinel(new TestClass(500));
    PointerSentinel<TestClass> sharedSentinel(std::make_shared<TestClass>(600));

    // Store pointers for comparison
    TestClass* rawPtr = rawSentinel.get();
    TestClass* sharedPtr = sharedSentinel.get();

    // Target sentinels
    PointerSentinel<TestClass> rawTarget;
    PointerSentinel<TestClass> sharedTarget;

    // Move assign
    rawTarget = std::move(rawSentinel);
    sharedTarget = std::move(sharedSentinel);

    // Validate moves
    EXPECT_TRUE(rawTarget.is_valid());
    EXPECT_EQ(rawTarget.get(), rawPtr);

    EXPECT_TRUE(sharedTarget.is_valid());
    EXPECT_EQ(sharedTarget.get(), sharedPtr);

    // Original should be invalid
    EXPECT_FALSE(rawSentinel.is_valid());
    EXPECT_FALSE(sharedSentinel.is_valid());

    // Self move assignment
    rawTarget = std::move(rawTarget);
    EXPECT_TRUE(rawTarget.is_valid());
}

// Test get and get_noexcept methods
TEST_F(PointerSentinelTest, GetMethods) {
    PointerSentinel<TestClass> rawSentinel(rawPtr_);
    PointerSentinel<TestClass> sharedSentinel(sharedPtr_);

    // Test get
    EXPECT_EQ(rawSentinel.get(), rawPtr_);
    EXPECT_EQ(sharedSentinel.get(), sharedPtr_.get());

    // Test get_noexcept
    EXPECT_EQ(rawSentinel.get_noexcept(), rawPtr_);
    EXPECT_EQ(sharedSentinel.get_noexcept(), sharedPtr_.get());

    // Invalid sentinel
    PointerSentinel<TestClass> invalidSentinel;
    EXPECT_THROW(invalidSentinel.get(), PointerException);
    EXPECT_EQ(invalidSentinel.get_noexcept(), nullptr);

    // Expired weak pointer
    std::shared_ptr<TestClass> tempShared = std::make_shared<TestClass>();
    std::weak_ptr<TestClass> tempWeak = tempShared;
    PointerSentinel<TestClass> weakSentinel(tempWeak);

    tempShared.reset();
    EXPECT_THROW(weakSentinel.get(), PointerException);
    EXPECT_EQ(weakSentinel.get_noexcept(), nullptr);
}

// Test invoke method
TEST_F(PointerSentinelTest, Invoke) {
    PointerSentinel<TestClass> sentinel(new TestClass(42));

    // Invoke getter
    EXPECT_EQ(sentinel.invoke(&TestClass::getValue), 42);

    // Invoke setter
    sentinel.invoke(&TestClass::setValue, 123);
    EXPECT_EQ(sentinel.invoke(&TestClass::getValue), 123);

    // Invoke method with multiple parameters
    EXPECT_EQ(sentinel.invoke(&TestClass::add, 10, 20), 153);  // 10 + 20 + 123

    // Invoke returning string
    EXPECT_EQ(sentinel.invoke(&TestClass::toString), "123");

    // Invalid sentinel
    PointerSentinel<TestClass> invalidSentinel;
    EXPECT_THROW(invalidSentinel.invoke(&TestClass::getValue),
                 PointerException);
}

// Test apply method
TEST_F(PointerSentinelTest, Apply) {
    PointerSentinel<TestClass> sentinel(new TestClass(42));

    // Apply lambda
    int result =
        sentinel.apply([](TestClass* obj) { return obj->getValue() * 2; });
    EXPECT_EQ(result, 84);

    // Apply lambda that modifies object
    sentinel.apply([](TestClass* obj) {
        obj->setValue(obj->getValue() + 10);
        return 0;  // Return value not used
    });
    EXPECT_EQ(sentinel.invoke(&TestClass::getValue), 52);

    // Apply with capture
    int multiplier = 3;
    result = sentinel.apply(
        [multiplier](TestClass* obj) { return obj->getValue() * multiplier; });
    EXPECT_EQ(result, 156);  // 52 * 3

    // Invalid sentinel
    PointerSentinel<TestClass> invalidSentinel;
    EXPECT_THROW(
        invalidSentinel.apply([](TestClass* obj) { return obj->getValue(); }),
        PointerException);
}

// Test applyVoid method
TEST_F(PointerSentinelTest, ApplyVoid) {
    PointerSentinel<TestClass> sentinel(new TestClass(42));

    // Apply void function
    sentinel.applyVoid([](TestClass* obj) { obj->setValue(100); });
    EXPECT_EQ(sentinel.invoke(&TestClass::getValue), 100);

    // Apply void function with additional args
    sentinel.applyVoid([](TestClass* obj, int value) { obj->setValue(value); },
                       200);
    EXPECT_EQ(sentinel.invoke(&TestClass::getValue), 200);

    // Invalid sentinel
    PointerSentinel<TestClass> invalidSentinel;
    EXPECT_THROW(
        invalidSentinel.applyVoid([](TestClass* obj) { obj->setValue(300); }),
        PointerException);
}

// Test type conversion
TEST_F(PointerSentinelTest, ConvertTo) {
    // Create a derived class for testing conversions
    class DerivedClass : public TestClass {
    public:
        explicit DerivedClass(int value)
            : TestClass(value), extra_(value * 10) {}
        int getExtra() const { return extra_; }

    private:
        int extra_;
    };

    // Create sentinel with derived class
    auto derived = std::make_shared<DerivedClass>(42);
    PointerSentinel<DerivedClass> derivedSentinel(derived);

    // Convert to base class
    PointerSentinel<TestClass> baseSentinel =
        derivedSentinel.convert_to<TestClass>();
    EXPECT_TRUE(baseSentinel.is_valid());
    EXPECT_EQ(baseSentinel.invoke(&TestClass::getValue), 42);

    // Convert back to derived class (downcast)
    // TODO: Fix this test
    // auto derivedPtr = dynamic_cast<DerivedClass*>(baseSentinel.get());
    // ASSERT_NE(derivedPtr, nullptr);
    // EXPECT_EQ(derivedPtr->getExtra(), 420);  // 42 * 10

    // Invalid conversion should fail to compile
    // Uncomment to test:
    // auto invalidConversion = derivedSentinel.convert_to<std::string>();
}

// Test async operations
TEST_F(PointerSentinelTest, AsyncOperations) {
    PointerSentinel<TestClass> sentinel(new TestClass(42));

    // Apply async - simple retrieval
    auto future1 =
        sentinel.apply_async([](TestClass* obj) { return obj->getValue(); });
    EXPECT_EQ(future1.get(), 42);

    // Apply async - with sleep to verify async behavior
    auto start = std::chrono::steady_clock::now();
    auto future2 = sentinel.apply_async([](TestClass* obj) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return obj->getValue() * 2;
    });
    auto result = future2.get();
    auto duration = std::chrono::steady_clock::now() - start;

    EXPECT_EQ(result, 84);  // 42 * 2
    EXPECT_GE(
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(),
        90);

    // Invalid sentinel
    PointerSentinel<TestClass> invalidSentinel;
    EXPECT_THROW(invalidSentinel.apply_async(
                     [](TestClass* obj) { return obj->getValue(); }),
                 PointerException);
}

// Test SIMD operations
TEST_F(PointerSentinelTest, SimdOperations) {
    // Create an array of objects for SIMD processing
    constexpr size_t ARRAY_SIZE = 10;
    auto array = new TestClass[ARRAY_SIZE];
    PointerSentinel<TestClass> sentinel(array);

    // Apply SIMD operation
    sentinel.apply_simd(testSimdOperation, ARRAY_SIZE);

    // Verify results
    for (size_t i = 0; i < ARRAY_SIZE; ++i) {
        EXPECT_EQ(array[i].getValue(), static_cast<int>(i));
    }

    // Clean up
    delete[] array;

    // Invalid sentinel
    PointerSentinel<TestClass> invalidSentinel;
    EXPECT_THROW(invalidSentinel.apply_simd(testSimdOperation, ARRAY_SIZE),
                 PointerException);
}

// Test thread safety
TEST_F(PointerSentinelTest, ThreadSafety) {
    constexpr int THREAD_COUNT = 10;
    constexpr int OPERATIONS_PER_THREAD = 1000;

    auto sharedObj = std::make_shared<TestClass>(0);
    PointerSentinel<TestClass> sentinel(sharedObj);

    std::vector<std::thread> threads;

    // Create threads that increment the value concurrently
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&sentinel, i, OPERATIONS_PER_THREAD]() {
            for (int j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                sentinel.applyVoid(
                    [](TestClass* obj) { obj->setValue(obj->getValue() + 1); });

                if (j % 100 == 0) {
                    // Occasionally read the value
                    sentinel.apply(
                        [](TestClass* obj) { return obj->getValue(); });
                }
            }
        });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify the final value (should be THREAD_COUNT * OPERATIONS_PER_THREAD)
    EXPECT_EQ(sentinel.invoke(&TestClass::getValue),
              THREAD_COUNT * OPERATIONS_PER_THREAD);
}

// Test weak pointer behavior
TEST_F(PointerSentinelTest, WeakPointerBehavior) {
    // Create a scope for the shared_ptr to go out of scope
    PointerSentinel<TestClass> weakSentinel;
    {
        auto sharedObj = std::make_shared<TestClass>(42);
        std::weak_ptr<TestClass> weakObj = sharedObj;
        weakSentinel = PointerSentinel<TestClass>(weakObj);

        // Should work fine while shared_ptr exists
        EXPECT_EQ(weakSentinel.invoke(&TestClass::getValue), 42);
    }

    // After shared_ptr is destroyed, the weak_ptr should be expired
    EXPECT_THROW(weakSentinel.get(), PointerException);
    EXPECT_EQ(weakSentinel.get_noexcept(), nullptr);

    EXPECT_THROW(weakSentinel.invoke(&TestClass::getValue), PointerException);
    EXPECT_THROW(
        weakSentinel.apply([](TestClass* obj) { return obj->getValue(); }),
        PointerException);
}

// Test destructor cleanup
TEST_F(PointerSentinelTest, DestructorCleanup) {
    // Use a static counter to track object destructions
    static int destructorCalls = 0;

    class TrackDestructor {
    public:
        TrackDestructor() = default;
        ~TrackDestructor() { destructorCalls++; }
    };

    destructorCalls = 0;

    // Create a scope for the sentinel to go out of scope
    {
        PointerSentinel<TrackDestructor> sentinel(new TrackDestructor());
    }  // sentinel goes out of scope here

    // Raw pointer should be deleted
    EXPECT_EQ(destructorCalls, 1);

    // Reset counter for next test
    destructorCalls = 0;

    // Test unique_ptr cleanup
    {
        PointerSentinel<TrackDestructor> sentinel(
            std::make_unique<TrackDestructor>());
    }

    // unique_ptr should be deleted
    EXPECT_EQ(destructorCalls, 1);
}

// Test exception propagation
TEST_F(PointerSentinelTest, ExceptionPropagation) {
    PointerSentinel<TestClass> sentinel(new TestClass(42));

    // Function that throws an exception
    auto throwingFunc = [](TestClass*) -> int {
        throw std::runtime_error("Test exception");
    };

    // Exception from apply should be wrapped in PointerException
    EXPECT_THROW(
        {
            try {
                sentinel.apply(throwingFunc);
            } catch (const PointerException& e) {
                // Verify the original exception message is included
                EXPECT_NE(std::string(e.what()).find("Test exception"),
                          std::string::npos);
                throw;
            }
        },
        PointerException);

    // Similar for invoke
    EXPECT_THROW({ sentinel.invoke(&TestClass::toString); }, PointerException);
}

// Test with const objects and methods
TEST_F(PointerSentinelTest, ConstObjectHandling) {
    class ConstMethodTest {
    public:
        ConstMethodTest() = default;
        int getValue() const { return 42; }
        int nonConstMethod() { return 123; }
    };

    PointerSentinel<ConstMethodTest> sentinel(new ConstMethodTest());

    // Call const method
    EXPECT_EQ(sentinel.invoke(&ConstMethodTest::getValue), 42);

    // Call non-const method
    EXPECT_EQ(sentinel.invoke(&ConstMethodTest::nonConstMethod), 123);
}

// Test with void return types
TEST_F(PointerSentinelTest, VoidReturnTypes) {
    class VoidReturnTest {
    public:
        VoidReturnTest() : value_(0) {}
        void increment() { value_++; }
        int getValue() const { return value_; }

    private:
        int value_;
    };

    PointerSentinel<VoidReturnTest> sentinel(new VoidReturnTest());

    // Call void return method through invoke
    sentinel.invoke(&VoidReturnTest::increment);
    EXPECT_EQ(sentinel.invoke(&VoidReturnTest::getValue), 1);
}

// Main function to run the tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

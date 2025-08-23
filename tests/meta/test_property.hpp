#ifndef ATOM_META_TEST_PROPERTY_HPP
#define ATOM_META_TEST_PROPERTY_HPP

#include <gtest/gtest.h>
#include "atom/meta/property.hpp"

#include <atomic>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace atom::meta::test {

// Custom copyable types for testing
struct Point {
    int x, y;

    Point(int x = 0, int y = 0) : x(x), y(y) {}

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }

    Point operator+(const Point& other) const {
        return Point(x + other.x, y + other.y);
    }

    Point operator-(const Point& other) const {
        return Point(x - other.x, y - other.y);
    }

    Point operator*(const Point& other) const {
        return Point(x * other.x, y * other.y);
    }

    Point operator/(const Point& other) const {
        return Point(x / (other.x ? other.x : 1), y / (other.y ? other.y : 1));
    }

    Point operator%(const Point& other) const {
        return Point(x % (other.x ? other.x : 1), y % (other.y ? other.y : 1));
    }

    auto operator<=>(const Point& other) const = default;

    friend std::ostream& operator<<(std::ostream& os, const Point& p) {
        os << "(" << p.x << ", " << p.y << ")";
        return os;
    }
};

// Test fixture for Property
class PropertyTest : public ::testing::Test {
protected:
    // Test class with properties defined using macros
    class TestClass {
    public:
        DEFINE_RW_PROPERTY(int, readWrite)
        DEFINE_RO_PROPERTY(std::string, readOnly)
        DEFINE_WO_PROPERTY(double, writeOnly)

        // Constructor to initialize the properties
        TestClass() : readWrite_(0), readOnly_("ReadOnly"), writeOnly_(0.0) {}

        // Method to check writeOnly value
        double getWriteOnlyValue() const { return writeOnly_; }
    };

    void SetUp() override {}
    void TearDown() override {}
};

// Test Property constructors
TEST_F(PropertyTest, Constructors) {
    // Default constructor
    Property<int> defaultProp;
    EXPECT_THROW(static_cast<int>(defaultProp), atom::error::InvalidArgument);

    // Constructor with initial value
    Property<int> valueProp(42);
    EXPECT_EQ(static_cast<int>(valueProp), 42);

    // Constructor with getter function
    bool getterCalled = false;
    Property<int> getterProp([&getterCalled]() {
        getterCalled = true;
        return 123;
    });
    EXPECT_EQ(static_cast<int>(getterProp), 123);
    EXPECT_TRUE(getterCalled);

    // Constructor with getter and setter functions
    bool setterCalled = false;
    int setterValue = 0;
    Property<int> getterSetterProp([]() { return 456; },
                                   [&setterCalled, &setterValue](int val) {
                                       setterCalled = true;
                                       setterValue = val;
                                   });
    EXPECT_EQ(static_cast<int>(getterSetterProp), 456);
    getterSetterProp = 789;
    EXPECT_TRUE(setterCalled);
    EXPECT_EQ(setterValue, 789);
}

// Test copy and move operations
TEST_F(PropertyTest, CopyAndMove) {
    // Setup properties
    Property<int> original(42);

    // Test copy constructor
    Property<int> copied(original);
    EXPECT_EQ(static_cast<int>(copied), 42);

    // Test copy assignment
    Property<int> copyAssigned;
    copyAssigned = original;
    EXPECT_EQ(static_cast<int>(copyAssigned), 42);

    // Test move constructor
    Property<int> moved(std::move(copied));
    EXPECT_EQ(static_cast<int>(moved), 42);

    // Test move assignment
    Property<int> moveAssigned;
    moveAssigned = std::move(moved);
    EXPECT_EQ(static_cast<int>(moveAssigned), 42);

    // Test with properties that have getters/setters
    int value = 100;
    Property<int> withAccessors([&value]() { return value; },
                                [&value](int v) { value = v; });

    // Test copy preserves accessors
    Property<int> copiedWithAccessors(withAccessors);
    EXPECT_EQ(static_cast<int>(copiedWithAccessors), 100);
    copiedWithAccessors = 200;
    EXPECT_EQ(value, 200);
}

// Test property value access and modification
TEST_F(PropertyTest, ValueAccessAndModification) {
    // Property with direct value
    Property<int> prop(10);
    EXPECT_EQ(static_cast<int>(prop), 10);

    // Modify the property
    prop = 20;
    EXPECT_EQ(static_cast<int>(prop), 20);

    // Property with getter/setter functions
    int backingValue = 30;
    Property<int> funcProp([&backingValue]() { return backingValue; },
                           [&backingValue](int val) { backingValue = val; });

    EXPECT_EQ(static_cast<int>(funcProp), 30);
    funcProp = 40;
    EXPECT_EQ(backingValue, 40);
    EXPECT_EQ(static_cast<int>(funcProp), 40);

    // Test onChange callback
    bool onChangeCalled = false;
    int changedValue = 0;

    Property<int> withCallback(50);
    withCallback.setOnChange([&onChangeCalled, &changedValue](const int& val) {
        onChangeCalled = true;
        changedValue = val;
    });

    withCallback = 60;
    EXPECT_TRUE(onChangeCalled);
    EXPECT_EQ(changedValue, 60);

    // Manual notification
    onChangeCalled = false;
    changedValue = 0;
    withCallback.notifyChange(70);
    EXPECT_TRUE(onChangeCalled);
    EXPECT_EQ(changedValue, 70);
    // Value should not change with manual notification
    EXPECT_EQ(static_cast<int>(withCallback), 60);
}

// Test making properties read-only or write-only
TEST_F(PropertyTest, AccessRestrictions) {
    // Create a property with getter and setter
    int value = 100;
    Property<int> prop([&value]() { return value; },
                       [&value](int val) { value = val; });

    // Test baseline functionality
    EXPECT_EQ(static_cast<int>(prop), 100);
    prop = 200;
    EXPECT_EQ(value, 200);

    // Make read-only
    prop.makeReadonly();
    EXPECT_EQ(static_cast<int>(prop), 200);
    prop = 300;  // This won't affect value
    EXPECT_EQ(value, 200);

    // Create a new property for write-only test
    value = 100;
    Property<int> prop2([&value]() { return value; },
                        [&value](int val) { value = val; });

    // Make write-only
    prop2.makeWriteonly();
    EXPECT_THROW(static_cast<int>(prop2), atom::error::InvalidArgument);
    prop2 = 300;
    EXPECT_EQ(value, 300);

    // Clear all accessors
    prop2.clear();
    EXPECT_THROW(static_cast<int>(prop2), atom::error::InvalidArgument);
    prop2 = 400;  // This won't affect value
    EXPECT_EQ(value, 300);
}

// Test operator overloading
TEST_F(PropertyTest, Operators) {
    // Create a property with a value
    Property<int> intProp(10);

    // Test arithmetic assignment operators
    intProp += 5;
    EXPECT_EQ(static_cast<int>(intProp), 15);

    intProp -= 3;
    EXPECT_EQ(static_cast<int>(intProp), 12);

    intProp *= 2;
    EXPECT_EQ(static_cast<int>(intProp), 24);

    intProp /= 3;
    EXPECT_EQ(static_cast<int>(intProp), 8);

    intProp %= 3;
    EXPECT_EQ(static_cast<int>(intProp), 2);

    // Test comparison operators
    Property<int> otherProp(2);
    EXPECT_TRUE(static_cast<int>(intProp) == static_cast<int>(otherProp));
    EXPECT_FALSE(static_cast<int>(intProp) != static_cast<int>(otherProp));

    otherProp = 3;
    EXPECT_FALSE(static_cast<int>(intProp) == static_cast<int>(otherProp));
    EXPECT_TRUE(static_cast<int>(intProp) != static_cast<int>(otherProp));

    // Test three-way comparison
    EXPECT_TRUE(static_cast<int>(intProp) < static_cast<int>(otherProp));
    EXPECT_TRUE(static_cast<int>(otherProp) > static_cast<int>(intProp));

    // Test stream output
    std::ostringstream oss;
    oss << intProp;
    EXPECT_EQ(oss.str(), "2");

    // Test with custom type
    Property<Point> pointProp(Point(1, 2));

    // Test arithmetic operators with custom type
    pointProp += Point(2, 3);
    EXPECT_EQ(static_cast<Point>(pointProp), Point(3, 5));

    pointProp -= Point(1, 2);
    EXPECT_EQ(static_cast<Point>(pointProp), Point(2, 3));

    pointProp *= Point(2, 2);
    EXPECT_EQ(static_cast<Point>(pointProp), Point(4, 6));

    pointProp /= Point(2, 3);
    EXPECT_EQ(static_cast<Point>(pointProp), Point(2, 2));

    pointProp %= Point(3, 3);
    EXPECT_EQ(static_cast<Point>(pointProp), Point(2, 2));

    // Test stream output with custom type
    std::ostringstream ossPoint;
    ossPoint << pointProp;
    EXPECT_EQ(ossPoint.str(), "(2, 2)");
}

// Test asynchronous operations
TEST_F(PropertyTest, AsyncOperations) {
    // Create a property
    Property<int> prop(10);

    // Test asyncGet
    auto futureGet = prop.asyncGet();
    EXPECT_EQ(futureGet.get(), 10);

    // Test asyncSet
    auto futureSet = prop.asyncSet(20);
    futureSet.wait();  // Wait for completion
    EXPECT_EQ(static_cast<int>(prop), 20);

    // Test concurrent async operations
    std::vector<std::thread> threads;
    std::atomic<int> successCount(0);

    // Spawn multiple threads to read/write the property
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&prop, &successCount, i]() {
            try {
                if (i % 2 == 0) {
                    // Even threads read
                    auto future = prop.asyncGet();
                    int val = future.get();
                    if (val >= 20)
                        successCount++;
                } else {
                    // Odd threads write
                    auto future = prop.asyncSet(20 + i);
                    future.wait();
                    successCount++;
                }
            } catch (...) {
                // Count failed operations
            }
        });
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successCount.load(), 10);  // All operations should succeed
}

// Test property caching
TEST_F(PropertyTest, Caching) {
    // Create a property with an expensive getter
    int computeCount = 0;
    Property<int> prop([&computeCount]() {
        computeCount++;
        return computeCount * 10;
    });

    // First access computes the value
    EXPECT_EQ(static_cast<int>(prop), 10);
    EXPECT_EQ(computeCount, 1);

    // Cache a value
    prop.cacheValue("key1", 100);

    // Get cached value
    auto cachedValue = prop.getCachedValue("key1");
    EXPECT_TRUE(cachedValue.has_value());
    EXPECT_EQ(cachedValue.value(), 100);

    // Access a non-existent cache key
    auto nonExistent = prop.getCachedValue("nonexistent");
    EXPECT_FALSE(nonExistent.has_value());

    // Clear the cache
    prop.clearCache();
    auto clearedCache = prop.getCachedValue("key1");
    EXPECT_FALSE(clearedCache.has_value());

    // Test cache with multiple threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&prop, i]() {
            // Each thread caches a different value
            prop.cacheValue("key" + std::to_string(i), i * 100);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify all cached values
    for (int i = 0; i < 10; ++i) {
        auto val = prop.getCachedValue("key" + std::to_string(i));
        EXPECT_TRUE(val.has_value());
        EXPECT_EQ(val.value(), i * 100);
    }
}

// Test using the Property macros
TEST_F(PropertyTest, PropertyMacros) {
    TestClass obj;

    // Test read-write property
    EXPECT_EQ(static_cast<int>(obj.readWrite), 0);
    obj.readWrite = 42;
    EXPECT_EQ(static_cast<int>(obj.readWrite), 42);

    // Test read-only property
    EXPECT_EQ(static_cast<std::string>(obj.readOnly), "ReadOnly");
    // obj.readOnly = "NewValue"; // This should not compile if uncommented

    // Test write-only property
    // EXPECT_THROW(static_cast<double>(obj.writeOnly),
    // atom::error::InvalidArgumentError);
    obj.writeOnly = 3.14;
    EXPECT_DOUBLE_EQ(obj.getWriteOnlyValue(), 3.14);
}

// Test thread-safety and concurrent access
TEST_F(PropertyTest, ThreadSafety) {
    // Create a property
    Property<int> prop(0);

    // Set up multiple threads
    constexpr int numThreads = 100;
    constexpr int opsPerThread = 100;
    std::vector<std::thread> threads;

    // Increment the property value from multiple threads
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&prop]() {
            for (int j = 0; j < opsPerThread; ++j) {
                int currentVal = static_cast<int>(prop);
                prop = currentVal + 1;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    // Expected final value
    EXPECT_EQ(static_cast<int>(prop), numThreads * opsPerThread);

    // Test concurrent cache access
    Property<int> cachedProp(0);
    threads.clear();

    // Each thread adds items to the cache
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&cachedProp, i]() {
            for (int j = 0; j < 10; ++j) {
                std::string key =
                    "thread" + std::to_string(i) + "_" + std::to_string(j);
                cachedProp.cacheValue(key, i * 100 + j);
            }
        });
    }

    // Wait for cache population
    for (auto& t : threads) {
        t.join();
    }

    // Verify all cache entries
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            std::string key =
                "thread" + std::to_string(i) + "_" + std::to_string(j);
            auto val = cachedProp.getCachedValue(key);
            EXPECT_TRUE(val.has_value());
            EXPECT_EQ(val.value(), i * 100 + j);
        }
    }
}

// Test edge cases
TEST_F(PropertyTest, EdgeCases) {
    // Test with empty getter/setter functions
    Property<int> emptyProp;
    EXPECT_THROW(static_cast<int>(emptyProp),
                 atom::error::InvalidArgument);

    // Test with nullptr callbacks
    emptyProp.setOnChange(nullptr);
    emptyProp = 42;  // Should not crash

    // Test assignment with the same value
    Property<int> prop(10);
    bool onChangeCalled = false;
    prop.setOnChange([&onChangeCalled](const int&) { onChangeCalled = true; });

    prop = 10;                    // Same value
    EXPECT_TRUE(onChangeCalled);  // onChange should still be called

    // Test with complex types and move semantics
    Property<std::vector<int>> vecProp;
    std::vector<int> vec = {1, 2, 3};
    vecProp = vec;
    EXPECT_EQ(static_cast<std::vector<int>>(vecProp).size(), 3);

    // Move assignment
    vecProp = std::vector<int>{4, 5, 6, 7};
    auto result = static_cast<std::vector<int>>(vecProp);
    EXPECT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], 4);
    EXPECT_EQ(result[3], 7);
}

// Test error handling
TEST_F(PropertyTest, ErrorHandling) {
    // Test with throwing getter
    Property<int> throwingProp(
        []() -> int { throw std::runtime_error("Getter error"); });

    EXPECT_THROW(static_cast<int>(throwingProp), std::runtime_error);

    // Test with throwing setter
    Property<int> throwingSetterProp(
        []() -> int { return 0; },
        [](int) { throw std::runtime_error("Setter error"); });

    EXPECT_THROW(throwingSetterProp = 42, std::runtime_error);

    // Test with throwing onChange callback
    Property<int> throwingCallbackProp(10);
    throwingCallbackProp.setOnChange(
        [](const int&) { throw std::runtime_error("Callback error"); });

    EXPECT_THROW(throwingCallbackProp = 20, std::runtime_error);

    // Test async operation with throwing function
    Property<int> asyncThrowingProp(
        []() -> int { throw std::runtime_error("Async getter error"); });

    auto future = asyncThrowingProp.asyncGet();
    EXPECT_THROW(future.get(), std::runtime_error);
}

}  // namespace atom::meta::test

// Main function to run the tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#endif  // ATOM_META_TEST_PROPERTY_HPP

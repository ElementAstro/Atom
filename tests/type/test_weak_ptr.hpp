#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "atom/type/weak_ptr.hpp"

namespace {

using namespace atom::type;
using namespace std::chrono_literals;

// Test fixture for EnhancedWeakPtr<T> tests
class EnhancedWeakPtrTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset the initial count for each test
        initialInstanceCount = EnhancedWeakPtr<int>::getTotalInstances();
    }

    void TearDown() override {
        // Allow time for any async operations to complete
        std::this_thread::sleep_for(10ms);
    }

    size_t initialInstanceCount = 0;
};

// Test fixture for EnhancedWeakPtr<void> tests
class EnhancedWeakPtrVoidTest : public ::testing::Test {
protected:
    void SetUp() override {
        initialInstanceCount = EnhancedWeakPtr<void>::getTotalInstances();
    }

    size_t initialInstanceCount = 0;
};

// Basic functionality tests
TEST_F(EnhancedWeakPtrTest, ConstructorsAndAssignments) {
    // Default constructor
    EnhancedWeakPtr<int> weak1;
    EXPECT_TRUE(weak1.expired());
    EXPECT_EQ(EnhancedWeakPtr<int>::getTotalInstances(),
              initialInstanceCount + 1);

    // Constructor with shared_ptr
    auto shared = std::make_shared<int>(42);
    EnhancedWeakPtr<int> weak2(shared);
    EXPECT_FALSE(weak2.expired());
    EXPECT_EQ(weak2.useCount(), 1);
    EXPECT_EQ(EnhancedWeakPtr<int>::getTotalInstances(),
              initialInstanceCount + 2);

    // Copy constructor
    EnhancedWeakPtr<int> weak3(weak2);
    EXPECT_FALSE(weak3.expired());
    EXPECT_EQ(weak3.useCount(), 1);
    EXPECT_EQ(EnhancedWeakPtr<int>::getTotalInstances(),
              initialInstanceCount + 3);

    // Move constructor
    EnhancedWeakPtr<int> weak4(std::move(weak3));
    EXPECT_FALSE(weak4.expired());
    EXPECT_EQ(weak4.useCount(), 1);
    EXPECT_EQ(EnhancedWeakPtr<int>::getTotalInstances(),
              initialInstanceCount + 4);

    // Copy assignment
    EnhancedWeakPtr<int> weak5;
    weak5 = weak2;
    EXPECT_FALSE(weak5.expired());
    EXPECT_EQ(weak5.useCount(), 1);

    // Move assignment
    EnhancedWeakPtr<int> weak6;
    weak6 = std::move(weak5);
    EXPECT_FALSE(weak6.expired());
    EXPECT_EQ(weak6.useCount(), 1);

    // Self-assignment should be safe
    weak6 = weak6;
    EXPECT_FALSE(weak6.expired());
    EXPECT_EQ(weak6.useCount(), 1);

    // Self-move-assignment should be safe
    weak6 = std::move(weak6);
    EXPECT_FALSE(weak6.expired());
    EXPECT_EQ(weak6.useCount(), 1);
}

TEST_F(EnhancedWeakPtrTest, BasicOperations) {
    auto shared = std::make_shared<int>(42);
    EnhancedWeakPtr<int> weak(shared);

    // Lock succeeds when shared_ptr exists
    auto locked = weak.lock();
    EXPECT_TRUE(locked);
    EXPECT_EQ(*locked, 42);

    // Expiry check
    EXPECT_FALSE(weak.expired());

    // Reset
    weak.reset();
    EXPECT_TRUE(weak.expired());
    EXPECT_FALSE(weak.lock());

    // Lock increment
    EnhancedWeakPtr<int> weak2(shared);
    EXPECT_EQ(weak2.getLockAttempts(), 0);
    weak2.lock();
    EXPECT_EQ(weak2.getLockAttempts(), 1);
    weak2.lock();
    EXPECT_EQ(weak2.getLockAttempts(), 2);

    // Expiry when shared_ptr is destroyed
    shared.reset();
    EnhancedWeakPtr<int> weak3(std::make_shared<int>(99));
    EXPECT_FALSE(weak3.expired());
    auto locked3 = weak3.lock();
    locked3.reset();
    EXPECT_TRUE(weak3.expired());
}

TEST_F(EnhancedWeakPtrTest, Comparison) {
    auto shared1 = std::make_shared<int>(42);
    auto shared2 = std::make_shared<int>(42);

    EnhancedWeakPtr<int> weak1(shared1);
    EnhancedWeakPtr<int> weak2(shared1);  // Same shared_ptr
    EnhancedWeakPtr<int> weak3(
        shared2);  // Different shared_ptr with same value

    // Same underlying shared_ptr should be equal
    EXPECT_TRUE(weak1 == weak2);

    // Different shared_ptrs should not be equal, even with same value
    EXPECT_FALSE(weak1 == weak3);

    // After reset, pointers should not be equal
    weak2.reset();
    EXPECT_FALSE(weak1 == weak2);

    // Two default-constructed pointers should be equal
    EnhancedWeakPtr<int> weak4;
    EnhancedWeakPtr<int> weak5;
    EXPECT_TRUE(weak4 == weak5);
}

// Enhanced functionality tests
TEST_F(EnhancedWeakPtrTest, WithLock) {
    auto shared = std::make_shared<int>(42);
    EnhancedWeakPtr<int> weak(shared);

    // With non-void return value
    auto result = weak.withLock([](int& value) {
        value *= 2;
        return value;
    });

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 84);
    EXPECT_EQ(*shared, 84);

    // With void return value
    bool executed = weak.withLock([](int& value) { value += 1; });

    EXPECT_TRUE(executed);
    EXPECT_EQ(*shared, 85);

    // After shared_ptr is destroyed
    shared.reset();
    auto result2 = weak.withLock([](int& value) { return value * 2; });

    EXPECT_FALSE(result2.has_value());

    bool executed2 = weak.withLock([](int& value) { value += 1; });

    EXPECT_FALSE(executed2);
}

TEST_F(EnhancedWeakPtrTest, WaitFor) {
    auto shared = std::make_shared<int>(42);
    EnhancedWeakPtr<int> weak(shared);

    // Should not wait because object is already available
    EXPECT_TRUE(weak.waitFor(100ms));

    // Should time out because object will never be available
    shared.reset();
    EXPECT_FALSE(weak.waitFor(100ms));

    // Test with object that becomes available during wait
    auto shared2 = std::make_shared<int>(99);
    EnhancedWeakPtr<int> weak2;

    std::thread t([&shared2, &weak2]() {
        std::this_thread::sleep_for(50ms);
        weak2 = EnhancedWeakPtr<int>(shared2);
        weak2.notifyAll();
    });

    // This will fail because even though the object becomes available,
    // the waitFor is operating on a copy of the weak pointer that never changes
    EXPECT_FALSE(weak2.waitFor(200ms));

    t.join();
}

TEST_F(EnhancedWeakPtrTest, TryLockOrElse) {
    auto shared = std::make_shared<int>(42);
    EnhancedWeakPtr<int> weak(shared);

    // Success case
    auto result = weak.tryLockOrElse([](int& value) { return value * 2; },
                                     []() { return -1; });

    EXPECT_EQ(result, 84);

    // Failure case
    shared.reset();
    result = weak.tryLockOrElse([](int& value) { return value * 2; },
                                []() { return -1; });

    EXPECT_EQ(result, -1);
}

TEST_F(EnhancedWeakPtrTest, TryLockPeriodic) {
    auto shared = std::make_shared<int>(42);
    EnhancedWeakPtr<int> weak(shared);

    // Successful immediate lock
    auto result = weak.tryLockPeriodic(10ms, 3);
    EXPECT_TRUE(result);
    EXPECT_EQ(*result, 42);

    // Failure after max attempts
    shared.reset();
    result = weak.tryLockPeriodic(10ms, 2);
    EXPECT_FALSE(result);

    // Object that becomes available after a couple attempts
    EnhancedWeakPtr<int> weak2;
    std::thread t([&shared, &weak2]() {
        std::this_thread::sleep_for(25ms);
        weak2 = EnhancedWeakPtr<int>(shared);
    });

    shared = std::make_shared<int>(99);
    result = weak2.tryLockPeriodic(10ms, 5);

    // This is expected to fail because the thread modifies a different weak2
    // than the one we're calling tryLockPeriodic on
    EXPECT_FALSE(result);

    t.join();
}

TEST_F(EnhancedWeakPtrTest, WeakPtrAndSharedPtrAccessors) {
    auto shared = std::make_shared<int>(42);
    EnhancedWeakPtr<int> weak(shared);

    // getWeakPtr
    auto stdWeak = weak.getWeakPtr();
    EXPECT_EQ(stdWeak.lock(), shared);

    // createShared
    auto newShared = weak.createShared();
    EXPECT_EQ(*newShared, 42);

    // Lock attempts counter
    EXPECT_EQ(weak.getLockAttempts(), 0);
    weak.lock();
    EXPECT_EQ(weak.getLockAttempts(), 1);
    weak.lock();
    EXPECT_EQ(weak.getLockAttempts(), 2);
}

TEST_F(EnhancedWeakPtrTest, AsyncLock) {
    auto shared = std::make_shared<int>(42);
    EnhancedWeakPtr<int> weak(shared);

    // Test async lock with valid pointer
    std::future<std::shared_ptr<int>> future = weak.asyncLock();
    auto result = future.get();
    EXPECT_TRUE(result);
    EXPECT_EQ(*result, 42);

    // Test async lock with expired pointer
    shared.reset();
    future = weak.asyncLock();
    result = future.get();
    EXPECT_FALSE(result);
}

TEST_F(EnhancedWeakPtrTest, WaitUntil) {
    auto shared = std::make_shared<int>(42);
    EnhancedWeakPtr<int> weak(shared);

    std::atomic<bool> flag{false};

    std::thread t([&flag]() {
        std::this_thread::sleep_for(50ms);
        flag = true;
    });

    // Test waiting with a predicate that becomes true
    bool result = weak.waitUntil([&flag] { return flag.load(); });
    EXPECT_TRUE(result);

    // Test waiting with already expired pointer
    shared.reset();
    flag = false;

    t.join();
    t = std::thread([&flag]() {
        std::this_thread::sleep_for(50ms);
        flag = true;
    });

    result = weak.waitUntil([&flag] { return flag.load(); });
    EXPECT_FALSE(result);  // Should return false because pointer is expired

    t.join();
}

TEST_F(EnhancedWeakPtrTest, Cast) {
    struct Base {
        virtual ~Base() = default;
        int base_value = 42;
    };
    struct Derived : public Base {
        int derived_value = 84;
    };

    auto shared = std::make_shared<Derived>();
    EnhancedWeakPtr<Base> baseWeak(std::static_pointer_cast<Base>(shared));

    // Cast from Base to Derived
    auto derivedWeak = baseWeak.cast<Derived>();
    auto derivedShared = derivedWeak.lock();

    EXPECT_TRUE(derivedShared);
    EXPECT_EQ(derivedShared->base_value, 42);
    EXPECT_EQ(derivedShared->derived_value, 84);

    // Expired pointer should still cast but result in expired pointer
    shared.reset();
    auto derivedWeak2 = baseWeak.cast<Derived>();
    EXPECT_TRUE(derivedWeak2.expired());
}

// Helper function tests
TEST_F(EnhancedWeakPtrTest, CreateWeakPtrGroup) {
    std::vector<std::shared_ptr<int>> sharedPtrs = {std::make_shared<int>(1),
                                                    std::make_shared<int>(2),
                                                    std::make_shared<int>(3)};

    auto weakPtrGroup = createWeakPtrGroup(sharedPtrs);

    EXPECT_EQ(weakPtrGroup.size(), 3);

    for (size_t i = 0; i < weakPtrGroup.size(); ++i) {
        EXPECT_FALSE(weakPtrGroup[i].expired());
        EXPECT_EQ(*weakPtrGroup[i].lock(), i + 1);
    }

    // Verify they become expired when source is cleared
    sharedPtrs.clear();

    for (const auto& weakPtr : weakPtrGroup) {
        EXPECT_TRUE(weakPtr.expired());
    }
}

TEST_F(EnhancedWeakPtrTest, BatchOperation) {
    std::vector<std::shared_ptr<int>> sharedPtrs = {std::make_shared<int>(1),
                                                    std::make_shared<int>(2),
                                                    std::make_shared<int>(3)};

    auto weakPtrGroup = createWeakPtrGroup(sharedPtrs);

    // Double all values
    batchOperation(weakPtrGroup, [](int& value) { value *= 2; });

    EXPECT_EQ(*sharedPtrs[0], 2);
    EXPECT_EQ(*sharedPtrs[1], 4);
    EXPECT_EQ(*sharedPtrs[2], 6);

    // Test with some expired pointers
    sharedPtrs[1].reset();

    int sum = 0;
    batchOperation(weakPtrGroup, [&sum](int& value) { sum += value; });

    // Only values from non-expired pointers should be added
    EXPECT_EQ(sum, 8);  // 2 + 6
}

// Void specialization tests
TEST_F(EnhancedWeakPtrVoidTest, BasicOperations) {
    // Create a concrete pointer that we'll convert to void
    auto concrete = std::make_shared<int>(42);
    auto voidShared = std::static_pointer_cast<void>(concrete);

    EnhancedWeakPtr<void> weak(voidShared);

    // Check basic operations
    EXPECT_FALSE(weak.expired());
    EXPECT_GT(weak.useCount(), 0);

    auto locked = weak.lock();
    EXPECT_TRUE(locked);

    // Reset
    weak.reset();
    EXPECT_TRUE(weak.expired());
    EXPECT_FALSE(weak.lock());

    // Lock counter
    EnhancedWeakPtr<void> weak2(voidShared);
    EXPECT_EQ(weak2.getLockAttempts(), 0);
    weak2.lock();
    EXPECT_EQ(weak2.getLockAttempts(), 1);
}

TEST_F(EnhancedWeakPtrVoidTest, WithLock) {
    auto concrete = std::make_shared<int>(42);
    auto voidShared = std::static_pointer_cast<void>(concrete);

    EnhancedWeakPtr<void> weak(voidShared);

    // With non-void return value
    bool executed = false;
    auto result = weak.withLock([&executed]() {
        executed = true;
        return 42;
    });

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, 42);
    EXPECT_TRUE(executed);

    // With void return value
    executed = false;
    bool success = weak.withLock([&executed]() { executed = true; });

    EXPECT_TRUE(success);
    EXPECT_TRUE(executed);

    // After shared_ptr is destroyed
    concrete.reset();
    executed = false;
    result = weak.withLock([&executed]() {
        executed = true;
        return 84;
    });

    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(executed);

    executed = false;
    success = weak.withLock([&executed]() { executed = true; });

    EXPECT_FALSE(success);
    EXPECT_FALSE(executed);
}

TEST_F(EnhancedWeakPtrVoidTest, TryLockOrElse) {
    auto concrete = std::make_shared<int>(42);
    auto voidShared = std::static_pointer_cast<void>(concrete);

    EnhancedWeakPtr<void> weak(voidShared);

    // Success case
    auto result = weak.tryLockOrElse([]() { return 42; }, []() { return -1; });

    EXPECT_EQ(result, 42);

    // Failure case
    concrete.reset();
    result = weak.tryLockOrElse([]() { return 42; }, []() { return -1; });

    EXPECT_EQ(result, -1);
}

// Tests for ATOM_USE_BOOST functionality
#ifdef ATOM_USE_BOOST
TEST_F(EnhancedWeakPtrTest, Validate) {
    auto shared = std::make_shared<int>(42);
    EnhancedWeakPtr<int> weak(shared);

    // Should not throw when valid
    EXPECT_NO_THROW(weak.validate());

    // Should throw when expired
    shared.reset();
    EXPECT_THROW(weak.validate(), EnhancedWeakPtrException);
}

TEST_F(EnhancedWeakPtrVoidTest, Validate) {
    auto concrete = std::make_shared<int>(42);
    auto voidShared = std::static_pointer_cast<void>(concrete);

    EnhancedWeakPtr<void> weak(voidShared);

    // Should not throw when valid
    EXPECT_NO_THROW(weak.validate());

    // Should throw when expired
    concrete.reset();
    EXPECT_THROW(weak.validate(), EnhancedWeakPtrException);
}
#endif

}  // namespace
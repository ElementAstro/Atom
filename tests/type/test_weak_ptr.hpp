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
    auto derivedWeak = baseWeak.staticCast<Derived>();
    auto derivedShared = derivedWeak.lock();

    EXPECT_TRUE(derivedShared);
    EXPECT_EQ(derivedShared->base_value, 42);
    EXPECT_EQ(derivedShared->derived_value, 84);

    // Expired pointer should still cast but result in expired pointer
    shared.reset();
    auto derivedWeak2 = baseWeak.staticCast<Derived>();
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
TEST_F(EnhancedWeakPtrTest, Map) {
    auto shared = std::make_shared<int>(42);
    EnhancedWeakPtr<int> weak(shared);

    // Test map functionality with valid pointer
    auto doubled = weak.map([](const int& v) { return v * 2; });
    EXPECT_TRUE(doubled.has_value());
    EXPECT_EQ(*doubled, 84);

    // Test with transformations
    auto asString = weak.map([](const int& v) { return std::to_string(v); });
    EXPECT_TRUE(asString.has_value());
    EXPECT_EQ(*asString, "42");

    // Test with expired pointer
    shared.reset();
    auto result = weak.map([](const int& v) { return v + 10; });
    EXPECT_FALSE(result.has_value());
}

TEST_F(EnhancedWeakPtrTest, Filter) {
    auto shared = std::make_shared<int>(42);
    EnhancedWeakPtr<int> weak(shared);

    // Test filter with condition that passes
    auto filtered = weak.filter([](const int& v) { return v > 40; });
    EXPECT_FALSE(filtered.expired());

    // Test filter with condition that fails
    filtered = weak.filter([](const int& v) { return v > 100; });
    EXPECT_TRUE(filtered.expired());

    // Test with expired pointer
    shared.reset();
    filtered = weak.filter([](const int& v) { return v > 0; });
    EXPECT_TRUE(filtered.expired());
}

TEST_F(EnhancedWeakPtrTest, TypeChecking) {
    struct Base {
        virtual ~Base() = default;
    };
    struct Derived : public Base {
        int value = 42;
    };
    struct Unrelated {};

    auto derived = std::make_shared<Derived>();
    auto baseWeak = EnhancedWeakPtr<Base>(derived);

    // Test isType with correct type
    EXPECT_TRUE(baseWeak.isType<Derived>());

    // Test isType with base type
    EXPECT_TRUE(baseWeak.isType<Base>());

    // Test isType with unrelated type
    EXPECT_FALSE(baseWeak.isType<Unrelated>());

    // Test with expired pointer
    derived.reset();
    EXPECT_FALSE(baseWeak.isType<Derived>());
}

TEST_F(EnhancedWeakPtrTest, DynamicCast) {
    struct Base {
        virtual ~Base() = default;
    };
    struct Derived : public Base {
        int value = 42;
    };
    struct OtherDerived : public Base {
        double value = 3.14;
    };

    auto derived = std::make_shared<Derived>();
    EnhancedWeakPtr<Base> baseWeak(derived);

    // Test successful cast
    auto derivedWeak = baseWeak.dynamicCast<Derived>();
    EXPECT_FALSE(derivedWeak.expired());
    auto locked = derivedWeak.lock();
    EXPECT_EQ(locked->value, 42);

    // Test failed cast
    auto otherDerivedWeak = baseWeak.dynamicCast<OtherDerived>();
    EXPECT_TRUE(otherDerivedWeak.expired());

    // Test with expired pointer
    derived.reset();
    derivedWeak = baseWeak.dynamicCast<Derived>();
    EXPECT_TRUE(derivedWeak.expired());
}

TEST_F(EnhancedWeakPtrTest, StaticCast) {
    struct Base {
        virtual ~Base() = default;
    };
    struct Derived : public Base {
        int value = 42;
    };

    std::shared_ptr<Base> base = std::make_shared<Derived>();
    EnhancedWeakPtr<Base> baseWeak(base);

    // Test static cast
    auto derivedWeak = baseWeak.staticCast<Derived>();
    EXPECT_FALSE(derivedWeak.expired());
    auto locked = derivedWeak.lock();
    EXPECT_EQ(locked->value, 42);

    // Test with expired pointer
    base.reset();
    derivedWeak = baseWeak.staticCast<Derived>();
    EXPECT_TRUE(derivedWeak.expired());
}

TEST_F(EnhancedWeakPtrTest, FilterWeakPtrs) {
    std::vector<std::shared_ptr<int>> sharedPtrs = {
        std::make_shared<int>(1), std::make_shared<int>(10),
        std::make_shared<int>(20), std::make_shared<int>(30)};

    auto weakPtrs = createWeakPtrGroup(sharedPtrs);

    // Test filtering with predicate
    auto filtered =
        filterWeakPtrs(weakPtrs, [](const int& v) { return v > 15; });
    EXPECT_EQ(filtered.size(), 2);

    // Verify contents of filtered pointers
    EXPECT_EQ(*filtered[0].lock(), 20);
    EXPECT_EQ(*filtered[1].lock(), 30);

    // Test with some expired pointers
    sharedPtrs[1].reset();
    sharedPtrs[2].reset();

    filtered = filterWeakPtrs(weakPtrs, [](const int& v) { return v > 0; });
    EXPECT_EQ(filtered.size(),
              2);  // Only the non-expired ones that match predicate
}

TEST_F(EnhancedWeakPtrTest, RetryPolicy) {
    // Test exponential backoff policy
    auto policy = RetryPolicy::exponentialBackoff(
        3, std::chrono::milliseconds(5), std::chrono::milliseconds(100));

    EXPECT_EQ(policy.maxAttempts(), 3);
    EXPECT_EQ(policy.interval(), std::chrono::milliseconds(5));
    EXPECT_EQ(policy.maxDuration(), std::chrono::milliseconds(100));

    // Test none policy
    auto nonePolicy = RetryPolicy::none();
    EXPECT_EQ(nonePolicy.maxAttempts(), 1);
    EXPECT_EQ(nonePolicy.interval(),
              std::chrono::steady_clock::duration::zero());
    EXPECT_EQ(nonePolicy.maxDuration(),
              std::chrono::steady_clock::duration::zero());

    // Test builder pattern
    auto customPolicy = RetryPolicy()
                            .withMaxAttempts(5)
                            .withInterval(std::chrono::milliseconds(10))
                            .withMaxDuration(std::chrono::seconds(2));

    EXPECT_EQ(customPolicy.maxAttempts(), 5);
    EXPECT_EQ(customPolicy.interval(), std::chrono::milliseconds(10));
    EXPECT_EQ(customPolicy.maxDuration(), std::chrono::seconds(2));
}

TEST_F(EnhancedWeakPtrTest, NotifyAll) {
    auto shared = std::make_shared<int>(42);
    EnhancedWeakPtr<int> weak;

    std::atomic<bool> threadStarted{false};
    std::atomic<bool> threadFinished{false};

    // Start a thread that waits on the condition variable
    std::thread t([&]() {
        threadStarted = true;

        // Wait with timeout to avoid hanging if notification doesn't work
        bool result = weak.waitFor(std::chrono::milliseconds(500));

        // Should succeed because notifyAll is called
        EXPECT_TRUE(result);
        EXPECT_FALSE(weak.expired());
        threadFinished = true;
    });

    // Wait for thread to start and begin waiting
    while (!threadStarted) {
        std::this_thread::yield();
    }

    // Short sleep to ensure thread is in wait state
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Assign value and notify
    weak = EnhancedWeakPtr<int>(shared);
    weak.notifyAll();

    t.join();
    EXPECT_TRUE(threadFinished);
}

#if HAS_EXPECTED
TEST_F(EnhancedWeakPtrTest, LockExpected) {
    auto shared = std::make_shared<int>(42);
    EnhancedWeakPtr<int> weak(shared);

    // Test with valid pointer
    auto expected = weak.lockExpected();
    EXPECT_TRUE(expected.has_value());
    EXPECT_EQ(*expected.value(), 42);

    // Test with expired pointer
    shared.reset();
    expected = weak.lockExpected();
    EXPECT_FALSE(expected.has_value());
    EXPECT_EQ(expected.error().type(), WeakPtrErrorType::Expired);
    EXPECT_FALSE(expected.error().message().empty());
}
#endif

TEST_F(EnhancedWeakPtrVoidTest, DynamicCastAndStatic) {
    struct Base {
        virtual ~Base() = default;
        int value = 42;
    };

    auto base = std::make_shared<Base>();
    auto voidShared = std::static_pointer_cast<void>(base);

    EnhancedWeakPtr<void> weak(voidShared);

    // Test cast to concrete type
    auto baseWeak = weak.cast<Base>();
    EXPECT_FALSE(baseWeak.expired());

    auto locked = baseWeak.lock();
    EXPECT_EQ(locked->value, 42);

    // Test with expired pointer
    base.reset();
    voidShared.reset();

    baseWeak = weak.cast<Base>();
    EXPECT_TRUE(baseWeak.expired());
}

TEST_F(EnhancedWeakPtrVoidTest, WaitUntil) {
    auto concrete = std::make_shared<int>(42);
    auto voidShared = std::static_pointer_cast<void>(concrete);

    EnhancedWeakPtr<void> weak(voidShared);

    std::atomic<bool> flag{false};

    std::thread t([&flag]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        flag = true;
    });

    // Test waiting for predicate
    bool result = weak.waitUntil([&flag]() { return flag.load(); });
    EXPECT_TRUE(result);

    // Test with already satisfied predicate
    result = weak.waitUntil([]() { return true; });
    EXPECT_TRUE(result);

    // Test with expired pointer
    concrete.reset();
    voidShared.reset();

    flag = false;
    result = weak.waitUntil([&flag]() { return flag.load(); });
    EXPECT_FALSE(result);

    t.join();
}

TEST_F(EnhancedWeakPtrTest, StatCounters) {
    size_t initialTotalLockAttempts =
        EnhancedWeakPtr<int>::getTotalSuccessfulLocks() +
        EnhancedWeakPtr<int>::getTotalFailedLocks();

    auto shared = std::make_shared<int>(42);
    EnhancedWeakPtr<int> weak(shared);

    // Make some successful locks
    for (int i = 0; i < 5; i++) {
        weak.lock();
    }

    // Verify successful lock count increased
    EXPECT_EQ(EnhancedWeakPtr<int>::getTotalSuccessfulLocks() +
                  EnhancedWeakPtr<int>::getTotalFailedLocks(),
              initialTotalLockAttempts + 5);

    // Reset the pointer and make failed lock attempts
    shared.reset();
    for (int i = 0; i < 3; i++) {
        weak.lock();
    }

    // Verify failed lock count increased
    EXPECT_EQ(EnhancedWeakPtr<int>::getTotalSuccessfulLocks() +
                  EnhancedWeakPtr<int>::getTotalFailedLocks(),
              initialTotalLockAttempts + 8);

    // Test reset stats
    EnhancedWeakPtr<int>::resetStats();
    EXPECT_EQ(EnhancedWeakPtr<int>::getTotalSuccessfulLocks(), 0);
    EXPECT_EQ(EnhancedWeakPtr<int>::getTotalFailedLocks(), 0);
    EXPECT_GT(EnhancedWeakPtr<int>::getTotalInstances(),
              0);  // Instances not reset
}

// Test for edge cases
TEST_F(EnhancedWeakPtrTest, EdgeCases) {
    // Test locking expired pointer with retry that has zero duration
    EnhancedWeakPtr<int> weak;
    auto result = weak.tryLockWithRetry(RetryPolicy::none());
    EXPECT_EQ(result, nullptr);

    // Test with large pointer
    struct LargeObject {
        std::array<char, 1024 * 1024> data;  // 1MB object
        int value = 42;
    };

    auto large = std::make_shared<LargeObject>();
    EnhancedWeakPtr<LargeObject> largeWeak(large);

    // Lock and access the large object
    auto locked = largeWeak.lock();
    EXPECT_EQ(locked->value, 42);

    // Test self-reference cycle
    struct Node {
        EnhancedWeakPtr<Node> weakSelf;
        int value = 0;
    };

    auto node = std::make_shared<Node>();
    node->weakSelf = EnhancedWeakPtr<Node>(node);

    // Verify self-reference works
    auto self = node->weakSelf.lock();
    EXPECT_EQ(self, node);

    // Break cycle and test
    node.reset();
    EXPECT_TRUE(self->weakSelf.expired());
}
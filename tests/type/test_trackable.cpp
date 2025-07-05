#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>

#include "atom/type/trackable.hpp"

class TrackableTest : public ::testing::Test {
protected:
    Trackable<int> intTrackable{42};
    Trackable<std::string> stringTrackable{"test"};
};

// Basic Construction and Value Access Tests
TEST_F(TrackableTest, Construction) {
    EXPECT_EQ(intTrackable.get(), 42);
    EXPECT_EQ(stringTrackable.get(), "test");
}

TEST_F(TrackableTest, GetTypeName) {
    std::string intType = intTrackable.getTypeName();
    std::string stringType = stringTrackable.getTypeName();

    EXPECT_TRUE(intType.find("int") != std::string::npos);
    EXPECT_TRUE(stringType.find("string") != std::string::npos);
}

// Observer Subscription Tests
TEST_F(TrackableTest, SubscribeAndNotify) {
    int oldValue = 0;
    int newValue = 0;

    intTrackable.subscribe([&](const int& old, const int& newVal) {
        oldValue = old;
        newValue = newVal;
    });

    intTrackable = 100;

    EXPECT_EQ(oldValue, 42);
    EXPECT_EQ(newValue, 100);
    EXPECT_EQ(intTrackable.get(), 100);
}

TEST_F(TrackableTest, MultipleObservers) {
    int observer1Called = 0;
    int observer2Called = 0;

    intTrackable.subscribe([&](const int&, const int&) { observer1Called++; });
    intTrackable.subscribe([&](const int&, const int&) { observer2Called++; });

    intTrackable = 100;

    EXPECT_EQ(observer1Called, 1);
    EXPECT_EQ(observer2Called, 1);
    EXPECT_TRUE(intTrackable.hasSubscribers());
}

TEST_F(TrackableTest, UnsubscribeAll) {
    int observer1Called = 0;

    intTrackable.subscribe([&](const int&, const int&) { observer1Called++; });

    EXPECT_TRUE(intTrackable.hasSubscribers());

    intTrackable.unsubscribeAll();
    EXPECT_FALSE(intTrackable.hasSubscribers());

    intTrackable = 100;
    EXPECT_EQ(observer1Called,
              0);  // Observer should not be called after unsubscribe
}

TEST_F(TrackableTest, OnChangeCallback) {
    int callbackCalled = 0;
    int callbackValue = 0;

    intTrackable.setOnChangeCallback([&](const int& newVal) {
        callbackCalled++;
        callbackValue = newVal;
    });

    intTrackable = 100;

    EXPECT_EQ(callbackCalled, 1);
    EXPECT_EQ(callbackValue, 100);
}

// Value Change Tests
TEST_F(TrackableTest, Assignment) {
    intTrackable = 100;
    EXPECT_EQ(intTrackable.get(), 100);

    // Assigning the same value should not trigger notifications
    int observer1Called = 0;
    intTrackable.subscribe([&](const int&, const int&) { observer1Called++; });

    intTrackable = 100;  // Same value
    EXPECT_EQ(observer1Called, 0);

    intTrackable = 200;  // Different value
    EXPECT_EQ(observer1Called, 1);
}

TEST_F(TrackableTest, ArithmeticOperations) {
    intTrackable = 10;

    int observer1Called = 0;
    intTrackable.subscribe([&](const int&, const int&) { observer1Called++; });

    intTrackable += 5;
    EXPECT_EQ(intTrackable.get(), 15);
    EXPECT_EQ(observer1Called, 1);

    intTrackable -= 3;
    EXPECT_EQ(intTrackable.get(), 12);
    EXPECT_EQ(observer1Called, 2);

    intTrackable *= 2;
    EXPECT_EQ(intTrackable.get(), 24);
    EXPECT_EQ(observer1Called, 3);

    intTrackable /= 3;
    EXPECT_EQ(intTrackable.get(), 8);
    EXPECT_EQ(observer1Called, 4);
}

TEST_F(TrackableTest, ConversionOperator) {
    int value = static_cast<int>(intTrackable);
    std::string strValue = static_cast<std::string>(stringTrackable);

    EXPECT_EQ(value, 42);
    EXPECT_EQ(strValue, "test");
}

// Exception Handling Tests
TEST_F(TrackableTest, ExceptionInObserver) {
    intTrackable.subscribe([](const int&, const int&) {
        throw std::runtime_error("Test exception");
    });

    // The exception in the observer should be caught and wrapped
    EXPECT_THROW(intTrackable = 100, std::exception);
}

// Deferred Notifications Tests
TEST_F(TrackableTest, DeferNotifications) {
    int observer1Called = 0;

    intTrackable.subscribe([&](const int&, const int&) { observer1Called++; });

    intTrackable.deferNotifications(true);

    intTrackable = 100;
    EXPECT_EQ(observer1Called, 0);  // Notifications are deferred

    intTrackable = 200;
    EXPECT_EQ(observer1Called, 0);  // Still deferred

    intTrackable.deferNotifications(false);
    EXPECT_EQ(observer1Called, 1);  // Now observers should be notified once
}

TEST_F(TrackableTest, DeferScoped) {
    int observer1Called = 0;

    intTrackable.subscribe([&](const int&, const int&) { observer1Called++; });

    {
        auto defer = intTrackable.deferScoped();

        intTrackable = 100;
        EXPECT_EQ(observer1Called, 0);  // Notifications are deferred

        intTrackable = 200;
        EXPECT_EQ(observer1Called, 0);  // Still deferred

        // defer will go out of scope here
    }

    EXPECT_EQ(observer1Called, 1);  // Now observers should be notified once
    EXPECT_EQ(intTrackable.get(), 200);
}

// Thread Safety Tests
TEST_F(TrackableTest, ThreadSafety) {
    const int numThreads = 10;
    const int operationsPerThread = 100;

    std::atomic<int> notificationCount{0};

    Trackable<int> threadSafeTrackable{0};
    threadSafeTrackable.subscribe(
        [&](const int&, const int& newVal) { notificationCount++; });

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < operationsPerThread; ++j) {
                if (i % 2 == 0) {
                    // Writer thread
                    threadSafeTrackable = j;
                } else {
                    // Reader thread
                    int val = threadSafeTrackable.get();
                    (void)val;  // Suppress unused variable warning
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // We can't predict exactly how many notifications will occur due to
    // the non-deterministic nature of thread execution and the optimization
    // that avoids notifications when the value doesn't change
    EXPECT_GT(notificationCount, 0);
}

// Complex Type Tests
struct ComplexType {
    int id;
    std::string name;

    bool operator==(const ComplexType& other) const {
        return id == other.id && name == other.name;
    }

    bool operator!=(const ComplexType& other) const {
        return !(*this == other);
    }
};

TEST_F(TrackableTest, ComplexTypeTracking) {
    Trackable<ComplexType> complexTrackable{{1, "original"}};

    int observer1Called = 0;
    ComplexType oldComplex;
    ComplexType newComplex;

    complexTrackable.subscribe(
        [&](const ComplexType& old, const ComplexType& newVal) {
            observer1Called++;
            oldComplex = old;
            newComplex = newVal;
        });

    complexTrackable = ComplexType{2, "updated"};

    EXPECT_EQ(observer1Called, 1);
    EXPECT_EQ(oldComplex.id, 1);
    EXPECT_EQ(oldComplex.name, "original");
    EXPECT_EQ(newComplex.id, 2);
    EXPECT_EQ(newComplex.name, "updated");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

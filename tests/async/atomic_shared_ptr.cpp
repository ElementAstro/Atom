#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

#include "atom/async/atomic_shared_ptr.hpp"

using namespace lithium::task::concurrency;

// A simple class to test with
class MyObject {
public:
    int id;
    static std::atomic<int> instance_count;

    MyObject(int i = 0) : id(i) { instance_count++; }
    ~MyObject() { instance_count--; }
};
std::atomic<int> MyObject::instance_count = 0;

// Test fixture for AtomicSharedPtr
class AtomicSharedPtrTest : public ::testing::Test {
protected:
    void SetUp() override {
        MyObject::instance_count = 0;  // Reset instance count before each test
    }

    void TearDown() override {
        // Ensure all MyObject instances are destroyed
        EXPECT_EQ(MyObject::instance_count, 0);
    }
};

// Test default constructor
TEST_F(AtomicSharedPtrTest, DefaultConstructor) {
    AtomicSharedPtr<MyObject> ptr;
    EXPECT_TRUE(ptr.is_null());
    EXPECT_EQ(ptr.use_count(), 0);
    EXPECT_FALSE(ptr);
}

// Test constructor with config
TEST_F(AtomicSharedPtrTest, ConstructorWithConfig) {
    AtomicSharedPtrConfig config;
    config.enable_statistics = true;
    AtomicSharedPtr<MyObject> ptr(config);
    EXPECT_TRUE(ptr.is_null());
    EXPECT_TRUE(ptr.get_stats() != nullptr);
    EXPECT_TRUE(ptr.get_config().enable_statistics);
}

// Test constructor with std::shared_ptr
TEST_F(AtomicSharedPtrTest, ConstructorFromSharedPtr) {
    auto shared = std::make_shared<MyObject>(1);
    AtomicSharedPtr<MyObject> ptr(shared);
    EXPECT_FALSE(ptr.is_null());
    EXPECT_EQ(ptr.load()->id, 1);
    EXPECT_EQ(ptr.use_count(), 1);  // Only the AtomicSharedPtr holds a ref
    EXPECT_EQ(MyObject::instance_count, 1);
}

// Test constructor with variadic arguments (make_unique style)
TEST_F(AtomicSharedPtrTest, ConstructorWithArgs) {
    AtomicSharedPtr<MyObject> ptr(2);
    EXPECT_FALSE(ptr.is_null());
    EXPECT_EQ(ptr.load()->id, 2);
    EXPECT_EQ(ptr.use_count(), 1);
    EXPECT_EQ(MyObject::instance_count, 1);
}

// Test destructor
TEST_F(AtomicSharedPtrTest, Destructor) {
    {
        AtomicSharedPtr<MyObject> ptr(3);
        EXPECT_EQ(MyObject::instance_count, 1);
    }
    EXPECT_EQ(MyObject::instance_count, 0);

    {
        auto shared = std::make_shared<MyObject>(4);
        AtomicSharedPtr<MyObject> ptr(shared);
        EXPECT_EQ(MyObject::instance_count, 1);
    }
    EXPECT_EQ(MyObject::instance_count,
              0);  // shared_ptr should release its ref
}

// Test copy constructor
TEST_F(AtomicSharedPtrTest, CopyConstructor) {
    AtomicSharedPtr<MyObject> original(5);
    AtomicSharedPtr<MyObject> copy = original;

    EXPECT_FALSE(original.is_null());
    EXPECT_FALSE(copy.is_null());
    EXPECT_EQ(original.load()->id, 5);
    EXPECT_EQ(copy.load()->id, 5);
    EXPECT_EQ(original.use_count(), 2);
    EXPECT_EQ(copy.use_count(), 2);
    EXPECT_EQ(MyObject::instance_count, 1);
}

// Test copy assignment
TEST_F(AtomicSharedPtrTest, CopyAssignment) {
    AtomicSharedPtr<MyObject> original(6);
    AtomicSharedPtr<MyObject> assigned;
    assigned = original;

    EXPECT_FALSE(original.is_null());
    EXPECT_FALSE(assigned.is_null());
    EXPECT_EQ(original.load()->id, 6);
    EXPECT_EQ(assigned.load()->id, 6);
    EXPECT_EQ(original.use_count(), 2);
    EXPECT_EQ(assigned.use_count(), 2);
    EXPECT_EQ(MyObject::instance_count, 1);

    AtomicSharedPtr<MyObject> another(7);
    assigned = another;                  // Assign new value
    EXPECT_EQ(original.use_count(), 1);  // Original should now have 1 ref
    EXPECT_EQ(another.use_count(), 2);
    EXPECT_EQ(assigned.use_count(), 2);
    EXPECT_EQ(MyObject::instance_count, 2);  // Original + another
}

// Test move constructor
TEST_F(AtomicSharedPtrTest, MoveConstructor) {
    AtomicSharedPtr<MyObject> original(8);
    MyObject* raw_ptr = original.get_raw_unsafe();
    AtomicSharedPtr<MyObject> moved = std::move(original);

    EXPECT_TRUE(original.is_null());  // Original should be null after move
    EXPECT_FALSE(moved.is_null());
    EXPECT_EQ(moved.get_raw_unsafe(), raw_ptr);
    EXPECT_EQ(moved.load()->id, 8);
    EXPECT_EQ(moved.use_count(), 1);
    EXPECT_EQ(MyObject::instance_count, 1);
}

// Test move assignment
TEST_F(AtomicSharedPtrTest, MoveAssignment) {
    AtomicSharedPtr<MyObject> original(9);
    MyObject* raw_ptr = original.get_raw_unsafe();
    AtomicSharedPtr<MyObject> assigned;
    assigned = std::move(original);

    EXPECT_TRUE(original.is_null());  // Original should be null after move
    EXPECT_FALSE(assigned.is_null());
    EXPECT_EQ(assigned.get_raw_unsafe(), raw_ptr);
    EXPECT_EQ(assigned.load()->id, 9);
    EXPECT_EQ(assigned.use_count(), 1);
    EXPECT_EQ(MyObject::instance_count, 1);

    AtomicSharedPtr<MyObject> another(10);
    assigned = std::move(another);  // Assign new value
    EXPECT_EQ(MyObject::instance_count,
              1);  // Old assigned (id 9) destroyed, new assigned (id 10)
    EXPECT_TRUE(another.is_null());
    EXPECT_EQ(assigned.load()->id, 10);
}

// Test load operation
TEST_F(AtomicSharedPtrTest, Load) {
    AtomicSharedPtr<MyObject> ptr(11);
    std::shared_ptr<MyObject> loaded_ptr = ptr.load();
    EXPECT_FALSE(loaded_ptr == nullptr);
    EXPECT_EQ(loaded_ptr->id, 11);
    EXPECT_EQ(ptr.use_count(), 2);  // AtomicSharedPtr + loaded_ptr
    EXPECT_EQ(MyObject::instance_count, 1);
}

// Test store operation
TEST_F(AtomicSharedPtrTest, Store) {
    AtomicSharedPtr<MyObject> ptr(12);
    EXPECT_EQ(MyObject::instance_count, 1);

    ptr.store(std::make_shared<MyObject>(13));
    EXPECT_EQ(MyObject::instance_count,
              1);  // Old object destroyed, new one created
    EXPECT_EQ(ptr.load()->id, 13);
    EXPECT_EQ(ptr.use_count(), 1);

    ptr.store(nullptr);
    EXPECT_TRUE(ptr.is_null());
    EXPECT_EQ(MyObject::instance_count, 0);
}

// Test exchange operation
TEST_F(AtomicSharedPtrTest, Exchange) {
    AtomicSharedPtr<MyObject> ptr(14);
    EXPECT_EQ(MyObject::instance_count, 1);

    auto old_shared = ptr.exchange(std::make_shared<MyObject>(15));
    EXPECT_EQ(old_shared->id, 14);
    EXPECT_EQ(ptr.load()->id, 15);
    EXPECT_EQ(MyObject::instance_count,
              2);  // Old object still held by old_shared, new one by ptr

    old_shared.reset();
    EXPECT_EQ(MyObject::instance_count, 1);  // Old object destroyed

    auto null_shared = ptr.exchange(nullptr);
    EXPECT_EQ(null_shared->id, 15);
    EXPECT_TRUE(ptr.is_null());
    EXPECT_EQ(MyObject::instance_count, 1);  // null_shared still holds the ref

    null_shared.reset();
    EXPECT_EQ(MyObject::instance_count, 0);
}

// Test compare_exchange_weak
TEST_F(AtomicSharedPtrTest, CompareExchangeWeak) {
    AtomicSharedPtr<MyObject> ptr(16);
    std::shared_ptr<MyObject> expected = ptr.load();  // expected points to 16
    std::shared_ptr<MyObject> desired = std::make_shared<MyObject>(17);

    // Successful CAS
    bool success = ptr.compare_exchange_weak(expected, desired);
    EXPECT_TRUE(success);
    EXPECT_EQ(ptr.load()->id, 17);
    EXPECT_EQ(MyObject::instance_count, 2);  // 16 (expected) + 17 (ptr)

    // Failed CAS (expected is now 16, but ptr is 17)
    std::shared_ptr<MyObject> new_desired = std::make_shared<MyObject>(18);
    success = ptr.compare_exchange_weak(expected, new_desired);
    EXPECT_FALSE(success);
    EXPECT_EQ(expected->id, 17);  // expected is updated to current value of ptr
    EXPECT_EQ(ptr.load()->id, 17);  // ptr remains 17
    EXPECT_EQ(
        MyObject::instance_count,
        3);  // 16 (old expected) + 17 (ptr, new expected) + 18 (new_desired)
}

// Test compare_exchange_strong
TEST_F(AtomicSharedPtrTest, CompareExchangeStrong) {
    AtomicSharedPtr<MyObject> ptr(19);
    std::shared_ptr<MyObject> expected = ptr.load();  // expected points to 19
    std::shared_ptr<MyObject> desired = std::make_shared<MyObject>(20);

    // Successful CAS
    bool success = ptr.compare_exchange_strong(expected, desired);
    EXPECT_TRUE(success);
    EXPECT_EQ(ptr.load()->id, 20);
    EXPECT_EQ(MyObject::instance_count, 2);  // 19 (expected) + 20 (ptr)

    // Failed CAS (expected is now 19, but ptr is 20)
    std::shared_ptr<MyObject> new_desired = std::make_shared<MyObject>(21);
    success = ptr.compare_exchange_strong(expected, new_desired);
    EXPECT_FALSE(success);
    EXPECT_EQ(expected->id, 20);  // expected is updated to current value of ptr
    EXPECT_EQ(ptr.load()->id, 20);  // ptr remains 20
    EXPECT_EQ(
        MyObject::instance_count,
        3);  // 19 (old expected) + 20 (ptr, new expected) + 21 (new_desired)
}

// Test compare_exchange_with_retry
TEST_F(AtomicSharedPtrTest, CompareExchangeWithRetry) {
    AtomicSharedPtrConfig config;
    config.max_retry_attempts = 5;
    config.retry_delay = std::chrono::nanoseconds(1);  // Minimal delay
    AtomicSharedPtr<MyObject> ptr(std::make_shared<MyObject>(22), config);

    std::shared_ptr<MyObject> expected = ptr.load();
    std::shared_ptr<MyObject> desired = std::make_shared<MyObject>(23);

    // Simulate a concurrent modification that fails the first few attempts
    std::thread t([&]() {
        // This thread will change the value a few times
        for (int i = 0; i < 3; ++i) {
            std::shared_ptr<MyObject> current = ptr.load();
            std::shared_ptr<MyObject> next =
                std::make_shared<MyObject>(current->id + 100);
            ptr.compare_exchange_strong(current, next);
            std::this_thread::sleep_for(std::chrono::nanoseconds(5));
        }
    });

    // The main thread tries to CAS, it should eventually succeed after retries
    bool success = ptr.compare_exchange_with_retry(expected, desired);
    t.join();

    EXPECT_TRUE(success);
    EXPECT_EQ(ptr.load()->id, 23);
    // Instance count will be higher due to intermediate objects created by the
    // thread
}

// Test conditional_store
TEST_F(AtomicSharedPtrTest, ConditionalStore) {
    AtomicSharedPtr<MyObject> ptr(24);

    // Condition true: current value is 24, new value 25
    bool stored = ptr.conditional_store(
        std::make_shared<MyObject>(25),
        [](const std::shared_ptr<MyObject>& p) { return p && p->id == 24; });
    EXPECT_TRUE(stored);
    EXPECT_EQ(ptr.load()->id, 25);
    EXPECT_EQ(MyObject::instance_count, 1);

    // Condition false: current value is 25, condition checks for 24
    stored = ptr.conditional_store(
        std::make_shared<MyObject>(26),
        [](const std::shared_ptr<MyObject>& p) { return p && p->id == 24; });
    EXPECT_FALSE(stored);
    EXPECT_EQ(ptr.load()->id, 25);           // Value should not have changed
    EXPECT_EQ(MyObject::instance_count, 2);  // 25 (ptr) + 26 (new_value)
}

// Test transform
TEST_F(AtomicSharedPtrTest, Transform) {
    AtomicSharedPtr<MyObject> ptr(27);

    auto transformed_ptr =
        ptr.transform([](const std::shared_ptr<MyObject>& p) {
            return std::make_shared<MyObject>(p ? p->id + 1 : 100);
        });

    EXPECT_EQ(transformed_ptr->id, 28);
    EXPECT_EQ(ptr.load()->id, 28);
    EXPECT_EQ(MyObject::instance_count, 1);

    // Test transform with concurrent modification
    AtomicSharedPtr<MyObject> concurrent_ptr(
        std::make_shared<MyObject>(1),
        AtomicSharedPtrConfig{.max_retry_attempts = 100});
    std::atomic<int> final_value = 0;

    auto increment_func = [&](int thread_id) {
        concurrent_ptr.transform([&](const std::shared_ptr<MyObject>& p) {
            // Simulate some work
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            return std::make_shared<MyObject>(p->id + 1);
        });
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(increment_func, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(concurrent_ptr.load()->id, 11);  // Initial 1 + 10 increments
    EXPECT_EQ(MyObject::instance_count,
              1);  // Only the final object should remain
}

// Test update
TEST_F(AtomicSharedPtrTest, Update) {
    AtomicSharedPtr<MyObject> ptr(29);

    auto updated_ptr = ptr.update([](const std::shared_ptr<MyObject>& p) {
        return std::make_shared<MyObject>(p ? p->id * 2 : 0);
    });

    EXPECT_EQ(updated_ptr->id, 58);
    EXPECT_EQ(ptr.load()->id, 58);
    EXPECT_EQ(MyObject::instance_count, 1);

    // Test update with concurrent modification
    AtomicSharedPtr<MyObject> concurrent_ptr(
        std::make_shared<MyObject>(1),
        AtomicSharedPtrConfig{.max_retry_attempts = 100});

    auto increment_func = [&](int thread_id) {
        concurrent_ptr.update([&](const std::shared_ptr<MyObject>& p) {
            // Simulate some work
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            return std::make_shared<MyObject>(p->id + 1);
        });
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back(increment_func, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(concurrent_ptr.load()->id, 11);  // Initial 1 + 10 increments
    EXPECT_EQ(MyObject::instance_count,
              1);  // Only the final object should remain
}

// Test wait_for
TEST_F(AtomicSharedPtrTest, WaitFor) {
    AtomicSharedPtr<MyObject> ptr(30);

    // Test immediate condition met
    auto result = ptr.wait_for(
        [](const std::shared_ptr<MyObject>& p) { return p && p->id == 30; },
        std::chrono::milliseconds(100));
    EXPECT_EQ(result->id, 30);

    // Test condition met after some delay
    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ptr.store(std::make_shared<MyObject>(31));
    });

    result = ptr.wait_for(
        [](const std::shared_ptr<MyObject>& p) { return p && p->id == 31; },
        std::chrono::milliseconds(200));
    EXPECT_EQ(result->id, 31);
    t.join();

    // Test timeout
    EXPECT_THROW(
        {
            ptr.wait_for(
                [](const std::shared_ptr<MyObject>& p) {
                    return p && p->id == 999;  // Never true
                },
                std::chrono::milliseconds(10));
        },
        AtomicSharedPtrException);
}

// Test with_exclusive_access
TEST_F(AtomicSharedPtrTest, WithExclusiveAccess) {
    AtomicSharedPtr<MyObject> ptr(32);

    // Successful exclusive access
    int new_id = ptr.with_exclusive_access([](MyObject* obj) {
        obj->id = 33;
        return obj->id;
    });
    EXPECT_EQ(new_id, 33);
    EXPECT_EQ(ptr.load()->id, 33);

    // Test with multiple references (should throw)
    std::shared_ptr<MyObject> extra_ref = ptr.load();
    EXPECT_THROW(
        {
            ptr.with_exclusive_access([](MyObject* obj) {
                obj->id = 34;
                return obj->id;
            });
        },
        AtomicSharedPtrException);
    extra_ref.reset();  // Release the extra reference

    // Test with null pointer (should throw)
    AtomicSharedPtr<MyObject> null_ptr;
    EXPECT_THROW(
        { null_ptr.with_exclusive_access([](MyObject* obj) { return 0; }); },
        AtomicSharedPtrException);
}

// Test is_null
TEST_F(AtomicSharedPtrTest, IsNull) {
    AtomicSharedPtr<MyObject> ptr;
    EXPECT_TRUE(ptr.is_null());
    ptr.store(std::make_shared<MyObject>(35));
    EXPECT_FALSE(ptr.is_null());
    ptr.reset();
    EXPECT_TRUE(ptr.is_null());
}

// Test use_count
TEST_F(AtomicSharedPtrTest, UseCount) {
    AtomicSharedPtr<MyObject> ptr;
    EXPECT_EQ(ptr.use_count(), 0);

    ptr.store(std::make_shared<MyObject>(36));
    EXPECT_EQ(ptr.use_count(), 1);

    std::shared_ptr<MyObject> loaded = ptr.load();
    EXPECT_EQ(ptr.use_count(), 2);

    AtomicSharedPtr<MyObject> copy = ptr;
    EXPECT_EQ(ptr.use_count(), 3);

    loaded.reset();
    EXPECT_EQ(ptr.use_count(), 2);

    copy.reset();
    EXPECT_EQ(ptr.use_count(), 1);

    ptr.reset();
    EXPECT_EQ(ptr.use_count(), 0);
}

// Test unique
TEST_F(AtomicSharedPtrTest, Unique) {
    AtomicSharedPtr<MyObject> ptr(37);
    EXPECT_TRUE(ptr.unique());

    std::shared_ptr<MyObject> loaded = ptr.load();
    EXPECT_FALSE(ptr.unique());

    loaded.reset();
    EXPECT_TRUE(ptr.unique());
}

// Test version (ABA problem prevention)
TEST_F(AtomicSharedPtrTest, Version) {
    AtomicSharedPtr<MyObject> ptr(38);
    uint64_t initial_version = ptr.version();
    EXPECT_GT(initial_version, 0);  // Should be at least 1 after creation

    ptr.store(std::make_shared<MyObject>(39));
    uint64_t new_version = ptr.version();
    EXPECT_GT(new_version, initial_version);

    // Storing the same value should also increment version if a new control
    // block is created
    ptr.store(ptr.load());
    EXPECT_GT(ptr.version(), new_version);
}

// Test reset
TEST_F(AtomicSharedPtrTest, Reset) {
    AtomicSharedPtr<MyObject> ptr(40);
    EXPECT_FALSE(ptr.is_null());
    EXPECT_EQ(MyObject::instance_count, 1);

    ptr.reset();
    EXPECT_TRUE(ptr.is_null());
    EXPECT_EQ(MyObject::instance_count, 0);
}

// Test get_raw_unsafe
TEST_F(AtomicSharedPtrTest, GetRawUnsafe) {
    MyObject* obj = new MyObject(41);
    // Fix: Use direct initialization with curly braces or std::make_shared
    AtomicSharedPtr<MyObject> ptr{std::shared_ptr<MyObject>(obj)};
    EXPECT_EQ(ptr.get_raw_unsafe(), obj);
    EXPECT_EQ(ptr.get_raw_unsafe()->id, 41);
}

// Test statistics
TEST_F(AtomicSharedPtrTest, Statistics) {
    AtomicSharedPtrConfig config;
    config.enable_statistics = true;
    AtomicSharedPtr<MyObject> ptr(config);
    EXPECT_TRUE(ptr.get_stats() != nullptr);

    ptr.store(std::make_shared<MyObject>(42));
    ptr.load();
    ptr.load();
    std::shared_ptr<MyObject> expected = ptr.load();
    ptr.compare_exchange_strong(expected, std::make_shared<MyObject>(43));

    const AtomicSharedPtrStats* stats = ptr.get_stats();
    EXPECT_GE(stats->store_operations.load(), 1);
    EXPECT_GE(stats->load_operations.load(), 3);
    EXPECT_GE(stats->cas_operations.load(), 1);
    EXPECT_EQ(stats->cas_failures.load(),
              0);  // Should be 0 if CAS succeeded on first try

    ptr.reset_stats();
    EXPECT_EQ(stats->store_operations.load(), 0);
    EXPECT_EQ(stats->load_operations.load(), 0);
}

// Test set_config
TEST_F(AtomicSharedPtrTest, SetConfig) {
    AtomicSharedPtr<MyObject> ptr;
    EXPECT_FALSE(ptr.get_config().enable_statistics);
    EXPECT_TRUE(ptr.get_stats() == nullptr);

    AtomicSharedPtrConfig new_config;
    new_config.enable_statistics = true;
    ptr.set_config(new_config);
    EXPECT_TRUE(ptr.get_config().enable_statistics);
    EXPECT_TRUE(ptr.get_stats() != nullptr);

    new_config.enable_statistics = false;
    ptr.set_config(new_config);
    EXPECT_FALSE(ptr.get_config().enable_statistics);
    EXPECT_TRUE(ptr.get_stats() == nullptr);
}

// Test operator bool
TEST_F(AtomicSharedPtrTest, OperatorBool) {
    AtomicSharedPtr<MyObject> ptr;
    EXPECT_FALSE(ptr);
    ptr.store(std::make_shared<MyObject>(44));
    EXPECT_TRUE(ptr);
}

// Test operator->
TEST_F(AtomicSharedPtrTest, OperatorArrow) {
    AtomicSharedPtr<MyObject> ptr(45);
    EXPECT_EQ(ptr->id, 45);

    ptr.reset();
    // Fix: Wrap the expression in a lambda for EXPECT_THROW
    EXPECT_THROW({ (void)ptr->id; }, AtomicSharedPtrException);
}

// Test make_with_deleter
TEST_F(AtomicSharedPtrTest, MakeWithDeleter) {
    bool deleter_called = false;
    auto custom_deleter = [&](MyObject* obj) {
        deleter_called = true;
        delete obj;
    };

    {
        AtomicSharedPtr<MyObject> ptr =
            AtomicSharedPtr<MyObject>::make_with_deleter(new MyObject(46),
                                                         custom_deleter);
        EXPECT_FALSE(ptr.is_null());
        EXPECT_EQ(ptr.load()->id, 46);
        EXPECT_EQ(MyObject::instance_count, 1);
        EXPECT_FALSE(deleter_called);
    }
    EXPECT_TRUE(deleter_called);
    EXPECT_EQ(MyObject::instance_count, 0);

    EXPECT_THROW(
        AtomicSharedPtr<MyObject>::make_with_deleter(nullptr, custom_deleter),
        AtomicSharedPtrException);
}

// Test from_unique
TEST_F(AtomicSharedPtrTest, FromUnique) {
    auto unique_ptr = std::make_unique<MyObject>(47);
    MyObject* raw_ptr = unique_ptr.get();
    AtomicSharedPtr<MyObject> ptr =
        AtomicSharedPtr<MyObject>::from_unique(std::move(unique_ptr));

    EXPECT_TRUE(unique_ptr == nullptr);  // Unique ptr should be empty
    EXPECT_FALSE(ptr.is_null());
    EXPECT_EQ(ptr.get_raw_unsafe(), raw_ptr);
    EXPECT_EQ(ptr.load()->id, 47);
    EXPECT_EQ(MyObject::instance_count, 1);
}

// Test make_atomic_shared (helper function)
TEST_F(AtomicSharedPtrTest, MakeAtomicShared) {
    AtomicSharedPtr<MyObject> ptr = make_atomic_shared<MyObject>(48);
    EXPECT_FALSE(ptr.is_null());
    EXPECT_EQ(ptr.load()->id, 48);
    EXPECT_EQ(MyObject::instance_count, 1);

    AtomicSharedPtrConfig config;
    config.enable_statistics = true;
    AtomicSharedPtr<MyObject> ptr_with_config =
        make_atomic_shared<MyObject>(config, 49);
    EXPECT_FALSE(ptr_with_config.is_null());
    EXPECT_EQ(ptr_with_config.load()->id, 49);
    EXPECT_TRUE(ptr_with_config.get_config().enable_statistics);
    EXPECT_EQ(MyObject::instance_count, 2);  // 48 + 49
}

// Concurrency test for load/store
TEST_F(AtomicSharedPtrTest, ConcurrentLoadStore) {
    AtomicSharedPtr<MyObject> ptr(0);
    const int num_threads = 10;
    const int operations_per_thread = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                if ((j % 2) == 0) {
                    ptr.store(std::make_shared<MyObject>(i * 1000 + j));
                } else {
                    std::shared_ptr<MyObject> obj = ptr.load();
                    if (obj) {
                        // Do something with obj to ensure it's valid
                        volatile int id = obj->id;
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Final check
    std::shared_ptr<MyObject> final_obj = ptr.load();
    EXPECT_FALSE(final_obj == nullptr);
    EXPECT_EQ(MyObject::instance_count,
              1);  // Only the last stored object should remain
}

// Concurrency test for compare_exchange
TEST_F(AtomicSharedPtrTest, ConcurrentCompareExchange) {
    AtomicSharedPtrConfig config;
    config.max_retry_attempts = 10000;  // Allow many retries for contention
    config.retry_delay = std::chrono::nanoseconds(1);
    config.enable_statistics = true;

    AtomicSharedPtr<MyObject> ptr(std::make_shared<MyObject>(0), config);
    const int num_threads = 10;
    const int increments_per_thread = 100;
    const int total_increments = num_threads * increments_per_thread;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                bool success = false;
                do {
                    std::shared_ptr<MyObject> expected = ptr.load();
                    std::shared_ptr<MyObject> desired =
                        std::make_shared<MyObject>(expected ? expected->id + 1
                                                            : 1);
                    success = ptr.compare_exchange_strong(expected, desired);
                } while (!success);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(ptr.load()->id, total_increments);
    EXPECT_EQ(MyObject::instance_count,
              1);  // Only the final object should remain

    // Check CAS statistics
    const AtomicSharedPtrStats* stats = ptr.get_stats();
    EXPECT_GE(stats->cas_operations.load(), total_increments);
    // cas_failures might be non-zero due to contention, but total operations
    // should be >= total_increments
}

// Test memory orderings
TEST_F(AtomicSharedPtrTest, MemoryOrderings) {
    AtomicSharedPtr<MyObject> ptr;

    // Test acquire/release for load/store
    std::thread t1([&]() {
        ptr.store(std::make_shared<MyObject>(100), std::memory_order_release);
    });

    std::thread t2([&]() {
        std::shared_ptr<MyObject> obj;
        do {
            obj = ptr.load(std::memory_order_acquire);
        } while (!obj || obj->id != 100);
        EXPECT_EQ(obj->id, 100);
    });

    t1.join();
    t2.join();

    // Test seq_cst for CAS
    AtomicSharedPtr<MyObject> cas_ptr(0);
    std::shared_ptr<MyObject> expected =
        cas_ptr.load(std::memory_order_seq_cst);
    std::shared_ptr<MyObject> desired = std::make_shared<MyObject>(1);
    bool success = cas_ptr.compare_exchange_strong(expected, desired,
                                                   std::memory_order_seq_cst,
                                                   std::memory_order_seq_cst);
    EXPECT_TRUE(success);
    EXPECT_EQ(cas_ptr.load()->id, 1);
}

// Test exception handling for null dereference
TEST_F(AtomicSharedPtrTest, NullDereferenceException) {
    AtomicSharedPtr<MyObject> ptr;
    // Fix: Wrap the expression in a lambda for EXPECT_THROW
    EXPECT_THROW({ (void)ptr->id; }, AtomicSharedPtrException);
}

// Test for ABA problem prevention (indirectly via versioning)
TEST_F(AtomicSharedPtrTest, ABAPrevention) {
    AtomicSharedPtrConfig config;
    config.max_retry_attempts = 1000;
    config.retry_delay = std::chrono::nanoseconds(1);
    AtomicSharedPtr<MyObject> ptr(std::make_shared<MyObject>(1), config);

    std::shared_ptr<MyObject> A = ptr.load();  // Value 1
    std::shared_ptr<MyObject> B = std::make_shared<MyObject>(2);
    std::shared_ptr<MyObject> C =
        std::make_shared<MyObject>(1);  // Same value as A, but different object

    // Thread 1: Tries to CAS A -> D
    std::shared_ptr<MyObject> D = std::make_shared<MyObject>(3);
    std::atomic<bool> t1_done = false;
    std::thread t1([&]() {
        std::shared_ptr<MyObject> expected_A = A;
        // This CAS might fail if main thread changes it to C and back to A
        ptr.compare_exchange_with_retry(expected_A, D);
        t1_done = true;
    });

    // Main thread: Changes A -> B -> C (back to value 1)
    std::this_thread::sleep_for(
        std::chrono::milliseconds(10));  // Give t1 a chance to load A
    ptr.store(B);
    ptr.store(C);  // Now ptr holds an object with value 1, but it's C, not A

    t1.join();

    // If ABA was not handled, t1 might have succeeded in CASing A->D
    // With versioning, even if the value is the same, the control block version
    // should differ. So, t1's CAS should have failed and retried with the new
    // C. The final value should be D (from t1's successful retry) or C (if t1
    // failed). Given the retry mechanism, it should eventually succeed with D.
    EXPECT_EQ(ptr.load()->id, 3);  // T1 should have eventually succeeded
    EXPECT_EQ(MyObject::instance_count, 1);
}

// Test for correct reference counting with multiple AtomicSharedPtrs
TEST_F(AtomicSharedPtrTest, MultipleAtomicSharedPtrRefs) {
    AtomicSharedPtr<MyObject> ptr1(1);
    EXPECT_EQ(ptr1.use_count(), 1);
    EXPECT_EQ(MyObject::instance_count, 1);

    AtomicSharedPtr<MyObject> ptr2 = ptr1;
    EXPECT_EQ(ptr1.use_count(), 2);
    EXPECT_EQ(ptr2.use_count(), 2);
    EXPECT_EQ(MyObject::instance_count, 1);

    AtomicSharedPtr<MyObject> ptr3(ptr1);
    EXPECT_EQ(ptr1.use_count(), 3);
    EXPECT_EQ(ptr2.use_count(), 3);
    EXPECT_EQ(ptr3.use_count(), 3);
    EXPECT_EQ(MyObject::instance_count, 1);

    ptr1.reset();
    EXPECT_EQ(ptr2.use_count(), 2);
    EXPECT_EQ(ptr3.use_count(), 2);
    EXPECT_EQ(MyObject::instance_count, 1);

    ptr2.reset();
    EXPECT_EQ(ptr3.use_count(), 1);
    EXPECT_EQ(MyObject::instance_count, 1);

    ptr3.reset();
    EXPECT_EQ(MyObject::instance_count, 0);
}

// Test for correct reference counting with std::shared_ptr mixed in
TEST_F(AtomicSharedPtrTest, MixedSharedPtrRefs) {
    AtomicSharedPtr<MyObject> ptr(1);
    EXPECT_EQ(ptr.use_count(), 1);
    EXPECT_EQ(MyObject::instance_count, 1);

    std::shared_ptr<MyObject> s_ptr1 = ptr.load();
    EXPECT_EQ(ptr.use_count(), 2);
    EXPECT_EQ(s_ptr1.use_count(), 2);
    EXPECT_EQ(MyObject::instance_count, 1);

    std::shared_ptr<MyObject> s_ptr2 = ptr.load();
    EXPECT_EQ(ptr.use_count(), 3);
    EXPECT_EQ(s_ptr1.use_count(), 3);
    EXPECT_EQ(s_ptr2.use_count(), 3);
    EXPECT_EQ(MyObject::instance_count, 1);

    s_ptr1.reset();
    EXPECT_EQ(ptr.use_count(), 2);
    EXPECT_EQ(s_ptr2.use_count(), 2);
    EXPECT_EQ(MyObject::instance_count, 1);

    ptr.reset();
    EXPECT_EQ(s_ptr2.use_count(), 1);
    EXPECT_EQ(MyObject::instance_count, 1);

    s_ptr2.reset();
    EXPECT_EQ(MyObject::instance_count, 0);
}

// Test for correct handling of null shared_ptr in operations
TEST_F(AtomicSharedPtrTest, NullSharedPtrHandling) {
    AtomicSharedPtr<MyObject> ptr;  // Starts null

    // Load from null
    std::shared_ptr<MyObject> loaded = ptr.load();
    EXPECT_TRUE(loaded == nullptr);

    // Store null
    ptr.store(nullptr);
    EXPECT_TRUE(ptr.is_null());

    // Exchange with null
    ptr.store(std::make_shared<MyObject>(1));
    std::shared_ptr<MyObject> exchanged = ptr.exchange(nullptr);
    EXPECT_FALSE(exchanged == nullptr);
    EXPECT_EQ(exchanged->id, 1);
    EXPECT_TRUE(ptr.is_null());
    exchanged.reset();

    // CAS with null
    ptr.store(std::make_shared<MyObject>(2));
    std::shared_ptr<MyObject> expected_null = nullptr;
    std::shared_ptr<MyObject> desired_null = nullptr;
    std::shared_ptr<MyObject> current_val = ptr.load();

    // CAS from non-null to null
    bool success = ptr.compare_exchange_strong(current_val, desired_null);
    EXPECT_TRUE(success);
    EXPECT_TRUE(ptr.is_null());
    EXPECT_EQ(MyObject::instance_count, 0);  // Original object should be gone

    // CAS from null to non-null
    ptr.reset();
    expected_null = nullptr;
    desired_null = std::make_shared<MyObject>(3);
    success = ptr.compare_exchange_strong(expected_null, desired_null);
    EXPECT_TRUE(success);
    EXPECT_FALSE(ptr.is_null());
    EXPECT_EQ(ptr.load()->id, 3);
    EXPECT_EQ(MyObject::instance_count, 1);
}

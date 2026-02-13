#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>      // Used for sleep_for
#include <functional>  // Used for std::function, std::ref, std::hash
#include <optional>    // Used for std::optional
#include <thread>
#include <vector>

#include "atom/async/threadlocal.hpp"

using namespace atom::async;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Ne;
using ::testing::NotNull;

// Helper struct to track construction, destruction, and cleanup
struct MyData {
    int value = 0;
    std::atomic<int>* init_count = nullptr;
    std::atomic<int>* destroy_count = nullptr;
    std::atomic<int>* cleanup_count = nullptr;
    std::thread::id thread_id;

    MyData() = default;  // Required by EnhancedThreadLocalStorable

    explicit MyData(int val, std::atomic<int>* init_c = nullptr,
                    std::atomic<int>* destroy_c = nullptr,
                    std::atomic<int>* cleanup_c = nullptr)
        : value(val),
          init_count(init_c),
          destroy_count(destroy_c),
          cleanup_count(cleanup_c) {
        if (init_count)
            (*init_count)++;
        thread_id = std::this_thread::get_id();
    }

    // Move constructor (required by EnhancedThreadLocalStorable)
    MyData(MyData&& other) noexcept
        : value(other.value),
          init_count(other.init_count),
          destroy_count(other.destroy_count),
          cleanup_count(other.cleanup_count),
          thread_id(other.thread_id) {
        // Reset other's pointers to prevent double counting in its destructor
        other.init_count = nullptr;
        other.destroy_count = nullptr;
        other.cleanup_count = nullptr;
    }

    // Move assignment (required by EnhancedThreadLocalStorable)
    MyData& operator=(MyData&& other) noexcept {
        if (this != &other) {
            // Call cleanup/destroy for the current object if pointers are valid
            if (cleanup_count)
                (*cleanup_count)++;
            if (destroy_count)
                (*destroy_count)++;

            value = other.value;
            init_count = other.init_count;
            destroy_count = other.destroy_count;
            cleanup_count = other.cleanup_count;
            thread_id = other.thread_id;

            // Reset other's pointers
            other.init_count = nullptr;
            other.destroy_count = nullptr;
            other.cleanup_count = nullptr;
        }
        return *this;
    }

    // Destructor (required by EnhancedThreadLocalStorable)
    ~MyData() noexcept {
        if (destroy_count)
            (*destroy_count)++;
    }

    // Equality for compareAndUpdate
    bool operator==(const MyData& other) const { return value == other.value; }
    bool operator==(int other_value) const { return value == other_value; }
};

// Test fixture for EnhancedThreadLocal tests
class ThreadLocalTest : public ::testing::Test {
protected:
    std::atomic<int> init_count{0};
    std::atomic<int> destroy_count{0};
    std::atomic<int> cleanup_count{0};

    // Cleanup function for MyData
    auto my_cleanup_fn() {
        return [&](MyData& data) {
            if (data.cleanup_count)
                (*data.cleanup_count)++;
        };
    }

    void SetUp() override {
        init_count = 0;
        destroy_count = 0;
        cleanup_count = 0;
    }

    void TearDown() override {
        // Ensure all thread-local values are cleaned up by the ThreadLocal
        // destructor The ThreadLocal object is destroyed automatically after
        // each test
    }
};

// Test default constructor - no initializer
TEST_F(ThreadLocalTest, DefaultConstructor_NoInitializer_ThrowsOnGet) {
    ThreadLocal<MyData> tl;

    // get() should throw if no initializer is provided and value doesn't exist
    EXPECT_THROW(tl.get(), ThreadLocalException);
    EXPECT_FALSE(tl.hasValue());
    EXPECT_THAT(tl.tryGet(), Eq(std::nullopt));
    EXPECT_THAT(tl.getPointer(), IsNull());
    EXPECT_THAT(tl.getPointer(), IsNull());   // const version
    EXPECT_THROW(*tl, ThreadLocalException);  // Dereference should throw
    EXPECT_THAT(
        tl->value,
        Eq(0));  // Arrow operator returns default constructed if get() throws
}

// Test constructor with InitializerFn
TEST_F(ThreadLocalTest, InitializerFn_InitializesOnFirstGet) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(100, &init_count, &destroy_count, &cleanup_count);
        }),
        my_cleanup_fn());

    EXPECT_FALSE(tl.hasValue());
    EXPECT_THAT(init_count.load(), Eq(0));

    // First get() should initialize
    MyData& data1 = tl.get();
    EXPECT_THAT(data1.value, Eq(100));
    EXPECT_THAT(init_count.load(), Eq(1));
    EXPECT_TRUE(tl.hasValue());
    EXPECT_THAT(tl.tryGet().value().get().value, Eq(100));
    EXPECT_THAT(tl.getPointer()->value, Eq(100));
    EXPECT_THAT(tl.getPointer()->value, Eq(100));  // const version
    EXPECT_THAT((*tl).value, Eq(100));             // Dereference
    EXPECT_THAT(tl->value, Eq(100));               // Arrow operator

    // Subsequent get() should not re-initialize
    MyData& data2 = tl.get();
    EXPECT_THAT(data2.value, Eq(100));
    EXPECT_THAT(init_count.load(), Eq(1));  // Still 1
    EXPECT_TRUE(tl.hasValue());

    // ValueWrapper test
    auto wrapper = tl.getWrapper();
    EXPECT_THAT(wrapper.get().value, Eq(100));
    EXPECT_THAT(wrapper->value, Eq(100));
    EXPECT_THAT((*wrapper).value, Eq(100));
    EXPECT_THAT(wrapper.apply([](MyData& d) { return d.value + 1; }), Eq(101));
    auto transformed_data =
        wrapper.transform([](MyData& d) { return MyData(d.value * 2); });
    EXPECT_THAT(transformed_data.value, Eq(200));
}

// Test constructor with ConditionalInitializerFn returning a value
TEST_F(ThreadLocalTest, ConditionalInitializerFn_ReturnsValue_Initializes) {
    // Explicitly cast lambda to ConditionalInitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::ConditionalInitializerFn>(
            [&]() -> std::optional<MyData> {
                return MyData(200, &init_count, &destroy_count, &cleanup_count);
            }),
        my_cleanup_fn());

    EXPECT_FALSE(tl.hasValue());
    EXPECT_THAT(init_count.load(), Eq(0));

    MyData& data = tl.get();
    EXPECT_THAT(data.value, Eq(200));
    EXPECT_THAT(init_count.load(), Eq(1));
    EXPECT_TRUE(tl.hasValue());
}

// Test constructor with ConditionalInitializerFn returning nullopt
TEST_F(ThreadLocalTest,
       ConditionalInitializerFn_ReturnsNullopt_DoesNotInitialize) {
    // Explicitly cast lambda to ConditionalInitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::ConditionalInitializerFn>(
            [&]() -> std::optional<MyData> {
                return std::nullopt;  // Return empty
            }),
        my_cleanup_fn());

    EXPECT_FALSE(tl.hasValue());
    EXPECT_THAT(init_count.load(), Eq(0));

    // get() should throw if initializer returns nullopt
    EXPECT_THROW(tl.get(), ThreadLocalException);
    EXPECT_FALSE(tl.hasValue());
    EXPECT_THAT(tl.tryGet(), Eq(std::nullopt));
    EXPECT_THAT(init_count.load(), Eq(0));  // Should not have been called
}

// Test constructor with ThreadIdInitializerFn
TEST_F(ThreadLocalTest, ThreadIdInitializerFn_InitializesWithThreadId) {
    // Explicitly cast lambda to ThreadIdInitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::ThreadIdInitializerFn>(
            [&](std::thread::id tid) {
                // Simple hash of thread ID for value
                size_t tid_hash = std::hash<std::thread::id>{}(tid);
                return MyData(static_cast<int>(tid_hash % 1000), &init_count,
                              &destroy_count, &cleanup_count);
            }),
        my_cleanup_fn());

    EXPECT_FALSE(tl.hasValue());
    EXPECT_THAT(init_count.load(), Eq(0));

    MyData& data = tl.get();
    EXPECT_THAT(init_count.load(), Eq(1));
    EXPECT_TRUE(tl.hasValue());
    EXPECT_THAT(data.thread_id, Eq(std::this_thread::get_id()));

    // Check value is based on thread ID hash
    size_t expected_hash =
        std::hash<std::thread::id>{}(std::this_thread::get_id());
    EXPECT_THAT(data.value, Eq(static_cast<int>(expected_hash % 1000)));
}

// Test constructor with a default value
TEST_F(ThreadLocalTest, DefaultValueConstructor_InitializesWithValue) {
    ThreadLocal<MyData> tl(
        MyData(300, &init_count, &destroy_count, &cleanup_count));

    EXPECT_FALSE(
        tl.hasValue());  // Value is created on first access, not construction

    MyData& data = tl.get();
    EXPECT_THAT(data.value, Eq(300));
    EXPECT_THAT(init_count.load(),
                Eq(1));  // Initializer (lambda capturing value) is called
    EXPECT_TRUE(tl.hasValue());
}

// Test getOrCreate when value doesn't exist
TEST_F(ThreadLocalTest, GetOrCreate_ValueDoesNotExist_CallsFactory) {
    ThreadLocal<MyData> tl;  // No initializers
    std::atomic<int> factory_call_count{0};

    EXPECT_FALSE(tl.hasValue());
    EXPECT_THAT(factory_call_count.load(), Eq(0));

    MyData& data = tl.getOrCreate([&]() {
        factory_call_count++;
        return MyData(400, &init_count, &destroy_count, &cleanup_count);
    });

    EXPECT_THAT(data.value, Eq(400));
    EXPECT_THAT(factory_call_count.load(), Eq(1));
    EXPECT_THAT(init_count.load(), Eq(1));
    EXPECT_TRUE(tl.hasValue());

    // Subsequent getOrCreate should not call factory
    MyData& data2 = tl.getOrCreate([&]() {
        factory_call_count++;  // This should not happen
        return MyData(500);
    });
    EXPECT_THAT(data2.value, Eq(400));  // Still the original value
    EXPECT_THAT(factory_call_count.load(), Eq(1));
}

// Test getOrCreate when value already exists
TEST_F(ThreadLocalTest, GetOrCreate_ValueExists_DoesNotCallFactory) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(400, &init_count, &destroy_count, &cleanup_count);
        }));
    std::atomic<int> factory_call_count{0};

    // First get() initializes the value using the constructor initializer
    (void)tl.get();
    EXPECT_THAT(init_count.load(), Eq(1));
    EXPECT_TRUE(tl.hasValue());

    // Now call getOrCreate - it should find the existing value
    MyData& data = tl.getOrCreate([&]() {
        factory_call_count++;  // This should not happen
        return MyData(500);
    });

    EXPECT_THAT(data.value, Eq(400));               // Still the original value
    EXPECT_THAT(factory_call_count.load(), Eq(0));  // Factory was not called
    EXPECT_THAT(init_count.load(), Eq(1));          // Still 1
}

// Test getOrCreate when factory throws
TEST_F(ThreadLocalTest, GetOrCreate_FactoryThrows_RethrowsAndDoesNotStore) {
    ThreadLocal<MyData> tl;  // No initializers

    EXPECT_FALSE(tl.hasValue());

    // Call getOrCreate with a factory that throws
    EXPECT_THROW(tl.getOrCreate([&]() -> MyData {
        throw std::runtime_error("Factory failed");
        return MyData(600);  // Unreachable
    }),
                 std::runtime_error);

    // Value should not have been created or stored
    EXPECT_FALSE(tl.hasValue());
    EXPECT_THAT(tl.tryGet(), Eq(std::nullopt));
    EXPECT_THAT(init_count.load(), Eq(0));
}

// Test tryGet when value exists
TEST_F(ThreadLocalTest, TryGet_ValueExists_ReturnsOptionalRef) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(700, &init_count, &destroy_count, &cleanup_count);
        }));

    EXPECT_FALSE(tl.hasValue());
    EXPECT_THAT(tl.tryGet(), Eq(std::nullopt));  // Before get()

    // Initialize the value
    (void)tl.get();
    EXPECT_TRUE(tl.hasValue());

    // tryGet should now return an optional reference
    auto opt_ref = tl.tryGet();
    EXPECT_THAT(opt_ref, Ne(std::nullopt));
    EXPECT_THAT(opt_ref.value().get().value, Eq(700));
}

// Test tryGet when value does not exist
TEST_F(ThreadLocalTest, TryGet_ValueDoesNotExist_ReturnsNullopt) {
    ThreadLocal<MyData> tl;  // No initializers

    EXPECT_FALSE(tl.hasValue());
    EXPECT_THAT(tl.tryGet(), Eq(std::nullopt));  // No value, no initializer
}

// Test access operators and ValueWrapper
TEST_F(ThreadLocalTest, AccessOperators_GetWrapper_Arrow_Dereference) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(800, &init_count, &destroy_count, &cleanup_count);
        }));

    // Initialize the value
    MyData& data = tl.get();

    // Test dereference operator
    EXPECT_THAT((*tl).value, Eq(800));
    (*tl).value = 801;
    EXPECT_THAT(data.value, Eq(801));

    // Test arrow operator
    EXPECT_THAT(tl->value, Eq(801));
    tl->value = 802;
    EXPECT_THAT(data.value, Eq(802));

    // Test ValueWrapper
    auto wrapper = tl.getWrapper();
    EXPECT_THAT(wrapper.get().value, Eq(802));
    EXPECT_THAT(wrapper->value, Eq(802));
    EXPECT_THAT((*wrapper).value, Eq(802));

    wrapper->value = 803;
    EXPECT_THAT(data.value, Eq(803));

    wrapper.apply([](MyData& d) { d.value += 10; });
    EXPECT_THAT(data.value, Eq(813));

    auto transformed_data =
        wrapper.transform([](MyData& d) { return MyData(d.value * 2); });
    EXPECT_THAT(transformed_data.value, Eq(813 * 2));
    EXPECT_THAT(data.value,
                Eq(813));  // Original value is unchanged by transform
}

// Test const access operators when no value exists
TEST_F(ThreadLocalTest,
       ConstAccessOperators_Arrow_Dereference_NoValue_ThrowsOrNull) {
    ThreadLocal<MyData> tl;  // No initializers

    // Const arrow operator should return nullptr
    const ThreadLocal<MyData>& const_tl = tl;
    EXPECT_THAT(const_tl.getPointer(), IsNull());
    // Accessing member of nullptr is UB, but often returns default value in
    // tests EXPECT_THAT(const_tl->value, Eq(0)); // Removed UB access

    // Const dereference operator should throw
    EXPECT_THROW(*const_tl, ThreadLocalException);
}

// Test reset() with default value
TEST_F(ThreadLocalTest, Reset_DefaultValue) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(900, &init_count, &destroy_count, &cleanup_count);
        }),
        my_cleanup_fn());

    // Initialize and set a value
    tl.get().value = 901;
    EXPECT_THAT(tl.get().value, Eq(901));
    EXPECT_THAT(init_count.load(), Eq(1));
    EXPECT_THAT(cleanup_count.load(), Eq(0));
    EXPECT_THAT(destroy_count.load(), Eq(0));

    // Reset to default value
    tl.reset();
    EXPECT_THAT(tl.get().value, Eq(0));     // Default constructed value
    EXPECT_THAT(init_count.load(), Eq(1));  // No new initialization
    EXPECT_THAT(cleanup_count.load(),
                Eq(1));  // Cleanup called for old value (901)
    EXPECT_THAT(destroy_count.load(), Eq(1));  // Old value destroyed
}

// Test reset(value)
TEST_F(ThreadLocalTest, Reset_WithValue) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(1000, &init_count, &destroy_count, &cleanup_count);
        }),
        my_cleanup_fn());

    // Initialize and set a value
    tl.get().value = 1001;
    EXPECT_THAT(tl.get().value, Eq(1001));
    EXPECT_THAT(init_count.load(), Eq(1));
    EXPECT_THAT(cleanup_count.load(), Eq(0));
    EXPECT_THAT(destroy_count.load(), Eq(0));

    // Reset with a new value
    tl.reset(MyData(1002, &init_count, &destroy_count, &cleanup_count));
    EXPECT_THAT(tl.get().value, Eq(1002));
    EXPECT_THAT(init_count.load(),
                Eq(2));  // New MyData was constructed for reset
    EXPECT_THAT(cleanup_count.load(),
                Eq(1));  // Cleanup called for old value (1001)
    EXPECT_THAT(destroy_count.load(), Eq(1));  // Old value destroyed
}

// Test hasValue and getPointer
TEST_F(ThreadLocalTest, HasValue_GetPointer_BeforeAndAfterGet) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(1100, &init_count, &destroy_count, &cleanup_count);
        }));

    EXPECT_FALSE(tl.hasValue());
    EXPECT_THAT(tl.getPointer(), IsNull());
    EXPECT_THAT(tl.getPointer(), IsNull());  // const version

    // Initialize
    (void)tl.get();

    EXPECT_TRUE(tl.hasValue());
    EXPECT_THAT(tl.getPointer(), NotNull());
    EXPECT_THAT(tl.getPointer()->value, Eq(1100));
    EXPECT_THAT(tl.getPointer(), NotNull());        // const version
    EXPECT_THAT(tl.getPointer()->value, Eq(1100));  // const version
}

// Test compareAndUpdate - success
TEST_F(ThreadLocalTest, CompareAndUpdate_Success) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(1200, &init_count, &destroy_count, &cleanup_count);
        }),
        my_cleanup_fn());

    // Initialize and set value
    tl.get().value = 1201;
    EXPECT_THAT(tl.get().value, Eq(1201));
    EXPECT_THAT(init_count.load(), Eq(1));
    EXPECT_THAT(cleanup_count.load(), Eq(0));
    EXPECT_THAT(destroy_count.load(), Eq(0));

    // Compare and update - success
    // Pass MyData object for expected value
    bool updated = tl.compareAndUpdate(
        MyData(1201),
        MyData(1202, &init_count, &destroy_count, &cleanup_count));
    EXPECT_TRUE(updated);
    EXPECT_THAT(tl.get().value, Eq(1202));
    EXPECT_THAT(init_count.load(),
                Eq(2));  // New MyData constructed for desired
    EXPECT_THAT(cleanup_count.load(),
                Eq(1));  // Cleanup called for old value (1201)
    EXPECT_THAT(destroy_count.load(), Eq(1));  // Old value destroyed
}

// Test compareAndUpdate - failure (wrong expected)
TEST_F(ThreadLocalTest, CompareAndUpdate_Failure_WrongExpected) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(1300, &init_count, &destroy_count, &cleanup_count);
        }),
        my_cleanup_fn());

    // Initialize and set value
    tl.get().value = 1301;
    EXPECT_THAT(tl.get().value, Eq(1301));
    EXPECT_THAT(init_count.load(), Eq(1));
    EXPECT_THAT(cleanup_count.load(), Eq(0));
    EXPECT_THAT(destroy_count.load(), Eq(0));

    // Compare and update - failure
    // Pass MyData object for expected value
    bool updated = tl.compareAndUpdate(
        MyData(1300),
        MyData(1302, &init_count, &destroy_count, &cleanup_count));
    EXPECT_FALSE(updated);
    EXPECT_THAT(tl.get().value, Eq(1301));  // Value should be unchanged
    EXPECT_THAT(init_count.load(),
                Eq(1));  // Desired MyData was constructed but not used
    EXPECT_THAT(cleanup_count.load(), Eq(0));  // Cleanup not called
    EXPECT_THAT(destroy_count.load(), Eq(1));  // Desired MyData was destroyed
}

// Test compareAndUpdate - failure (no value)
TEST_F(ThreadLocalTest, CompareAndUpdate_Failure_NoValue) {
    // Use default constructor and set cleanup function
    ThreadLocal<MyData> tl;
    tl.setCleanupFunction(my_cleanup_fn());

    EXPECT_FALSE(tl.hasValue());
    EXPECT_THAT(init_count.load(), Eq(0));
    EXPECT_THAT(cleanup_count.load(), Eq(0));
    EXPECT_THAT(destroy_count.load(), Eq(0));

    // Compare and update when no value exists
    // Pass MyData object for expected value
    bool updated = tl.compareAndUpdate(
        MyData(1400),
        MyData(1401, &init_count, &destroy_count, &cleanup_count));
    EXPECT_FALSE(updated);
    EXPECT_FALSE(tl.hasValue());  // Value still doesn't exist
    EXPECT_THAT(init_count.load(),
                Eq(0));  // Desired MyData was constructed but not used
    EXPECT_THAT(cleanup_count.load(), Eq(0));  // Cleanup not called
    EXPECT_THAT(destroy_count.load(), Eq(1));  // Desired MyData was destroyed
}

// Test update - success
TEST_F(ThreadLocalTest, Update_Success) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(1500, &init_count, &destroy_count, &cleanup_count);
        }),
        my_cleanup_fn());

    // Initialize
    (void)tl.get();
    EXPECT_THAT(tl.get().value, Eq(1500));
    EXPECT_THAT(init_count.load(), Eq(1));
    EXPECT_THAT(cleanup_count.load(), Eq(0));
    EXPECT_THAT(destroy_count.load(), Eq(0));

    // Update using a function
    bool updated = tl.update([](MyData& data) { data.value += 50; });
    EXPECT_TRUE(updated);
    EXPECT_THAT(tl.get().value, Eq(1550));
    EXPECT_THAT(init_count.load(), Eq(1));  // No new construction
    EXPECT_THAT(cleanup_count.load(),
                Eq(0));  // Cleanup not called (in-place update)
    EXPECT_THAT(destroy_count.load(), Eq(0));  // No destruction
}

// Test update - failure (no value)
TEST_F(ThreadLocalTest, Update_Failure_NoValue) {
    // Use default constructor and set cleanup function
    ThreadLocal<MyData> tl;
    tl.setCleanupFunction(my_cleanup_fn());

    EXPECT_FALSE(tl.hasValue());
    EXPECT_THAT(init_count.load(), Eq(0));

    // Update when no value exists
    bool updated = tl.update([](MyData& data) {
        data.value += 50;  // This should not be called
    });
    EXPECT_FALSE(updated);
    EXPECT_FALSE(tl.hasValue());
    EXPECT_THAT(init_count.load(), Eq(0));
}

// Test forEach
TEST_F(ThreadLocalTest, ForEach_IteratesOverValues) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(1600, &init_count, &destroy_count, &cleanup_count);
        }));

    const int num_threads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> main_thread_iteration_count{0};
    std::atomic<int> total_value_sum{0};

    // Initialize values in multiple threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tl, i]() {
            tl.get().value = 1600 + i;  // Set a unique value per thread
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_THAT(tl.size(), Eq(num_threads));
    EXPECT_THAT(init_count.load(), Eq(num_threads));

    // Iterate using forEach from the main thread
    tl.forEach([&](MyData& data) {
        main_thread_iteration_count++;
        total_value_sum += data.value;
    });

    EXPECT_THAT(main_thread_iteration_count.load(), Eq(num_threads));
    int expected_sum = 0;
    for (int i = 0; i < num_threads; ++i)
        expected_sum += (1600 + i);
    EXPECT_THAT(total_value_sum.load(), Eq(expected_sum));
}

// Test forEachWithId
TEST_F(ThreadLocalTest, ForEachWithId_IteratesWithThreadId) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(1700, &init_count, &destroy_count, &cleanup_count);
        }));

    const int num_threads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> main_thread_iteration_count{0};
    std::vector<std::thread::id> initialized_tids(num_threads);

    // Initialize values in multiple threads and store their IDs
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tl, &initialized_tids, i]() {
            tl.get().value = 1700 + i;  // Set a unique value per thread
            initialized_tids[i] = std::this_thread::get_id();
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_THAT(tl.size(), Eq(num_threads));
    EXPECT_THAT(init_count.load(), Eq(num_threads));

    // Iterate using forEachWithId from the main thread
    tl.forEachWithId([&](MyData& data, std::thread::id tid) {
        main_thread_iteration_count++;
        // Verify the thread ID matches the one stored in MyData
        EXPECT_THAT(data.thread_id, Eq(tid));
        // Verify the thread ID is one of the initialized thread IDs
        bool found_tid = false;
        for (const auto& init_tid : initialized_tids) {
            if (init_tid == tid) {
                found_tid = true;
                break;
            }
        }
        EXPECT_TRUE(found_tid);
    });

    EXPECT_THAT(main_thread_iteration_count.load(), Eq(num_threads));
}

// Test clearCurrentThread
TEST_F(ThreadLocalTest, ClearCurrentThread_RemovesCurrentThreadValue) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(1800, &init_count, &destroy_count, &cleanup_count);
        }),
        my_cleanup_fn());

    // Initialize value in main thread
    (void)tl.get();
    EXPECT_TRUE(tl.hasValue());
    EXPECT_THAT(tl.size(), Eq(1));
    EXPECT_THAT(init_count.load(), Eq(1));
    EXPECT_THAT(cleanup_count.load(), Eq(0));
    EXPECT_THAT(destroy_count.load(), Eq(0));

    // Clear value for current thread
    tl.clearCurrentThread();
    EXPECT_FALSE(tl.hasValue());
    EXPECT_THAT(tl.size(), Eq(0));
    EXPECT_THAT(cleanup_count.load(), Eq(1));  // Cleanup should be called
    EXPECT_THAT(destroy_count.load(), Eq(1));  // Value should be destroyed

    // Getting again should re-initialize
    (void)tl.get();
    EXPECT_TRUE(tl.hasValue());
    EXPECT_THAT(tl.size(), Eq(1));
    EXPECT_THAT(init_count.load(), Eq(2));     // Re-initialized
    EXPECT_THAT(cleanup_count.load(), Eq(1));  // Still 1
    EXPECT_THAT(destroy_count.load(), Eq(1));  // Still 1
}

// Test clear
TEST_F(ThreadLocalTest, Clear_RemovesAllValues) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(1900, &init_count, &destroy_count, &cleanup_count);
        }),
        my_cleanup_fn());

    const int num_threads = 5;
    std::vector<std::thread> threads;

    // Initialize values in multiple threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tl]() { (void)tl.get(); });
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_THAT(tl.size(), Eq(num_threads));
    EXPECT_THAT(init_count.load(), Eq(num_threads));
    EXPECT_THAT(cleanup_count.load(), Eq(0));
    EXPECT_THAT(destroy_count.load(), Eq(0));

    // Clear all values from main thread
    tl.clear();
    EXPECT_THAT(tl.size(), Eq(0));
    EXPECT_FALSE(tl.hasValue());  // Main thread value is also cleared
    EXPECT_THAT(cleanup_count.load(),
                Eq(num_threads));  // Cleanup called for all
    EXPECT_THAT(destroy_count.load(), Eq(num_threads));  // All values destroyed

    // Getting again should re-initialize for the current thread
    (void)tl.get();
    EXPECT_TRUE(tl.hasValue());
    EXPECT_THAT(tl.size(), Eq(1));
    EXPECT_THAT(init_count.load(),
                Eq(num_threads + 1));  // Main thread re-initialized
    EXPECT_THAT(cleanup_count.load(), Eq(num_threads));  // Still num_threads
    EXPECT_THAT(destroy_count.load(), Eq(num_threads));  // Still num_threads
}

// Test removeIf
TEST_F(ThreadLocalTest, RemoveIf_RemovesMatchingValues) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(2000, &init_count, &destroy_count, &cleanup_count);
        }),
        my_cleanup_fn());

    const int num_threads = 10;
    std::vector<std::thread> threads;

    // Initialize values in multiple threads with values 2000 to 2009
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tl, i]() { tl.get().value = 2000 + i; });
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_THAT(tl.size(), Eq(num_threads));
    EXPECT_THAT(init_count.load(), Eq(num_threads));
    EXPECT_THAT(cleanup_count.load(), Eq(0));
    EXPECT_THAT(destroy_count.load(), Eq(0));

    // Remove values where value is even
    std::size_t removed_count =
        tl.removeIf([](MyData& data) { return data.value % 2 == 0; });

    EXPECT_THAT(
        removed_count,
        Eq(num_threads / 2));  // Should remove 5 values (2000, 2002, ..., 2008)
    EXPECT_THAT(tl.size(), Eq(num_threads / 2));  // 5 remaining
    EXPECT_THAT(cleanup_count.load(),
                Eq(num_threads / 2));  // Cleanup called for removed
    EXPECT_THAT(destroy_count.load(),
                Eq(num_threads / 2));  // Removed values destroyed

    // Verify remaining values are odd
    std::atomic<int> remaining_count{0};
    tl.forEach([&](MyData& data) {
        EXPECT_THAT(data.value % 2, Eq(1));
        remaining_count++;
    });
    EXPECT_THAT(remaining_count.load(), Eq(num_threads / 2));
}

// Test size and empty
TEST_F(ThreadLocalTest, Size_Empty_ReflectState) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(2100, &init_count, &destroy_count, &cleanup_count);
        }));

    EXPECT_THAT(tl.size(), Eq(0));
    EXPECT_TRUE(tl.empty());

    // Initialize in main thread
    (void)tl.get();
    EXPECT_THAT(tl.size(), Eq(1));
    EXPECT_FALSE(tl.empty());

    // Initialize in another thread
    std::thread t([&tl]() { (void)tl.get(); });
    t.join();

    EXPECT_THAT(tl.size(), Eq(2));
    EXPECT_FALSE(tl.empty());

    // Clear current thread (main)
    tl.clearCurrentThread();
    EXPECT_THAT(tl.size(), Eq(1));
    EXPECT_FALSE(tl.empty());

    // Clear all
    tl.clear();
    EXPECT_THAT(tl.size(), Eq(0));
    EXPECT_TRUE(tl.empty());
}

// Test setCleanupFunction
TEST_F(ThreadLocalTest, SetCleanupFunction_ChangesCleanup) {
    // Explicitly cast lambda to InitializerFn, pass nullptr for initial cleanup
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(2200, &init_count, &destroy_count,
                          nullptr);  // No cleanup initially
        }),
        nullptr);

    // Initialize
    (void)tl.get();
    EXPECT_THAT(init_count.load(), Eq(1));
    EXPECT_THAT(cleanup_count.load(), Eq(0));

    // Set a cleanup function
    tl.setCleanupFunction(my_cleanup_fn());

    // Clear the value - the new cleanup function should be called
    tl.clearCurrentThread();
    EXPECT_THAT(cleanup_count.load(), Eq(1));  // Cleanup called

    // Re-initialize
    (void)tl.get();
    EXPECT_THAT(init_count.load(), Eq(2));
    EXPECT_THAT(cleanup_count.load(), Eq(1));  // Still 1

    // Set a different cleanup function
    std::atomic<int> another_cleanup_count{0};
    tl.setCleanupFunction([&](MyData& data) { another_cleanup_count++; });

    // Clear again - the new cleanup function should be called
    tl.clearCurrentThread();
    EXPECT_THAT(cleanup_count.load(), Eq(1));  // Old cleanup not called again
    EXPECT_THAT(another_cleanup_count.load(), Eq(1));  // New cleanup called
}

// Test hasValueForThread
TEST_F(ThreadLocalTest, HasValueForThread_ChecksSpecificThread) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(2300, &init_count, &destroy_count, &cleanup_count);
        }));

    std::thread::id main_tid = std::this_thread::get_id();
    std::thread::id other_tid;

    EXPECT_FALSE(tl.hasValueForThread(main_tid));

    // Initialize in main thread
    (void)tl.get();
    EXPECT_TRUE(tl.hasValueForThread(main_tid));

    // Initialize in another thread
    std::thread t([&tl, &other_tid]() {
        other_tid = std::this_thread::get_id();
        (void)tl.get();
    });
    t.join();

    EXPECT_TRUE(tl.hasValueForThread(main_tid));
    EXPECT_TRUE(tl.hasValueForThread(other_tid));
    EXPECT_FALSE(
        tl.hasValueForThread(std::thread::id()));  // Check a non-existent ID
}

// Test thread safety of concurrent get()
TEST_F(ThreadLocalTest, ThreadSafety_ConcurrentGet) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            // Simulate some work during initialization
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return MyData(2400, &init_count, &destroy_count, &cleanup_count);
        }),
        my_cleanup_fn());

    const int num_threads = 20;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tl, &success_count]() {
            try {
                MyData& data = tl.get();
                EXPECT_THAT(data.value, Eq(2400));
                success_count++;
            } catch (...) {
                // Should not throw
                ADD_FAILURE() << "Exception thrown in thread";
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_THAT(success_count.load(), Eq(num_threads));
    EXPECT_THAT(init_count.load(),
                Eq(num_threads));  // Each thread initializes its own copy
    EXPECT_THAT(tl.size(), Eq(num_threads));
}

// Test thread safety of concurrent getOrCreate
TEST_F(ThreadLocalTest, ThreadSafety_ConcurrentGetOrCreate) {
    ThreadLocal<MyData> tl;  // No initializers
    std::atomic<int> factory_call_count{0};

    const int num_threads = 20;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tl, &factory_call_count, &success_count, i]() {
            try {
                MyData& data = tl.getOrCreate([&]() {
                    factory_call_count++;
                    // Simulate work
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    return MyData(3000 + i, nullptr, nullptr,
                                  nullptr);  // Use different values
                });
                // Verify the value is one of the expected values (3000 + thread
                // index) Note: We can't easily verify the *exact* value if
                // multiple threads race to initialize the *same* thread-local
                // slot (which shouldn't happen with thread-local storage, but
                // getOrCreate is general). For thread-local, each thread gets
                // its *own* slot, so the factory is called once per thread.
                EXPECT_GE(data.value, 3000);
                EXPECT_LT(data.value, 3000 + num_threads);
                success_count++;
            } catch (...) {
                ADD_FAILURE() << "Exception thrown in thread";
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_THAT(success_count.load(), Eq(num_threads));
    EXPECT_THAT(factory_call_count.load(),
                Eq(num_threads));  // Factory called once per thread
    EXPECT_THAT(tl.size(), Eq(num_threads));
}

// Test thread safety of concurrent reset
TEST_F(ThreadLocalTest, ThreadSafety_ConcurrentReset) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(4000, &init_count, &destroy_count, &cleanup_count);
        }),
        my_cleanup_fn());

    const int num_threads = 10;
    const int num_resets_per_thread = 50;
    std::vector<std::thread> threads;

    // Initialize values in all threads first
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tl]() { (void)tl.get(); });
    }
    for (auto& t : threads) {
        t.join();
    }
    threads.clear();  // Clear threads vector

    EXPECT_THAT(tl.size(), Eq(num_threads));
    EXPECT_THAT(init_count.load(), Eq(num_threads));
    EXPECT_THAT(cleanup_count.load(), Eq(0));
    EXPECT_THAT(destroy_count.load(), Eq(0));

    // Concurrently reset values in each thread
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tl, i, num_resets_per_thread, this]() {
            for (int j = 0; j < num_resets_per_thread; ++j) {
                tl.reset(MyData(4000 + i * 100 + j, &init_count, &destroy_count,
                                &cleanup_count));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Each thread initialized once, then reset num_resets_per_thread times.
    // Each reset constructs a new MyData and destroys the old one, calling
    // cleanup.
    EXPECT_THAT(init_count.load(),
                Eq(num_threads + num_threads * num_resets_per_thread));
    EXPECT_THAT(cleanup_count.load(), Eq(num_threads * num_resets_per_thread));
    EXPECT_THAT(destroy_count.load(), Eq(num_threads * num_resets_per_thread));
    EXPECT_THAT(tl.size(), Eq(num_threads));  // Each thread still has a value

    // Verify the final value in each thread (should be the last value set)
    std::atomic<int> verify_count{0};
    tl.forEachWithId([&](MyData& data, std::thread::id tid) {
        // Finding the original thread index from tid is tricky.
        // Let's just verify the value is within the expected range for resets.
        EXPECT_GE(data.value, 4000);
        EXPECT_LT(data.value, 4000 + num_threads * 100 + num_resets_per_thread);
        verify_count++;
    });
    EXPECT_THAT(verify_count.load(), Eq(num_threads));
}

// Test thread safety of concurrent clearCurrentThread
TEST_F(ThreadLocalTest, ThreadSafety_ConcurrentClearCurrentThread) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(5000, &init_count, &destroy_count, &cleanup_count);
        }),
        my_cleanup_fn());

    const int num_threads = 10;
    const int num_clears_per_thread = 50;
    std::vector<std::thread> threads;

    // Initialize values in all threads first
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tl]() { (void)tl.get(); });
    }
    for (auto& t : threads) {
        t.join();
    }
    threads.clear();

    EXPECT_THAT(tl.size(), Eq(num_threads));
    EXPECT_THAT(init_count.load(), Eq(num_threads));
    EXPECT_THAT(cleanup_count.load(), Eq(0));
    EXPECT_THAT(destroy_count.load(), Eq(0));

    // Concurrently clear and re-get in each thread
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tl, num_clears_per_thread]() {
            for (int j = 0; j < num_clears_per_thread; ++j) {
                tl.clearCurrentThread();
                EXPECT_FALSE(tl.hasValue());
                (void)tl.get();  // Re-initialize
                EXPECT_TRUE(tl.hasValue());
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Each thread initialized once, then cleared and re-initialized
    // num_clears_per_thread times.
    EXPECT_THAT(init_count.load(),
                Eq(num_threads + num_threads * num_clears_per_thread));
    EXPECT_THAT(cleanup_count.load(), Eq(num_threads * num_clears_per_thread));
    EXPECT_THAT(destroy_count.load(), Eq(num_threads * num_clears_per_thread));
    EXPECT_THAT(tl.size(),
                Eq(num_threads));  // Each thread ends up with a value
}

// Test thread safety of concurrent clear
TEST_F(ThreadLocalTest, ThreadSafety_ConcurrentClear) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(6000, &init_count, &destroy_count, &cleanup_count);
        }),
        my_cleanup_fn());

    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> clear_call_count{0};

    // Initialize values in all threads first
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tl]() { (void)tl.get(); });
    }
    for (auto& t : threads) {
        t.join();
    }
    threads.clear();

    EXPECT_THAT(tl.size(), Eq(num_threads));
    EXPECT_THAT(init_count.load(), Eq(num_threads));
    EXPECT_THAT(cleanup_count.load(), Eq(0));
    EXPECT_THAT(destroy_count.load(), Eq(0));

    // Concurrently call clear from multiple threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tl, &clear_call_count]() {
            tl.clear();
            clear_call_count++;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All clear calls should succeed and eventually empty the map.
    // The total cleanup/destroy count should reflect the initial number of
    // values.
    EXPECT_THAT(clear_call_count.load(), Eq(num_threads));
    EXPECT_THAT(tl.size(), Eq(0));
    EXPECT_THAT(cleanup_count.load(),
                Eq(num_threads));  // All initial values cleaned up
    EXPECT_THAT(destroy_count.load(),
                Eq(num_threads));  // All initial values destroyed

    // Getting again should re-initialize for the current thread
    (void)tl.get();
    EXPECT_TRUE(tl.hasValue());
    EXPECT_THAT(tl.size(), Eq(1));
    EXPECT_THAT(init_count.load(), Eq(num_threads + 1));
}

// Test thread safety of concurrent forEach
TEST_F(ThreadLocalTest, ThreadSafety_ConcurrentForEach) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(7000, &init_count, &destroy_count, &cleanup_count);
        }));

    const int num_threads = 10;
    const int num_iterations_per_thread = 50;
    std::vector<std::thread> threads;
    std::atomic<int> total_iteration_count{0};

    // Initialize values in all threads first
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tl]() { (void)tl.get(); });
    }
    for (auto& t : threads) {
        t.join();
    }
    threads.clear();

    EXPECT_THAT(tl.size(), Eq(num_threads));

    // Concurrently call forEach from multiple threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(
            [&tl, &total_iteration_count, num_iterations_per_thread]() {
                for (int j = 0; j < num_iterations_per_thread; ++j) {
                    tl.forEach([&](MyData& data) {
                        // Just access the data, don't modify
                        (void)data.value;
                        total_iteration_count++;
                    });
                }
            });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Each of the num_threads calling forEach num_iterations_per_thread times.
    // Each forEach iterates over the num_threads values currently stored.
    EXPECT_THAT(total_iteration_count.load(),
                Eq(num_threads * num_iterations_per_thread * num_threads));
}

// Test thread safety of concurrent update
TEST_F(ThreadLocalTest, ThreadSafety_ConcurrentUpdate) {
    // Explicitly cast lambda to InitializerFn
    ThreadLocal<MyData> tl(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(8000, &init_count, &destroy_count, &cleanup_count);
        }),
        my_cleanup_fn());

    const int num_threads = 10;
    const int num_updates_per_thread = 100;
    std::vector<std::thread> threads;

    // Initialize values in all threads first
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tl]() { (void)tl.get(); });
    }
    for (auto& t : threads) {
        t.join();
    }
    threads.clear();

    EXPECT_THAT(tl.size(), Eq(num_threads));
    EXPECT_THAT(init_count.load(), Eq(num_threads));

    // Concurrently update values in each thread
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tl, num_updates_per_thread]() {
            for (int j = 0; j < num_updates_per_thread; ++j) {
                bool updated = tl.update([](MyData& data) {
                    data.value++;  // Increment the value
                });
                EXPECT_TRUE(updated);  // Update should succeed as value exists
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Each thread initialized its value to 8000, then incremented it 100 times.
    // The final value in each thread should be 8000 + 100 = 8100.
    std::atomic<int> verify_count{0};
    tl.forEach([&](MyData& data) {
        EXPECT_THAT(data.value, Eq(8100));
        verify_count++;
    });
    EXPECT_THAT(verify_count.load(), Eq(num_threads));
}

// Test destructor calls cleanup for remaining values
TEST_F(ThreadLocalTest, Destructor_CallsCleanupForAllRemaining) {
    // Use a raw pointer to control the lifetime of ThreadLocal
    // Explicitly cast lambda to InitializerFn
    auto* tl_ptr = new ThreadLocal<MyData>(
        static_cast<ThreadLocal<MyData>::InitializerFn>([&]() {
            return MyData(9000, &init_count, &destroy_count, &cleanup_count);
        }),
        my_cleanup_fn());

    const int num_threads = 5;
    std::vector<std::thread> threads;

    // Initialize values in multiple threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([tl_ptr]() { (void)tl_ptr->get(); });
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_THAT(tl_ptr->size(), Eq(num_threads));
    EXPECT_THAT(init_count.load(), Eq(num_threads));
    EXPECT_THAT(cleanup_count.load(), Eq(0));
    EXPECT_THAT(destroy_count.load(), Eq(0));

    // Delete the ThreadLocal object
    delete tl_ptr;

    // The destructor should have iterated through all remaining values and
    // called cleanup
    EXPECT_THAT(cleanup_count.load(), Eq(num_threads));
    EXPECT_THAT(destroy_count.load(),
                Eq(num_threads));  // Values destroyed after cleanup
}

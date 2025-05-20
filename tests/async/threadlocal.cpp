#include <gtest/gtest.h>

#include <atomic>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "atom/async/threadlocal.hpp"

namespace atom::async::test {

// Simple counter class for testing cleanup functions
class Counter {
public:
    Counter() = default;
    explicit Counter(int value) : value_(value) {}

    int value() const { return value_; }
    void increment() { ++value_; }
    void decrement() { --value_; }

    // For testing equality comparison
    bool operator==(const Counter& other) const {
        return value_ == other.value();
    }

    // For testing update functions
    Counter operator+(const Counter& other) const {
        return Counter(value_ + other.value());
    }

private:
    int value_ = 0;
};

// Global counter to track number of cleanup calls
std::atomic<int> g_cleanup_counter{0};

class ThreadLocalTest : public ::testing::Test {
protected:
    void SetUp() override { g_cleanup_counter.store(0); }

    static void cleanup_function(Counter& c) { g_cleanup_counter.fetch_add(1); }
};

// Test default constructor with no initializer
TEST_F(ThreadLocalTest, DefaultConstructor) {
    ThreadLocal<int> tl;
    EXPECT_FALSE(tl.hasValue());
    EXPECT_THROW(tl.get(), ThreadLocalException);
    EXPECT_TRUE(tl.empty());
    EXPECT_EQ(tl.size(), 0);
}

// Test initializer function constructor - 使用显式类型转换解决歧义
TEST_F(ThreadLocalTest, InitializerConstructor) {
    // 显式指定构造函数接受的是 InitializerFn 类型
    ThreadLocal<int> tl(std::function<int()>([]() { return 42; }));
    EXPECT_FALSE(tl.hasValue());  // Value not yet initialized
    EXPECT_EQ(tl.get(), 42);      // Should initialize value
    EXPECT_TRUE(tl.hasValue());   // Value now initialized
    EXPECT_FALSE(tl.empty());
    EXPECT_EQ(tl.size(), 1);
}

// Test default value constructor
TEST_F(ThreadLocalTest, DefaultValueConstructor) {
    ThreadLocal<std::string> tl(std::string("default"));
    EXPECT_EQ(tl.get(), "default");
    EXPECT_TRUE(tl.hasValue());
}

// Test conditional initializer - success case
TEST_F(ThreadLocalTest, ConditionalInitializerSuccess) {
    // 使用显式类型标注
    std::function<std::optional<int>()> conditional_init =
        []() -> std::optional<int> { return 100; };

    ThreadLocal<int> tl(conditional_init);
    EXPECT_EQ(tl.get(), 100);
    EXPECT_TRUE(tl.hasValue());
}

// Test conditional initializer - failure case
TEST_F(ThreadLocalTest, ConditionalInitializerFailure) {
    std::function<std::optional<int>()> conditional_init =
        []() -> std::optional<int> { return std::nullopt; };

    ThreadLocal<int> tl(conditional_init);
    EXPECT_THROW(tl.get(), ThreadLocalException);
    EXPECT_FALSE(tl.hasValue());
}

// Test thread ID initializer
TEST_F(ThreadLocalTest, ThreadIdInitializer) {
    ThreadLocal<std::string> tl(
        std::function<std::string(std::thread::id)>([](std::thread::id tid) {
            std::stringstream ss;
            ss << "Thread ID: " << tid;
            return ss.str();
        }));

    std::string value = tl.get();
    EXPECT_TRUE(value.find("Thread ID:") != std::string::npos);
    EXPECT_TRUE(tl.hasValue());
}

// Test reset method
TEST_F(ThreadLocalTest, Reset) {
    ThreadLocal<int> tl(std::function<int()>([]() { return 42; }));

    // Initialize the value
    EXPECT_EQ(tl.get(), 42);

    // Reset with new value
    tl.reset(100);
    EXPECT_EQ(tl.get(), 100);

    // Reset with default value
    tl.reset();
    EXPECT_EQ(tl.get(), 0);
}

// Test cleanup function
TEST_F(ThreadLocalTest, CleanupFunction) {
    {
        // 明确指定初始化函数类型和清理函数类型
        std::function<Counter()> init_func = []() { return Counter(1); };
        ThreadLocal<Counter> tl(init_func, cleanup_function);

        // Initialize the value
        tl.get();
        EXPECT_EQ(g_cleanup_counter.load(), 0);

        // Reset should trigger cleanup
        tl.reset(Counter(2));
        EXPECT_EQ(g_cleanup_counter.load(), 1);

        // Destructor should trigger cleanup
    }
    EXPECT_EQ(g_cleanup_counter.load(), 2);
}

// Test tryGet method
TEST_F(ThreadLocalTest, TryGet) {
    ThreadLocal<int> tl(std::function<int()>([]() { return 42; }));

    // Value not yet initialized
    {
        auto opt_value = tl.tryGet();
        EXPECT_FALSE(opt_value.has_value());
    }

    // Initialize the value
    tl.get();

    // Value now available
    {
        auto opt_value = tl.tryGet();
        EXPECT_TRUE(opt_value.has_value());
        EXPECT_EQ(opt_value.value().get(), 42);
    }
}

// Test getOrCreate method
TEST_F(ThreadLocalTest, GetOrCreate) {
    ThreadLocal<int> tl;  // No initializer

    // Create value using getOrCreate
    int& value = tl.getOrCreate([]() { return 50; });
    EXPECT_EQ(value, 50);
    EXPECT_TRUE(tl.hasValue());

    // Value already exists, factory won't be called
    int& value2 = tl.getOrCreate([]() { return 999; });
    EXPECT_EQ(value2, 50);
}

// Test getWrapper method and ValueWrapper functionality
TEST_F(ThreadLocalTest, ValueWrapper) {
    ThreadLocal<Counter> tl(
        std::function<Counter()>([]() { return Counter(5); }));

    // Get wrapper
    auto wrapper = tl.getWrapper();

    // Test reference access
    Counter& counter = wrapper.get();
    EXPECT_EQ(counter.value(), 5);

    // Test member access
    EXPECT_EQ(wrapper->value(), 5);

    // Test dereference
    EXPECT_EQ((*wrapper).value(), 5);

    // Test apply
    int result = wrapper.apply([](Counter& c) {
        c.increment();
        return c.value();
    });
    EXPECT_EQ(result, 6);

    // Test transform
    Counter new_counter =
        wrapper.transform([](Counter& c) { return Counter(c.value() * 2); });
    EXPECT_EQ(new_counter.value(), 12);  // 6 * 2

    // Original value should be modified by apply but not by transform
    EXPECT_EQ(tl.get().value(), 6);
}

// Test compareAndUpdate method
TEST_F(ThreadLocalTest, CompareAndUpdate) {
    ThreadLocal<Counter> tl(
        std::function<Counter()>([]() { return Counter(10); }));

    // Initialize the value
    tl.get();

    // Successful update
    bool success = tl.compareAndUpdate(Counter(10), Counter(20));
    EXPECT_TRUE(success);
    EXPECT_EQ(tl.get().value(), 20);

    // Failed update (expected value doesn't match)
    success = tl.compareAndUpdate(Counter(10), Counter(30));
    EXPECT_FALSE(success);
    EXPECT_EQ(tl.get().value(), 20);  // Value unchanged
}

// Test update method
TEST_F(ThreadLocalTest, Update) {
    ThreadLocal<Counter> tl(
        std::function<Counter()>([]() { return Counter(15); }));

    // Initialize the value
    tl.get();

    // Update function
    bool success = tl.update([](Counter& c) {
        c.increment();
        return c;
    });

    EXPECT_TRUE(success);
    EXPECT_EQ(tl.get().value(), 16);

    // Update on uninitialized ThreadLocal should fail
    ThreadLocal<Counter> tl2;
    success = tl2.update([](Counter& c) { return c; });
    EXPECT_FALSE(success);
}

// Test forEach method
TEST_F(ThreadLocalTest, ForEach) {
    ThreadLocal<int> tl(std::function<int()>([]() { return 5; }));

    // Initialize in current thread
    tl.get();

    int sum = 0;
    tl.forEach([&sum](int& value) { sum += value; });

    EXPECT_EQ(sum, 5);

    // Test with exception in forEach lambda
    EXPECT_NO_THROW(tl.forEach(
        [](int& value) { throw std::runtime_error("Test exception"); }));
}

// Test forEachWithId method
TEST_F(ThreadLocalTest, ForEachWithId) {
    ThreadLocal<int> tl(std::function<int()>([]() { return 5; }));

    // Initialize in current thread
    tl.get();

    auto current_id = std::this_thread::get_id();
    bool found_current_thread = false;

    tl.forEachWithId([&](int& value, std::thread::id tid) {
        if (tid == current_id) {
            found_current_thread = true;
            EXPECT_EQ(value, 5);
        }
    });

    EXPECT_TRUE(found_current_thread);

    // Test with exception in forEachWithId lambda
    EXPECT_NO_THROW(tl.forEachWithId([](int& value, std::thread::id tid) {
        throw std::runtime_error("Test exception");
    }));
}

// Test findIf method
TEST_F(ThreadLocalTest, FindIf) {
    ThreadLocal<int> tl(std::function<int()>([]() { return 42; }));

    // Initialize in current thread
    tl.get();

    // Find a value that satisfies the predicate
    auto found = tl.findIf([](int& value) { return value > 40; });
    EXPECT_TRUE(found.has_value());
    EXPECT_EQ(found.value().get(), 42);

    // Nothing satisfies this predicate
    auto not_found = tl.findIf([](int& value) { return value > 100; });
    EXPECT_FALSE(not_found.has_value());
}

// Test removeIf method
TEST_F(ThreadLocalTest, RemoveIf) {
    ThreadLocal<int> tl(std::function<int()>([]() { return 42; }));

    // Initialize in current thread
    tl.get();

    // Remove values that satisfy the predicate
    std::size_t removed = tl.removeIf([](int& value) { return value > 40; });
    EXPECT_EQ(removed, 1);
    EXPECT_FALSE(tl.hasValue());
    EXPECT_TRUE(tl.empty());

    // Initialize again
    tl.get();

    // Nothing satisfies this predicate
    removed = tl.removeIf([](int& value) { return value > 100; });
    EXPECT_EQ(removed, 0);
    EXPECT_TRUE(tl.hasValue());
}

// Test clear method
TEST_F(ThreadLocalTest, Clear) {
    std::function<Counter()> init_func = []() { return Counter(1); };
    ThreadLocal<Counter> tl(init_func, cleanup_function);

    // Initialize in current thread
    tl.get();

    EXPECT_EQ(tl.size(), 1);
    EXPECT_EQ(g_cleanup_counter.load(), 0);

    // Clear all values
    tl.clear();

    EXPECT_EQ(tl.size(), 0);
    EXPECT_TRUE(tl.empty());
    EXPECT_EQ(g_cleanup_counter.load(), 1);  // Cleanup called
}

// Test clearCurrentThread method
TEST_F(ThreadLocalTest, ClearCurrentThread) {
    std::function<Counter()> init_func = []() { return Counter(1); };
    ThreadLocal<Counter> tl(init_func, cleanup_function);

    // Initialize in current thread
    tl.get();

    EXPECT_EQ(tl.size(), 1);
    EXPECT_EQ(g_cleanup_counter.load(), 0);

    // Clear current thread's value
    tl.clearCurrentThread();

    EXPECT_EQ(tl.size(), 0);
    EXPECT_FALSE(tl.hasValue());
    EXPECT_EQ(g_cleanup_counter.load(), 1);  // Cleanup called
}

// Test setCleanupFunction method
TEST_F(ThreadLocalTest, SetCleanupFunction) {
    ThreadLocal<Counter> tl(
        std::function<Counter()>([]() { return Counter(1); }));

    // Initialize in current thread
    tl.get();

    // Set cleanup function
    tl.setCleanupFunction(cleanup_function);

    // Reset should trigger cleanup
    tl.reset(Counter(2));
    EXPECT_EQ(g_cleanup_counter.load(), 1);

    // Remove cleanup function
    tl.setCleanupFunction(nullptr);

    // Reset should not trigger cleanup
    tl.reset(Counter(3));
    EXPECT_EQ(g_cleanup_counter.load(), 1);  // Unchanged
}

// Test hasValueForThread method
TEST_F(ThreadLocalTest, HasValueForThread) {
    ThreadLocal<int> tl(std::function<int()>([]() { return 42; }));

    auto current_id = std::this_thread::get_id();

    // Not yet initialized
    EXPECT_FALSE(tl.hasValueForThread(current_id));

    // Initialize in current thread
    tl.get();

    EXPECT_TRUE(tl.hasValueForThread(current_id));

    // Should return false for a non-existent thread ID
    std::thread::id non_existent_id;
    EXPECT_FALSE(tl.hasValueForThread(non_existent_id));
}

// Test multi-threading with multiple threads accessing the same ThreadLocal
TEST_F(ThreadLocalTest, MultiThreadAccess) {
    ThreadLocal<int> tl(
        std::function<int(std::thread::id)>([](std::thread::id tid) {
            // Use thread ID to generate a unique number
            std::hash<std::thread::id> hasher;
            return static_cast<int>(hasher(tid) % 1000);
        }));

    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::vector<int> thread_values(num_threads, 0);

    // Create threads that access the ThreadLocal
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&tl, i, &thread_values]() {
            // Each thread accesses its own value
            thread_values[i] = tl.get();
        });
    }

    // Join all threads
    for (auto& t : threads) {
        t.join();
    }

    // Check that all threads got a value (not necessarily unique due to hash
    // collisions)
    for (int val : thread_values) {
        EXPECT_GE(val, 0);
        EXPECT_LT(val, 1000);
    }

    // Check that ThreadLocal stores multiple values
    EXPECT_GT(tl.size(), 0);
    EXPECT_LE(tl.size(), static_cast<std::size_t>(
                             num_threads));  // May be less due to thread reuse
}

// Test exception handling in initializer
TEST_F(ThreadLocalTest, InitializerException) {
    ThreadLocal<int> tl(std::function<int()>(
        []() -> int { throw std::runtime_error("Initializer failed"); }));

    EXPECT_THROW(
        {
            try {
                tl.get();
            } catch (const ThreadLocalException& e) {
                EXPECT_EQ(e.error(), ThreadLocalError::InitializationFailed);
                throw;
            }
        },
        ThreadLocalException);
}

// Test exception handling in conditional initializer
TEST_F(ThreadLocalTest, ConditionalInitializerException) {
    ThreadLocal<int> tl(
        std::function<std::optional<int>()>([]() -> std::optional<int> {
            throw std::runtime_error("Conditional initializer failed");
        }));

    EXPECT_THROW(
        {
            try {
                tl.get();
            } catch (const ThreadLocalException& e) {
                EXPECT_EQ(e.error(), ThreadLocalError::InitializationFailed);
                throw;
            }
        },
        ThreadLocalException);
}

// Test arrow operator access
TEST_F(ThreadLocalTest, ArrowOperator) {
    ThreadLocal<std::string> tl(
        std::function<std::string()>([]() { return std::string("test"); }));

    EXPECT_EQ(tl->size(), 4);

    // Test const version
    const ThreadLocal<std::string>& const_ref = tl;
    EXPECT_EQ(const_ref->size(), 4);

    // Test with no initializer (should return nullptr)
    ThreadLocal<std::string> tl_empty;
    EXPECT_EQ(tl_empty.operator->(), nullptr);
}

// Test dereference operator
TEST_F(ThreadLocalTest, DereferenceOperator) {
    ThreadLocal<int> tl(std::function<int()>([]() { return 42; }));

    EXPECT_EQ(*tl, 42);

    // Test const version
    const ThreadLocal<int>& const_ref = tl;
    EXPECT_EQ(*const_ref, 42);

    // Test with modification
    *tl = 100;
    EXPECT_EQ(tl.get(), 100);
}

// Test getPointer methods
TEST_F(ThreadLocalTest, GetPointer) {
    ThreadLocal<int> tl(std::function<int()>([]() { return 42; }));

    // Not yet initialized
    EXPECT_EQ(tl.getPointer(), nullptr);

    // Initialize
    tl.get();

    // Now should return valid pointer
    int* ptr = tl.getPointer();
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 42);

    // Modify through pointer
    *ptr = 100;
    EXPECT_EQ(tl.get(), 100);

    // Test const version
    const ThreadLocal<int>& const_ref = tl;
    const int* const_ptr = const_ref.getPointer();
    EXPECT_NE(const_ptr, nullptr);
    EXPECT_EQ(*const_ptr, 100);
}

}  // namespace atom::async::test

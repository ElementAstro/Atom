// filepath: tests/async/generator.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>  // Required for std::tuple
#include <vector>

#include "atom/async/generator.hpp"

using namespace atom::async;
using namespace std::chrono_literals;

// Helper function to convert integers to appropriate types for testing
template <typename T>
T makeTestValue(int value) {
    if constexpr (std::is_same_v<T, std::string>) {
        return std::to_string(value);
    } else {
        return static_cast<T>(value);
    }
}

// Test fixture for Generator
template <typename T>
class GeneratorTest : public ::testing::Test {};

using GeneratorTypes = ::testing::Types<int, std::string, double>;
TYPED_TEST_SUITE(GeneratorTest, GeneratorTypes);

TYPED_TEST(GeneratorTest, EmptyGenerator) {
    auto gen_func = []() -> Generator<TypeParam> { co_return; };
    Generator<TypeParam> gen = gen_func();
    EXPECT_TRUE(gen.begin() == gen.end());
}

TYPED_TEST(GeneratorTest, SingleYield) {
    auto gen_func = []() -> Generator<TypeParam> {
        co_yield makeTestValue<TypeParam>(1);
    };
    Generator<TypeParam> gen = gen_func();
    auto it = gen.begin();
    ASSERT_FALSE(it == gen.end());
    EXPECT_EQ(*it, makeTestValue<TypeParam>(1));
    ++it;
    EXPECT_TRUE(it == gen.end());
}

TYPED_TEST(GeneratorTest, MultipleYields) {
    auto gen_func = []() -> Generator<TypeParam> {
        co_yield makeTestValue<TypeParam>(1);
        co_yield makeTestValue<TypeParam>(2);
        co_yield makeTestValue<TypeParam>(3);
    };
    Generator<TypeParam> gen = gen_func();
    std::vector<TypeParam> expected = {makeTestValue<TypeParam>(1),
                                       makeTestValue<TypeParam>(2),
                                       makeTestValue<TypeParam>(3)};
    std::vector<TypeParam> actual;
    for (const auto& val : gen) {
        actual.push_back(val);
    }
    EXPECT_EQ(actual, expected);
}

TYPED_TEST(GeneratorTest, ExceptionHandling) {
    auto gen_func = []() -> Generator<TypeParam> {
        co_yield makeTestValue<TypeParam>(1);
        throw std::runtime_error("Test Exception");
        co_yield makeTestValue<TypeParam>(2);  // Unreachable
    };
    Generator<TypeParam> gen = gen_func();
    auto it = gen.begin();
    ASSERT_FALSE(it == gen.end());
    EXPECT_EQ(*it, makeTestValue<TypeParam>(1));
    ++it;
    EXPECT_THROW(*it, std::runtime_error);
    EXPECT_TRUE(it == gen.end());  // After exception, generator should be done
}

TYPED_TEST(GeneratorTest, MoveSemantics) {
    auto gen_func = []() -> Generator<TypeParam> {
        co_yield makeTestValue<TypeParam>(10);
        co_yield makeTestValue<TypeParam>(20);
    };
    Generator<TypeParam> gen1 = gen_func();
    Generator<TypeParam> gen2 = std::move(gen1);  // Move constructor

    auto it = gen2.begin();
    ASSERT_FALSE(it == gen2.end());
    EXPECT_EQ(*it, makeTestValue<TypeParam>(10));

    Generator<TypeParam> gen3 = gen_func();
    gen2 = std::move(gen3);  // Move assignment
    it = gen2.begin();
    ASSERT_FALSE(it == gen2.end());
    EXPECT_EQ(*it, makeTestValue<TypeParam>(10));
}

// Test cases for from_range
TEST(GeneratorUtilsTest, FromRangeVector) {
    std::vector<int> data = {10, 20, 30, 40};
    auto gen = from_range(data);
    std::vector<int> actual;
    for (const auto& val : gen) {
        actual.push_back(val);
    }
    EXPECT_EQ(actual, data);
}

TEST(GeneratorUtilsTest, FromRangeInitializerList) {
    // Fix: Convert initializer list to std::vector to satisfy
    // std::ranges::input_range concept
    auto gen = from_range(std::vector<int>{1, 2, 3});
    std::vector<int> actual;
    for (const auto& val : gen) {
        actual.push_back(val);
    }
    EXPECT_EQ(actual, (std::vector<int>{1, 2, 3}));
}

TEST(GeneratorUtilsTest, FromRangeEmpty) {
    std::vector<int> data = {};
    auto gen = from_range(data);
    EXPECT_TRUE(gen.begin() == gen.end());
}

// Test cases for range
TEST(GeneratorUtilsTest, RangePositiveStep) {
    auto gen = range(0, 5);  // Default step 1
    std::vector<int> actual;
    for (const auto& val : gen) {
        actual.push_back(val);
    }
    EXPECT_EQ(actual, (std::vector<int>{0, 1, 2, 3, 4}));
}

TEST(GeneratorUtilsTest, RangePositiveStepCustom) {
    auto gen = range(0, 10, 2);
    std::vector<int> actual;
    for (const auto& val : gen) {
        actual.push_back(val);
    }
    EXPECT_EQ(actual, (std::vector<int>{0, 2, 4, 6, 8}));
}

TEST(GeneratorUtilsTest, RangeNegativeStep) {
    auto gen = range(5, 0, -1);
    std::vector<int> actual;
    for (const auto& val : gen) {
        actual.push_back(val);
    }
    EXPECT_EQ(actual, (std::vector<int>{5, 4, 3, 2, 1}));
}

TEST(GeneratorUtilsTest, RangeNegativeStepCustom) {
    auto gen = range(10, 0, -3);
    std::vector<int> actual;
    for (const auto& val : gen) {
        actual.push_back(val);
    }
    EXPECT_EQ(actual, (std::vector<int>{10, 7, 4, 1}));
}

TEST(GeneratorUtilsTest, RangeZeroStepThrows) {
    EXPECT_THROW(range(0, 5, 0), std::invalid_argument);
}

TEST(GeneratorUtilsTest, RangeEmpty) {
    auto gen = range(5, 5);
    EXPECT_TRUE(gen.begin() == gen.end());
}

// Test cases for infinite_range
TEST(GeneratorUtilsTest, InfiniteRangeBasic) {
    auto gen = infinite_range(0, 1);
    auto it = gen.begin();
    EXPECT_EQ(*it, 0);
    ++it;
    EXPECT_EQ(*it, 1);
    ++it;
    EXPECT_EQ(*it, 2);
    // Don't iterate too far, it's infinite!
}

TEST(GeneratorUtilsTest, InfiniteRangeCustomStartStep) {
    auto gen = infinite_range(10, -2);
    auto it = gen.begin();
    EXPECT_EQ(*it, 10);
    ++it;
    EXPECT_EQ(*it, 8);
    ++it;
    EXPECT_EQ(*it, 6);
}

TEST(GeneratorUtilsTest, InfiniteRangeZeroStepThrows) {
    EXPECT_THROW(infinite_range(0, 0), std::invalid_argument);
}

// Test fixture for TwoWayGenerator
// Fix: Redefine to take a single TypeParam which is a tuple
template <typename T>
class TwoWayGeneratorTest : public ::testing::Test {};

using TwoWayGeneratorTypes =
    ::testing::Types<std::tuple<int, int>, std::tuple<std::string, int>,
                     std::tuple<double, std::string>>;
TYPED_TEST_SUITE(TwoWayGeneratorTest, TwoWayGeneratorTypes);

TYPED_TEST(TwoWayGeneratorTest, BasicSendReceive) {
    // Fix: Unpack the tuple to get Yield and Receive types
    using Yield = std::tuple_element_t<0, TypeParam>;
    using Receive = std::tuple_element_t<1, TypeParam>;

    auto gen_func = []() -> TwoWayGenerator<Yield, Receive> {
        Receive r1 = co_await std::suspend_always{};
        co_yield static_cast<Yield>(r1 + static_cast<Receive>(10));
        Receive r2 = co_await std::suspend_always{};
        co_yield static_cast<Yield>(r2 * static_cast<Receive>(2));
        co_return;
    };

    TwoWayGenerator<Yield, Receive> gen = gen_func();

    EXPECT_FALSE(gen.done());
    EXPECT_EQ(gen.next(static_cast<Receive>(1)), static_cast<Yield>(11));
    EXPECT_FALSE(gen.done());
    EXPECT_EQ(gen.next(static_cast<Receive>(5)), static_cast<Yield>(10));
    EXPECT_TRUE(gen.done());  // Generator should be done after last yield
    EXPECT_THROW(gen.next(static_cast<Receive>(0)), std::logic_error);
}

TYPED_TEST(TwoWayGeneratorTest, ExceptionInCoroutine) {
    // Fix: Unpack the tuple to get Yield and Receive types
    using Yield = std::tuple_element_t<0, TypeParam>;
    using Receive = std::tuple_element_t<1, TypeParam>;

    auto gen_func = []() -> TwoWayGenerator<Yield, Receive> {
        co_yield static_cast<Yield>(1);
        throw std::runtime_error("Coroutine error");
        co_yield static_cast<Yield>(2);
    };

    TwoWayGenerator<Yield, Receive> gen = gen_func();
    EXPECT_EQ(gen.next(static_cast<Receive>(0)), static_cast<Yield>(1));
    EXPECT_THROW(gen.next(static_cast<Receive>(0)), std::runtime_error);
    EXPECT_TRUE(gen.done());
}

TYPED_TEST(TwoWayGeneratorTest, MoveSemanticsTwoWay) {
    // Fix: Unpack the tuple to get Yield and Receive types
    using Yield = std::tuple_element_t<0, TypeParam>;
    using Receive = std::tuple_element_t<1, TypeParam>;

    auto gen_func = []() -> TwoWayGenerator<Yield, Receive> {
        Receive r1 = co_await std::suspend_always{};
        co_yield static_cast<Yield>(r1 + static_cast<Receive>(1));
        co_return;
    };

    TwoWayGenerator<Yield, Receive> gen1 = gen_func();
    TwoWayGenerator<Yield, Receive> gen2 = std::move(gen1);  // Move constructor

    EXPECT_EQ(gen2.next(static_cast<Receive>(10)), static_cast<Yield>(11));
    EXPECT_TRUE(gen2.done());

    TwoWayGenerator<Yield, Receive> gen3 = gen_func();
    gen2 = std::move(gen3);  // Move assignment
    EXPECT_EQ(gen2.next(static_cast<Receive>(20)), static_cast<Yield>(21));
    EXPECT_TRUE(gen2.done());
}

// Specialization for TwoWayGenerator<Yield, void>
TEST(TwoWayGeneratorVoidReceiveTest, BasicYield) {
    auto gen_func = []() -> TwoWayGenerator<int, void> {
        co_yield 10;
        co_yield 20;
        co_return;
    };

    TwoWayGenerator<int, void> gen = gen_func();
    EXPECT_FALSE(gen.done());
    EXPECT_EQ(gen.next(), 10);
    EXPECT_FALSE(gen.done());
    EXPECT_EQ(gen.next(), 20);
    EXPECT_TRUE(gen.done());
    EXPECT_THROW(gen.next(), std::logic_error);
}

TEST(TwoWayGeneratorVoidReceiveTest, ExceptionHandling) {
    auto gen_func = []() -> TwoWayGenerator<int, void> {
        co_yield 1;
        throw std::runtime_error("Coroutine error");
        co_yield 2;
    };

    TwoWayGenerator<int, void> gen = gen_func();
    EXPECT_EQ(gen.next(), 1);
    EXPECT_THROW(gen.next(), std::runtime_error);
    EXPECT_TRUE(gen.done());
}

#ifdef ATOM_USE_BOOST_LOCKFREE
// Test fixture for ConcurrentGenerator
template <typename T>
class ConcurrentGeneratorTest : public ::testing::Test {};

using ConcurrentGeneratorTypes = ::testing::Types<int, std::string>;
TYPED_TEST_SUITE(ConcurrentGeneratorTest, ConcurrentGeneratorTypes);

TYPED_TEST(ConcurrentGeneratorTest, BasicOperation) {
    auto gen_func = []() -> Generator<TypeParam> {
        for (int i = 0; i < 5; ++i) {
            co_yield makeTestValue<TypeParam>(i);
        }
    };

    ConcurrentGenerator<TypeParam> c_gen(gen_func);
    std::vector<TypeParam> actual;
    for (int i = 0; i < 5; ++i) {
        actual.push_back(c_gen.next());
    }
    EXPECT_TRUE(c_gen.done());
    EXPECT_EQ(actual.size(), 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(actual[i], makeTestValue<TypeParam>(i));
    }
    EXPECT_THROW(c_gen.next(), std::runtime_error);  // Should throw when done
}

TYPED_TEST(ConcurrentGeneratorTest, TryNextOperation) {
    auto gen_func = []() -> Generator<TypeParam> {
        co_yield makeTestValue<TypeParam>(100);
        co_yield makeTestValue<TypeParam>(200);
    };

    ConcurrentGenerator<TypeParam> c_gen(gen_func);
    TypeParam val;
    EXPECT_TRUE(c_gen.try_next(val));
    EXPECT_EQ(val, makeTestValue<TypeParam>(100));
    EXPECT_TRUE(c_gen.try_next(val));
    EXPECT_EQ(val, makeTestValue<TypeParam>(200));
    EXPECT_FALSE(c_gen.try_next(val));  // No more values
    EXPECT_TRUE(c_gen.done());
}

TYPED_TEST(ConcurrentGeneratorTest, ConcurrentConsumption) {
    const int num_elements = 1000;
    auto gen_func = [num_elements]() -> Generator<int> {
        for (int i = 0; i < num_elements; ++i) {
            co_yield i;
        }
    };

    ConcurrentGenerator<int> c_gen(gen_func);
    std::vector<int> consumed_values;
    std::mutex mtx;

    auto consumer = [&]() {
        int val;
        while (true) {
            if (c_gen.try_next(val)) {
                std::lock_guard<std::mutex> lock(mtx);
                consumed_values.push_back(val);
            } else if (c_gen.done()) {
                break;
            } else {
                std::this_thread::yield();  // Wait for producer
            }
        }
    };

    std::vector<std::thread> consumers;
    for (int i = 0; i < 4; ++i) {
        consumers.emplace_back(consumer);
    }

    for (auto& t : consumers) {
        t.join();
    }

    EXPECT_TRUE(c_gen.done());
    std::sort(consumed_values.begin(), consumed_values.end());
    EXPECT_EQ(consumed_values.size(), num_elements);
    for (int i = 0; i < num_elements; ++i) {
        EXPECT_EQ(consumed_values[i], i);
    }
}

TYPED_TEST(ConcurrentGeneratorTest, ExceptionPropagation) {
    auto gen_func = []() -> Generator<TypeParam> {
        co_yield makeTestValue<TypeParam>(1);
        throw std::runtime_error("Producer error");
        co_yield makeTestValue<TypeParam>(2);
    };

    ConcurrentGenerator<TypeParam> c_gen(gen_func);
    EXPECT_EQ(c_gen.next(), makeTestValue<TypeParam>(1));
    EXPECT_THROW(c_gen.next(), std::runtime_error);
    EXPECT_TRUE(c_gen.done());
}

TYPED_TEST(ConcurrentGeneratorTest, MoveSemanticsConcurrent) {
    auto gen_func = []() -> Generator<TypeParam> {
        co_yield makeTestValue<TypeParam>(10);
        co_yield makeTestValue<TypeParam>(20);
    };

    ConcurrentGenerator<TypeParam> c_gen1(gen_func);
    ConcurrentGenerator<TypeParam> c_gen2 =
        std::move(c_gen1);  // Move constructor

    EXPECT_EQ(c_gen2.next(), makeTestValue<TypeParam>(10));
    EXPECT_EQ(c_gen2.next(), makeTestValue<TypeParam>(20));
    EXPECT_TRUE(c_gen2.done());

    ConcurrentGenerator<TypeParam> c_gen3(gen_func);
    c_gen2 = std::move(c_gen3);  // Move assignment
    EXPECT_EQ(c_gen2.next(), makeTestValue<TypeParam>(10));
    EXPECT_EQ(c_gen2.next(), makeTestValue<TypeParam>(20));
    EXPECT_TRUE(c_gen2.done());
}

// Test cases for make_concurrent_generator
TEST(MakeConcurrentGeneratorTest, BasicUsage) {
    auto my_generator_function = []() -> Generator<int> {
        co_yield 100;
        co_yield 200;
    };

    auto c_gen = make_concurrent_generator(my_generator_function);
    EXPECT_EQ(c_gen.next(), 100);
    EXPECT_EQ(c_gen.next(), 200);
    EXPECT_TRUE(c_gen.done());
}

// Test fixture for LockFreeTwoWayGenerator
// Fix: Redefine to take a single TypeParam which is a tuple
template <typename T>
class LockFreeTwoWayGeneratorTest : public ::testing::Test {};

using LockFreeTwoWayGeneratorTypes =
    ::testing::Types<std::tuple<int, int>, std::tuple<std::string, int>,
                     std::tuple<double, std::string>>;
TYPED_TEST_SUITE(LockFreeTwoWayGeneratorTest, LockFreeTwoWayGeneratorTypes);

TYPED_TEST(LockFreeTwoWayGeneratorTest, BasicSendReceive) {
    // Fix: Unpack the tuple to get Yield and Receive types
    using Yield = std::tuple_element_t<0, TypeParam>;
    using Receive = std::tuple_element_t<1, TypeParam>;

    auto coroutine_func = []() -> TwoWayGenerator<Yield, Receive> {
        Receive r1 = co_await std::suspend_always{};
        co_yield static_cast<Yield>(r1 + static_cast<Receive>(10));
        Receive r2 = co_await std::suspend_always{};
        co_yield static_cast<Yield>(r2 * static_cast<Receive>(2));
        co_return;
    };

    LockFreeTwoWayGenerator<Yield, Receive> gen(coroutine_func);

    EXPECT_FALSE(gen.done());
    EXPECT_EQ(gen.send(static_cast<Receive>(1)), static_cast<Yield>(11));
    EXPECT_FALSE(gen.done());
    EXPECT_EQ(gen.send(static_cast<Receive>(5)), static_cast<Yield>(10));
    EXPECT_TRUE(gen.done());
    EXPECT_THROW(gen.send(static_cast<Receive>(0)), std::runtime_error);
}

TYPED_TEST(LockFreeTwoWayGeneratorTest, ExceptionPropagation) {
    // Fix: Unpack the tuple to get Yield and Receive types
    using Yield = std::tuple_element_t<0, TypeParam>;
    using Receive = std::tuple_element_t<1, TypeParam>;

    auto coroutine_func = []() -> TwoWayGenerator<Yield, Receive> {
        co_yield static_cast<Yield>(1);
        throw std::runtime_error("Worker error");
        co_yield static_cast<Yield>(2);
    };

    LockFreeTwoWayGenerator<Yield, Receive> gen(coroutine_func);
    EXPECT_EQ(gen.send(static_cast<Receive>(0)), static_cast<Yield>(1));
    EXPECT_THROW(gen.send(static_cast<Receive>(0)), std::runtime_error);
    EXPECT_TRUE(gen.done());
}

TYPED_TEST(LockFreeTwoWayGeneratorTest, ConcurrentSendReceive) {
    using Yield = int;
    using Receive = int;

    const int num_iterations = 100;
    auto coroutine_func =
        [num_iterations]() -> TwoWayGenerator<Yield, Receive> {
        for (int i = 0; i < num_iterations; ++i) {
            Receive r = co_await std::suspend_always{};
            co_yield r * 2;
        }
        co_return;
    };

    LockFreeTwoWayGenerator<Yield, Receive> gen(coroutine_func);

    std::vector<std::future<Yield>> futures;
    for (int i = 0; i < num_iterations; ++i) {
        futures.push_back(std::async(std::launch::async,
                                     [&gen, i]() { return gen.send(i); }));
    }

    std::vector<Yield> results;
    for (auto& f : futures) {
        results.push_back(f.get());
    }

    std::sort(results.begin(), results.end());
    for (int i = 0; i < num_iterations; ++i) {
        EXPECT_EQ(results[i], i * 2);
    }
    EXPECT_TRUE(gen.done());
}

TYPED_TEST(LockFreeTwoWayGeneratorTest, MoveSemanticsLockFreeTwoWay) {
    // Fix: Unpack the tuple to get Yield and Receive types
    using Yield = std::tuple_element_t<0, TypeParam>;
    using Receive = std::tuple_element_t<1, TypeParam>;

    auto coroutine_func = []() -> TwoWayGenerator<Yield, Receive> {
        Receive r = co_await std::suspend_always{};
        co_yield static_cast<Yield>(r + static_cast<Receive>(1));
        co_return;
    };

    LockFreeTwoWayGenerator<Yield, Receive> gen1(coroutine_func);
    LockFreeTwoWayGenerator<Yield, Receive> gen2 =
        std::move(gen1);  // Move constructor

    EXPECT_EQ(gen2.send(static_cast<Receive>(10)), static_cast<Yield>(11));
    EXPECT_TRUE(gen2.done());

    LockFreeTwoWayGenerator<Yield, Receive> gen3(coroutine_func);
    gen2 = std::move(gen3);  // Move assignment
    EXPECT_EQ(gen2.send(static_cast<Receive>(20)), static_cast<Yield>(21));
    EXPECT_TRUE(gen2.done());
}

// Specialization for LockFreeTwoWayGenerator<Yield, void>
TEST(LockFreeTwoWayGeneratorVoidReceiveTest, BasicNext) {
    auto coroutine_func = []() -> TwoWayGenerator<int, void> {
        co_yield 10;
        co_yield 20;
        co_return;
    };

    LockFreeTwoWayGenerator<int, void> gen(coroutine_func);
    EXPECT_FALSE(gen.done());
    EXPECT_EQ(gen.next(), 10);
    EXPECT_FALSE(gen.done());
    EXPECT_EQ(gen.next(), 20);
    EXPECT_TRUE(gen.done());
    EXPECT_THROW(gen.next(), std::runtime_error);
}

TEST(LockFreeTwoWayGeneratorVoidReceiveTest, ExceptionPropagation) {
    auto coroutine_func = []() -> TwoWayGenerator<int, void> {
        co_yield 1;
        throw std::runtime_error("Worker error");
        co_yield 2;
    };

    LockFreeTwoWayGenerator<int, void> gen(coroutine_func);
    EXPECT_EQ(gen.next(), 1);
    EXPECT_THROW(gen.next(), std::runtime_error);
    EXPECT_TRUE(gen.done());
}

TEST(LockFreeTwoWayGeneratorVoidReceiveTest, ConcurrentNext) {
    auto coroutine_func = []() -> TwoWayGenerator<int, void> {
        for (int i = 0; i < 1000; ++i) {
            co_yield i;
        }
        co_return;
    };

    LockFreeTwoWayGenerator<int, void> gen(coroutine_func);

    std::vector<std::future<int>> futures;
    for (int i = 0; i < 1000; ++i) {
        futures.push_back(
            std::async(std::launch::async, [&gen]() { return gen.next(); }));
    }

    std::vector<int> results;
    for (auto& f : futures) {
        results.push_back(f.get());
    }

    std::sort(results.begin(), results.end());
    for (int i = 0; i < 1000; ++i) {
        EXPECT_EQ(results[i], i);
    }
    EXPECT_TRUE(gen.done());
}
#endif  // ATOM_USE_BOOST_LOCKFREE

#ifdef ATOM_USE_BOOST_LOCKS
// Test fixture for ThreadSafeGenerator
template <typename T>
class ThreadSafeGeneratorTest : public ::testing::Test {};

using ThreadSafeGeneratorTypes = ::testing::Types<int, std::string>;
TYPED_TEST_SUITE(ThreadSafeGeneratorTest, ThreadSafeGeneratorTypes);

TYPED_TEST(ThreadSafeGeneratorTest, BasicOperation) {
    auto gen_func = []() -> Generator<TypeParam> {
        for (int i = 0; i < 5; ++i) {
            co_yield static_cast<TypeParam>(i);
        }
    };

    ThreadSafeGenerator<TypeParam> ts_gen(gen_func());
    std::vector<TypeParam> actual;
    for (const auto& val : ts_gen) {
        actual.push_back(val);
    }
    EXPECT_EQ(actual.size(), 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(actual[i], static_cast<TypeParam>(i));
    }
}

TYPED_TEST(ThreadSafeGeneratorTest, ConcurrentIteration) {
    const int num_elements = 1000;
    auto gen_func = [num_elements]() -> Generator<int> {
        for (int i = 0; i < num_elements; ++i) {
            co_yield i;
        }
    };

    ThreadSafeGenerator<int> ts_gen(gen_func());
    std::vector<int> consumed_values;
    std::mutex mtx;

    auto consumer = [&]() {
        for (auto it = ts_gen.begin(); it != ts_gen.end(); ++it) {
            std::lock_guard<std::mutex> lock(mtx);
            consumed_values.push_back(*it);
        }
    };

    std::vector<std::thread> consumers;
    for (int i = 0; i < 4; ++i) {
        consumers.emplace_back(consumer);
    }

    for (auto& t : consumers) {
        t.join();
    }

    std::sort(consumed_values.begin(), consumed_values.end());
    EXPECT_EQ(consumed_values.size(), num_elements);
    for (int i = 0; i < num_elements; ++i) {
        EXPECT_EQ(consumed_values[i], i);
    }
}

TYPED_TEST(ThreadSafeGeneratorTest, ExceptionPropagation) {
    auto gen_func = []() -> Generator<TypeParam> {
        co_yield static_cast<TypeParam>(1);
        throw std::runtime_error("Producer error");
        co_yield static_cast<TypeParam>(2);
    };

    ThreadSafeGenerator<TypeParam> ts_gen(gen_func());
    auto it = ts_gen.begin();
    EXPECT_EQ(*it, static_cast<TypeParam>(1));
    ++it;
    EXPECT_THROW(*it, std::runtime_error);
    EXPECT_TRUE(it == ts_gen.end());
}

TYPED_TEST(ThreadSafeGeneratorTest, MoveSemanticsThreadSafe) {
    auto gen_func = []() -> Generator<TypeParam> {
        co_yield static_cast<TypeParam>(10);
        co_yield static_cast<TypeParam>(20);
    };

    ThreadSafeGenerator<TypeParam> ts_gen1(gen_func());
    ThreadSafeGenerator<TypeParam> ts_gen2 =
        std::move(ts_gen1);  // Move constructor

    auto it = ts_gen2.begin();
    EXPECT_EQ(*it, static_cast<TypeParam>(10));
    ++it;
    EXPECT_EQ(*it, static_cast<TypeParam>(20));
    ++it;
    EXPECT_TRUE(it == ts_gen2.end());

    ThreadSafeGenerator<TypeParam> ts_gen3(gen_func());
    ts_gen2 = std::move(ts_gen3);  // Move assignment
    it = ts_gen2.begin();
    EXPECT_EQ(*it, static_cast<TypeParam>(10));
    ++it;
    EXPECT_EQ(*it, static_cast<TypeParam>(20));
    ++it;
    EXPECT_TRUE(it == ts_gen2.end());
}

#endif  // ATOM_USE_BOOST_LOCKS

// Enhanced edge case and performance tests
class GeneratorEnhancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        large_data.resize(10000);
        std::iota(large_data.begin(), large_data.end(), 1);
    }

    std::vector<int> large_data;
};

// Memory management tests
TEST_F(GeneratorEnhancedTest, LargeGeneratorMemoryUsage) {
    auto gen_func = []() -> Generator<int> {
        for (int i = 0; i < 100000; ++i) {
            co_yield i;
        }
    };

    Generator<int> gen = gen_func();
    int count = 0;

    // Process generator in chunks to test memory stability
    for ([[maybe_unused]] const auto& val : gen) {
        count++;
        if (count >= 50000)
            break;  // Process half
    }

    EXPECT_EQ(count, 50000);
}

TEST_F(GeneratorEnhancedTest, GeneratorResourceCleanup) {
    std::atomic<int> destructor_count{0};

    struct TestResource {
        std::atomic<int>* counter;
        TestResource(std::atomic<int>* c) : counter(c) {}
        ~TestResource() {
            if (counter)
                counter->fetch_add(1);
        }
    };

    {
        auto gen_func = [&destructor_count]() -> Generator<int> {
            TestResource resource(&destructor_count);
            for (int i = 0; i < 5; ++i) {
                co_yield i;
            }
        };

        Generator<int> gen = gen_func();
        auto it = gen.begin();
        ++it;  // Partially consume
        // Generator goes out of scope here
    }

    // Give some time for cleanup
    std::this_thread::sleep_for(10ms);
    EXPECT_EQ(destructor_count.load(), 1);
}

// Performance tests
TEST_F(GeneratorEnhancedTest, GeneratorPerformance) {
    auto gen_func = []() -> Generator<int> {
        for (int i = 0; i < 100000; ++i) {
            co_yield i;
        }
    };

    auto start = std::chrono::high_resolution_clock::now();

    Generator<int> gen = gen_func();
    int sum = 0;
    for (const auto& val : gen) {
        sum += val;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Sum of 0 to 99999 = 99999 * 100000 / 2 = 4999950000
    EXPECT_EQ(sum, 4999950000LL);
    EXPECT_LT(duration.count(), 1000);  // Should complete within 1 second
}

// Complex exception scenarios
TEST_F(GeneratorEnhancedTest, ExceptionInMiddleOfGeneration) {
    auto gen_func = []() -> Generator<int> {
        for (int i = 0; i < 10; ++i) {
            if (i == 5) {
                throw std::runtime_error("Exception at 5");
            }
            co_yield i;
        }
    };

    Generator<int> gen = gen_func();
    std::vector<int> collected;

    try {
        for (const auto& val : gen) {
            collected.push_back(val);
        }
        FAIL() << "Expected exception";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Exception at 5");
    }

    // Should have collected values before exception
    std::vector<int> expected = {0, 1, 2, 3, 4};
    EXPECT_EQ(collected, expected);
}

TEST_F(GeneratorEnhancedTest, ExceptionRecovery) {
    auto gen_func = []() -> Generator<int> {
        for (int i = 0; i < 5; ++i) {
            if (i == 2) {
                // Simulate error recovery by yielding error indicator
                co_yield -1;
                break;
            }
            co_yield i;
        }
    };

    Generator<int> gen = gen_func();
    std::vector<int> collected;

    for (const auto& val : gen) {
        collected.push_back(val);
    }

    std::vector<int> expected = {0, 1, -1};
    EXPECT_EQ(collected, expected);
}

// Iterator edge cases
TEST_F(GeneratorEnhancedTest, IteratorComparison) {
    auto gen_func = []() -> Generator<int> {
        co_yield 1;
        co_yield 2;
    };

    Generator<int> gen = gen_func();
    auto it1 = gen.begin();
    auto it2 = gen.begin();  // Second begin call

    // Both iterators should be equal initially
    EXPECT_TRUE(it1 == it2);

    ++it1;
    EXPECT_FALSE(it1 == it2);  // Now they should be different
}

TEST_F(GeneratorEnhancedTest, IteratorPostIncrement) {
    auto gen_func = []() -> Generator<int> {
        co_yield 10;
        co_yield 20;
        co_yield 30;
    };

    Generator<int> gen = gen_func();
    auto it = gen.begin();

    EXPECT_EQ(*it, 10);
    auto old_it = it++;
    EXPECT_EQ(*old_it, 10);  // Post-increment returns old value
    EXPECT_EQ(*it, 20);      // Iterator has advanced
}

#ifdef ATOM_USE_BOOST_LOCKFREE
// Concurrent generator tests
TEST_F(GeneratorEnhancedTest, ConcurrentGeneratorStress) {
    auto gen_func = []() -> Generator<int> {
        for (int i = 0; i < 10000; ++i) {
            co_yield i;
        }
    };

    ConcurrentGenerator<int> c_gen(gen_func);
    std::vector<int> consumed_values;
    std::mutex mtx;
    std::atomic<bool> done{false};

    // Multiple consumer threads
    std::vector<std::thread> consumers;
    for (int i = 0; i < 8; ++i) {
        consumers.emplace_back([&]() {
            while (!done.load()) {
                int val;
                if (c_gen.try_next(val)) {
                    std::lock_guard<std::mutex> lock(mtx);
                    consumed_values.push_back(val);
                } else if (c_gen.done()) {
                    break;
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    // Wait for completion
    while (!c_gen.done()) {
        std::this_thread::sleep_for(1ms);
    }
    done.store(true);

    for (auto& t : consumers) {
        t.join();
    }

    // Verify all elements were consumed
    std::sort(consumed_values.begin(), consumed_values.end());
    EXPECT_EQ(consumed_values.size(), 10000);
    for (int i = 0; i < 10000; ++i) {
        EXPECT_EQ(consumed_values[i], i);
    }
}
#endif  // ATOM_USE_BOOST_LOCKFREE

// Integration with other async components
TEST_F(GeneratorEnhancedTest, GeneratorWithFutures) {
    auto gen_func = []() -> Generator<std::future<int>> {
        for (int i = 0; i < 5; ++i) {
            auto promise = std::make_shared<std::promise<int>>();
            auto future = promise->get_future();

            // Simulate async work
            std::thread([promise, i]() {
                std::this_thread::sleep_for(10ms);
                promise->set_value(i * i);
            }).detach();

            co_yield std::move(future);
        }
    };

    Generator<std::future<int>> gen = gen_func();
    std::vector<int> results;

    for (auto& future : gen) {
        results.push_back(const_cast<std::future<int>&>(future).get());
    }

    std::vector<int> expected = {0, 1, 4, 9, 16};
    EXPECT_EQ(results, expected);
}

// Range utility edge cases
TEST_F(GeneratorEnhancedTest, RangeWithLargeStep) {
    auto gen = range(0, 100, 25);
    std::vector<int> actual;

    for (const auto& val : gen) {
        actual.push_back(val);
    }

    std::vector<int> expected = {0, 25, 50, 75};
    EXPECT_EQ(actual, expected);
}

TEST_F(GeneratorEnhancedTest, RangeWithNegativeNumbers) {
    auto gen = range(-10, -5, 2);
    std::vector<int> actual;

    for (const auto& val : gen) {
        actual.push_back(val);
    }

    std::vector<int> expected = {-10, -8, -6};
    EXPECT_EQ(actual, expected);
}

TEST_F(GeneratorEnhancedTest, InfiniteRangePartialConsumption) {
    auto gen = infinite_range(100, 3);
    std::vector<int> actual;

    auto it = gen.begin();
    for (int i = 0; i < 5; ++i) {
        actual.push_back(*it);
        ++it;
    }

    std::vector<int> expected = {100, 103, 106, 109, 112};
    EXPECT_EQ(actual, expected);
}

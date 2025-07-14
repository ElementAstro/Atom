// filepath: atom/async/test_generator.hpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <future>
#include <string>
#include <thread>
#include <tuple>  // Required for std::tuple
#include <vector>

#include "atom/async/generator.hpp"

using namespace atom::async;

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
        co_yield static_cast<TypeParam>(1);
    };
    Generator<TypeParam> gen = gen_func();
    auto it = gen.begin();
    ASSERT_FALSE(it == gen.end());
    EXPECT_EQ(*it, static_cast<TypeParam>(1));
    ++it;
    EXPECT_TRUE(it == gen.end());
}

TYPED_TEST(GeneratorTest, MultipleYields) {
    auto gen_func = []() -> Generator<TypeParam> {
        co_yield static_cast<TypeParam>(1);
        co_yield static_cast<TypeParam>(2);
        co_yield static_cast<TypeParam>(3);
    };
    Generator<TypeParam> gen = gen_func();
    std::vector<TypeParam> expected = {static_cast<TypeParam>(1),
                                       static_cast<TypeParam>(2),
                                       static_cast<TypeParam>(3)};
    std::vector<TypeParam> actual;
    for (const auto& val : gen) {
        actual.push_back(val);
    }
    EXPECT_EQ(actual, expected);
}

TYPED_TEST(GeneratorTest, ExceptionHandling) {
    auto gen_func = []() -> Generator<TypeParam> {
        co_yield static_cast<TypeParam>(1);
        throw std::runtime_error("Test Exception");
        co_yield static_cast<TypeParam>(2);  // Unreachable
    };
    Generator<TypeParam> gen = gen_func();
    auto it = gen.begin();
    ASSERT_FALSE(it == gen.end());
    EXPECT_EQ(*it, static_cast<TypeParam>(1));
    ++it;
    EXPECT_THROW(*it, std::runtime_error);
    EXPECT_TRUE(it == gen.end());  // After exception, generator should be done
}

TYPED_TEST(GeneratorTest, MoveSemantics) {
    auto gen_func = []() -> Generator<TypeParam> {
        co_yield static_cast<TypeParam>(10);
        co_yield static_cast<TypeParam>(20);
    };
    Generator<TypeParam> gen1 = gen_func();
    Generator<TypeParam> gen2 = std::move(gen1);  // Move constructor

    auto it = gen2.begin();
    ASSERT_FALSE(it == gen2.end());
    EXPECT_EQ(*it, static_cast<TypeParam>(10));

    Generator<TypeParam> gen3 = gen_func();
    gen2 = std::move(gen3);  // Move assignment
    it = gen2.begin();
    ASSERT_FALSE(it == gen2.end());
    EXPECT_EQ(*it, static_cast<TypeParam>(10));
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

// Test fixture for ConcurrentGenerator
template <typename T>
class ConcurrentGeneratorTest : public ::testing::Test {};

using ConcurrentGeneratorTypes = ::testing::Types<int, std::string>;
TYPED_TEST_SUITE(ConcurrentGeneratorTest, ConcurrentGeneratorTypes);

TYPED_TEST(ConcurrentGeneratorTest, BasicOperation) {
    auto gen_func = []() -> Generator<TypeParam> {
        for (int i = 0; i < 5; ++i) {
            co_yield static_cast<TypeParam>(i);
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
        EXPECT_EQ(actual[i], static_cast<TypeParam>(i));
    }
    EXPECT_THROW(c_gen.next(), std::runtime_error);  // Should throw when done
}

TYPED_TEST(ConcurrentGeneratorTest, TryNextOperation) {
    auto gen_func = []() -> Generator<TypeParam> {
        co_yield static_cast<TypeParam>(100);
        co_yield static_cast<TypeParam>(200);
    };

    ConcurrentGenerator<TypeParam> c_gen(gen_func);
    TypeParam val;
    EXPECT_TRUE(c_gen.try_next(val));
    EXPECT_EQ(val, static_cast<TypeParam>(100));
    EXPECT_TRUE(c_gen.try_next(val));
    EXPECT_EQ(val, static_cast<TypeParam>(200));
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
        co_yield static_cast<TypeParam>(1);
        throw std::runtime_error("Producer error");
        co_yield static_cast<TypeParam>(2);
    };

    ConcurrentGenerator<TypeParam> c_gen(gen_func);
    EXPECT_EQ(c_gen.next(), static_cast<TypeParam>(1));
    EXPECT_THROW(c_gen.next(), std::runtime_error);
    EXPECT_TRUE(c_gen.done());
}

TYPED_TEST(ConcurrentGeneratorTest, MoveSemanticsConcurrent) {
    auto gen_func = []() -> Generator<TypeParam> {
        co_yield static_cast<TypeParam>(10);
        co_yield static_cast<TypeParam>(20);
    };

    ConcurrentGenerator<TypeParam> c_gen1(gen_func);
    ConcurrentGenerator<TypeParam> c_gen2 =
        std::move(c_gen1);  // Move constructor

    EXPECT_EQ(c_gen2.next(), static_cast<TypeParam>(10));
    EXPECT_EQ(c_gen2.next(), static_cast<TypeParam>(20));
    EXPECT_TRUE(c_gen2.done());

    ConcurrentGenerator<TypeParam> c_gen3(gen_func);
    c_gen2 = std::move(c_gen3);  // Move assignment
    EXPECT_EQ(c_gen2.next(), static_cast<TypeParam>(10));
    EXPECT_EQ(c_gen2.next(), static_cast<TypeParam>(20));
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
    const int num_elements = 1000;
    auto coroutine_func = [num_elements]() -> TwoWayGenerator<int, void> {
        for (int i = 0; i < num_elements; ++i) {
            co_yield i;
        }
        co_return;
    };

    LockFreeTwoWayGenerator<int, void> gen(coroutine_func);

    std::vector<std::future<int>> futures;
    for (int i = 0; i < num_elements; ++i) {
        futures.push_back(
            std::async(std::launch::async, [&gen]() { return gen.next(); }));
    }

    std::vector<int> results;
    for (auto& f : futures) {
        results.push_back(f.get());
    }

    std::sort(results.begin(), results.end());
    for (int i = 0; i < num_elements; ++i) {
        EXPECT_EQ(results[i], i);
    }
    EXPECT_TRUE(gen.done());
}

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

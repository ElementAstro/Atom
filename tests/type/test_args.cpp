#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "atom/type/args.hpp"

using namespace atom;

class ArgsTest : public ::testing::Test {
protected:
    Args args;
};

// Basic Operations Tests
TEST_F(ArgsTest, DefaultConstructor) {
    EXPECT_TRUE(args.empty());
    EXPECT_EQ(args.size(), 0);
}

TEST_F(ArgsTest, SetAndGet) {
    args.set("int_key", 42);
    args.set("string_key", std::string("hello"));
    args.set("double_key", 3.14);

    EXPECT_EQ(args.get<int>("int_key"), 42);
    EXPECT_EQ(args.get<std::string>("string_key"), "hello");
    EXPECT_EQ(args.get<double>("double_key"), 3.14);
}

TEST_F(ArgsTest, SetAndGetMacros) {
    SET_ARGUMENT(args, test_int, 42);
    SET_ARGUMENT(args, test_string, "hello");

    EXPECT_EQ(GET_ARGUMENT(args, test_int, int), 42);
    EXPECT_EQ(GET_ARGUMENT(args, test_string, std::string), "hello");
    EXPECT_TRUE(HAS_ARGUMENT(args, test_int));
    EXPECT_FALSE(HAS_ARGUMENT(args, nonexistent));
}

TEST_F(ArgsTest, Contains) {
    args.set("key1", 42);

    EXPECT_TRUE(args.contains("key1"));
    EXPECT_FALSE(args.contains("key2"));
}

TEST_F(ArgsTest, Remove) {
    args.set("key1", 42);
    args.set("key2", "hello");

    EXPECT_EQ(args.size(), 2);

    args.remove("key1");
    EXPECT_EQ(args.size(), 1);
    EXPECT_FALSE(args.contains("key1"));
    EXPECT_TRUE(args.contains("key2"));

    REMOVE_ARGUMENT(args, key2);
    EXPECT_TRUE(args.empty());
}

TEST_F(ArgsTest, Clear) {
    args.set("key1", 42);
    args.set("key2", "hello");

    EXPECT_EQ(args.size(), 2);

    args.clear();
    EXPECT_TRUE(args.empty());
    EXPECT_EQ(args.size(), 0);
}

// Type Handling Tests
TEST_F(ArgsTest, TypeChecking) {
    args.set("int_key", 42);

    EXPECT_TRUE(args.isType<int>("int_key"));
    EXPECT_FALSE(args.isType<double>("int_key"));
    EXPECT_FALSE(args.isType<int>("nonexistent"));
}

TEST_F(ArgsTest, GetWithDefault) {
    args.set("key1", 42);

    EXPECT_EQ(args.getOr("key1", 0), 42);
    EXPECT_EQ(args.getOr("nonexistent", 100), 100);
}

TEST_F(ArgsTest, GetOptional) {
    args.set("key1", 42);

    auto result1 = args.getOptional<int>("key1");
    auto result2 = args.getOptional<int>("nonexistent");
    auto result3 = args.getOptional<std::string>("key1");

    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), 42);
    EXPECT_FALSE(result2.has_value());
    EXPECT_FALSE(result3.has_value());
}

TEST_F(ArgsTest, GetMultiple) {
    args.set("key1", 42);
    args.set("key3", 100);

    std::vector<string_view_type> keys = {"key1", "key2", "key3"};
    auto results = args.get<int>(std::span(keys));

    EXPECT_EQ(results.size(), 3);
    EXPECT_TRUE(results[0].has_value());
    EXPECT_EQ(results[0].value(), 42);
    EXPECT_FALSE(results[1].has_value());
    EXPECT_TRUE(results[2].has_value());
    EXPECT_EQ(results[2].value(), 100);
}

TEST_F(ArgsTest, OperatorAccessor) {
    args.set("key1", 42);

    EXPECT_EQ(args.operator[]<int>("key1"), 42);
    EXPECT_THROW(args.operator[]<std::string>("key1"), std::bad_any_cast);
    EXPECT_THROW(args.operator[]<int>("nonexistent"), std::out_of_range);

    // Test non-const any access
    args["new_key"] = 100;
    EXPECT_EQ(args.get<int>("new_key"), 100);

    // Test any reference modification
    args["key1"] = 200;
    EXPECT_EQ(args.get<int>("key1"), 200);
}

// Batch Operations Tests
TEST_F(ArgsTest, BatchSet) {
    std::vector<std::pair<string_view_type, int>> pairs = {
        {"key1", 1}, {"key2", 2}, {"key3", 3}};

    args.set<int>(std::span(pairs));

    EXPECT_EQ(args.size(), 3);
    EXPECT_EQ(args.get<int>("key1"), 1);
    EXPECT_EQ(args.get<int>("key2"), 2);
    EXPECT_EQ(args.get<int>("key3"), 3);
}

TEST_F(ArgsTest, InitializerListSet) {
    args.set({{"key1", 1}, {"key2", "hello"}, {"key3", 3.14}});

    EXPECT_EQ(args.size(), 3);
    EXPECT_EQ(args.get<int>("key1"), 1);
    EXPECT_EQ(args.get<const char*>("key2"), "hello");
    EXPECT_EQ(args.get<double>("key3"), 3.14);
}

// Validation Tests
TEST_F(ArgsTest, Validation) {
    // Set validator for integers to be positive
    args.setValidator("int_key", [](const any_type& val) {
        try {
#ifdef ATOM_USE_BOOST
            return boost::any_cast<int>(val) > 0;
#else
            return std::any_cast<int>(val) > 0;
#endif
        } catch (...) {
            return false;
        }
    });

    // Valid value
    args.set("int_key", 42);
    EXPECT_EQ(args.get<int>("int_key"), 42);

    // Invalid value should throw
    EXPECT_THROW(args.set("int_key", -5), std::runtime_error);

    // Key without validator should accept any value
    args.set("other_key", -10);
    EXPECT_EQ(args.get<int>("other_key"), -10);
}

// Iterator Tests
TEST_F(ArgsTest, Iterators) {
    args.set("key1", 1);
    args.set("key2", 2);

    size_t count = 0;
    for ([[maybe_unused]] const auto& [key, value] : args) {
        count++;
    }

    EXPECT_EQ(count, 2);

    // Test const iterators
    const Args& constArgs = args;
    count = 0;
    for (auto it = constArgs.begin(); it != constArgs.end(); ++it) {
        count++;
    }

    EXPECT_EQ(count, 2);

    // Test cbegin/cend
    count = 0;
    for (auto it = args.cbegin(); it != args.cend(); ++it) {
        count++;
    }

    EXPECT_EQ(count, 2);
}

TEST_F(ArgsTest, Items) {
    args.set("key1", 1);
    args.set("key2", "hello");

    auto items = args.items();
    EXPECT_EQ(items.size(), 2);
}

// Higher-order Function Tests
TEST_F(ArgsTest, ForEach) {
    args.set("key1", 1);
    args.set("key2", 2);
    args.set("key3", 3);

    int sum = 0;
    args.forEach(
        [&sum]([[maybe_unused]] string_view_type key, const any_type& value) {
#ifdef ATOM_USE_BOOST
            sum += boost::any_cast<int>(value);
#else
            sum += std::any_cast<int>(value);
#endif
        });

    EXPECT_EQ(sum, 6);
}

TEST_F(ArgsTest, Transform) {
    args.set("key1", 1);
    args.set("key2", 2);

    auto doubled = args.transform([](const any_type& value) -> any_type {
#ifdef ATOM_USE_BOOST
        return boost::any_cast<int>(value) * 2;
#else
        return std::any_cast<int>(value) * 2;
#endif
    });

    EXPECT_EQ(doubled.get<int>("key1"), 2);
    EXPECT_EQ(doubled.get<int>("key2"), 4);
}

TEST_F(ArgsTest, Filter) {
    args.set("key1", 1);
    args.set("key2", 2);
    args.set("key3", 3);

    auto evens = args.filter(
        []([[maybe_unused]] string_view_type key, const any_type& value) {
#ifdef ATOM_USE_BOOST
            return boost::any_cast<int>(value) % 2 == 0;
#else
            return std::any_cast<int>(value) % 2 == 0;
#endif
        });

    EXPECT_EQ(evens.size(), 1);
    EXPECT_TRUE(evens.contains("key2"));
    EXPECT_FALSE(evens.contains("key1"));
    EXPECT_FALSE(evens.contains("key3"));
}

// Error Handling Tests
TEST_F(ArgsTest, ErrorHandling) {
    args.set("int_key", 42);

    // Getting with wrong type should throw
    EXPECT_THROW(args.get<std::string>("int_key"), std::bad_any_cast);

    // Getting non-existent key should throw
    EXPECT_THROW(args.get<int>("nonexistent"), std::out_of_range);
}

// Thread Safety Tests (only if thread safety is enabled)
#ifdef ATOM_THREAD_SAFE
TEST_F(ArgsTest, ThreadSafety) {
    constexpr int numThreads = 10;
    constexpr int numOperations = 1000;

    std::atomic<int> successCount{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < numOperations; ++j) {
                try {
                    // Writer thread
                    if (i % 3 == 0) {
                        args.set("key" + std::to_string(j % 100), j);
                    }
                    // Reader thread
                    else if (i % 3 == 1) {
                        try {
                            args.get<int>("key" + std::to_string(j % 100));
                            successCount++;
                        } catch (const std::out_of_range&) {
                            // Key doesn't exist yet, that's okay
                        }
                    }
                    // Remover thread
                    else {
                        args.remove("key" + std::to_string(j % 100));
                    }
                } catch (...) {
                    // Ignore other exceptions
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Just ensure we didn't crash
    SUCCEED() << "Completed thread safety test with " << successCount
              << " successful reads";
}
#endif

// Complex Type Tests
struct ComplexType {
    int id;
    std::string name;

    bool operator==(const ComplexType& other) const {
        return id == other.id && name == other.name;
    }
};

TEST_F(ArgsTest, ComplexTypes) {
    ComplexType obj{42, "test"};
    args.set("complex", obj);

    auto result = args.get<ComplexType>("complex");
    EXPECT_EQ(result.id, 42);
    EXPECT_EQ(result.name, "test");
}

// Memory Pool Tests
TEST_F(ArgsTest, MemoryEfficiency) {
    // This test simply verifies that we can add many items without crashing
    // due to memory fragmentation (which the memory pool helps avoid)
    for (int i = 0; i < 10000; ++i) {
        args.set("key" + std::to_string(i), i);
    }

    EXPECT_EQ(args.size(), 10000);
}

#ifdef ATOM_USE_JSON
// JSON Serialization Tests (only if JSON support is enabled)
TEST_F(ArgsTest, JsonSerialization) {
    args.set("int_key", 42);
    args.set("string_key", "hello");
    args.set("bool_key", true);

    auto json = args.toJson();

    EXPECT_EQ(json["int_key"], 42);
    EXPECT_EQ(json["string_key"], "hello");
    EXPECT_EQ(json["bool_key"], true);

    // Test deserialization
    Args newArgs;
    newArgs.fromJson(json);

    EXPECT_EQ(newArgs.size(), 3);
    EXPECT_EQ(newArgs.get<int>("int_key"), 42);
    EXPECT_EQ(newArgs.get<std::string>("string_key"), "hello");
    EXPECT_EQ(newArgs.get<bool>("bool_key"), true);
}
#endif

// Move Semantics Tests
TEST_F(ArgsTest, MoveSemantics) {
    args.set("key1", 42);

    Args movedArgs = std::move(args);

    // Original args should be in valid but unspecified state
    // Moved args should have the data
    EXPECT_EQ(movedArgs.size(), 1);
    EXPECT_EQ(movedArgs.get<int>("key1"), 42);
}

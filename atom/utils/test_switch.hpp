#ifndef ATOM_UTILS_TEST_SWITCH_HPP
#define ATOM_UTILS_TEST_SWITCH_HPP

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "atom/utils/switch.hpp"

namespace atom::utils::test {

class StringSwitchTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Non-thread-safe switch
        switch_ = std::make_unique<StringSwitch<false, int>>();

        // Thread-safe switch
        threadSafeSwitch_ = std::make_unique<StringSwitch<true, int>>();
    }

    void TearDown() override {
        switch_.reset();
        threadSafeSwitch_.reset();
    }

    std::unique_ptr<StringSwitch<false, int>> switch_;
    std::unique_ptr<StringSwitch<true, int>> threadSafeSwitch_;
};

TEST_F(StringSwitchTest, DefaultConstruction) {
    EXPECT_TRUE(switch_->empty());
    EXPECT_EQ(switch_->size(), 0);
}

TEST_F(StringSwitchTest, RegisterAndMatch) {
    switch_->registerCase("test", [](int x) {
        return StringSwitch<false, int>::ReturnType{x + 1};
    });

    auto result = switch_->match("test", 5);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(std::get<int>(*result), 6);
}

TEST_F(StringSwitchTest, DefaultFunction) {
    switch_->setDefault(
        [](int x) { return StringSwitch<false, int>::ReturnType{x * 2}; });

    auto result = switch_->match("nonexistent", 5);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(std::get<int>(*result), 10);
}

TEST_F(StringSwitchTest, EmptyKeyRejection) {
    EXPECT_THROW(
        switch_->registerCase(
            "", [](int) { return StringSwitch<false, int>::ReturnType{0}; }),
        std::invalid_argument);
}

TEST_F(StringSwitchTest, UnregisterCase) {
    switch_->registerCase(
        "test", [](int x) { return StringSwitch<false, int>::ReturnType{x}; });
    EXPECT_TRUE(switch_->hasCase("test"));

    EXPECT_TRUE(switch_->unregisterCase("test"));
    EXPECT_FALSE(switch_->hasCase("test"));
}

TEST_F(StringSwitchTest, ClearCases) {
    switch_->registerCase(
        "test1", [](int x) { return StringSwitch<false, int>::ReturnType{x}; });
    switch_->registerCase(
        "test2", [](int x) { return StringSwitch<false, int>::ReturnType{x}; });

    EXPECT_EQ(switch_->size(), 2);
    switch_->clearCases();
    EXPECT_TRUE(switch_->empty());
}

TEST_F(StringSwitchTest, ThreadSafeConcurrentAccess) {
    constexpr int NUM_THREADS = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([this, i]() {
            std::string key = "key" + std::to_string(i);
            threadSafeSwitch_->registerCase(key, [i](int x) {
                return StringSwitch<true, int>::ReturnType{x + i};
            });
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(threadSafeSwitch_->size(), NUM_THREADS);
}

TEST_F(StringSwitchTest, MatchWithSpan) {
    switch_->registerCase("test", [](int x) {
        return StringSwitch<false, int>::ReturnType{x * 2};
    });

    std::vector<std::tuple<int>> args{{5}};
    auto result = switch_->matchWithSpan("test", std::span(args));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(std::get<int>(*result), 10);
}

TEST_F(StringSwitchTest, ParallelMatching) {
    for (int i = 0; i < 5; ++i) {
        std::string key = "key" + std::to_string(i);
        threadSafeSwitch_->registerCase(key, [i](int x) {
            return StringSwitch<true, int>::ReturnType{x + i};
        });
    }

    std::vector<std::string> keys{"key0", "key1", "key2", "key3", "key4"};
    auto results = threadSafeSwitch_->matchParallel(keys, 10);

    ASSERT_EQ(results.size(), 5);
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(results[i].has_value());
        EXPECT_EQ(std::get<int>(*results[i]), 10 + i);
    }
}

TEST_F(StringSwitchTest, ExceptionHandling) {
    switch_->registerCase("error",
                          [](int) -> StringSwitch<false, int>::ReturnType {
                              throw std::runtime_error("Test error");
                          });

    auto result = switch_->match("error", 0);
    EXPECT_FALSE(result.has_value());
}

TEST_F(StringSwitchTest, InitializerListConstruction) {
    StringSwitch<false, int> initSwitch(
        {{"key1",
          [](int x) { return StringSwitch<false, int>::ReturnType{x + 1}; }},
         {"key2",
          [](int x) { return StringSwitch<false, int>::ReturnType{x + 2}; }}});

    EXPECT_EQ(initSwitch.size(), 2);
    EXPECT_TRUE(initSwitch.hasCase("key1"));
    EXPECT_TRUE(initSwitch.hasCase("key2"));
}

TEST_F(StringSwitchTest, GetCases) {
    switch_->registerCase(
        "test1", [](int x) { return StringSwitch<false, int>::ReturnType{x}; });
    switch_->registerCase(
        "test2", [](int x) { return StringSwitch<false, int>::ReturnType{x}; });

    auto cases = switch_->getCases();
    EXPECT_EQ(cases.size(), 2);
    EXPECT_TRUE(std::find(cases.begin(), cases.end(), "test1") != cases.end());
    EXPECT_TRUE(std::find(cases.begin(), cases.end(), "test2") != cases.end());
}

TEST_F(StringSwitchTest, DifferentReturnTypes) {
    StringSwitch<false, int> variantSwitch;

    variantSwitch.registerCase(
        "int", [](int x) { return StringSwitch<false, int>::ReturnType{x}; });

    variantSwitch.registerCase("string", [](int x) {
        return StringSwitch<false, int>::ReturnType{std::to_string(x)};
    });

    variantSwitch.registerCase("monostate", [](int) {
        return StringSwitch<false, int>::ReturnType{std::monostate{}};
    });

    auto intResult = variantSwitch.match("int", 42);
    ASSERT_TRUE(intResult.has_value());
    EXPECT_EQ(std::get<int>(*intResult), 42);

    auto stringResult = variantSwitch.match("string", 42);
    ASSERT_TRUE(stringResult.has_value());
    EXPECT_EQ(std::get<std::string>(*stringResult), "42");

    auto monostateResult = variantSwitch.match("monostate", 42);
    ASSERT_TRUE(monostateResult.has_value());
    EXPECT_TRUE(std::holds_alternative<std::monostate>(*monostateResult));
}

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_SWITCH_HPP
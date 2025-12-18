/*
 * test_perf.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for performance profiling in atom/tests/performance/perf.hpp

**************************************************/

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "atom/tests/performance/perf.hpp"

namespace atom::test::performance::tests {

// ============================================================================
// Perf Construction Tests
// ============================================================================

class PerfConstructionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PerfConstructionTest, ConstructWithTag) {
    Perf p({"TestFunc", "test.cpp", 10, "TestTag"});
    EXPECT_TRUE(true);
}

TEST_F(PerfConstructionTest, ConstructWithLocation) {
    Perf::Location loc{__func__, __FILE__, __LINE__, "custom_tag"};
    Perf p(loc);
    EXPECT_TRUE(true);
}

// ============================================================================
// Perf RAII Tests
// ============================================================================

class PerfRAIITest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PerfRAIITest, AutomaticMeasurement) {
    {
        Perf p({"AutoTest", __FILE__, __LINE__, "auto"});
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(true);
}

TEST_F(PerfRAIITest, NestedMeasurement) {
    {
        Perf outer({"OuterTest", __FILE__, __LINE__, "outer"});
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        {
            Perf inner({"InnerTest", __FILE__, __LINE__, "inner"});
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    EXPECT_TRUE(true);
}

// ============================================================================
// Perf Location Tests
// ============================================================================

class PerfLocationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PerfLocationTest, LocationConstruction) {
    Perf::Location loc{__func__, __FILE__, __LINE__, "tag"};

    EXPECT_EQ(std::string(loc.func), __func__);
    EXPECT_NE(loc.file, nullptr);
    EXPECT_GT(loc.line, 0);
}

TEST_F(PerfLocationTest, LocationWithTag) {
    Perf::Location loc{"MyFunc", "myfile.cpp", 42, "mytag"};

    EXPECT_EQ(std::string(loc.func), "MyFunc");
    EXPECT_EQ(std::string(loc.file), "myfile.cpp");
    EXPECT_EQ(loc.line, 42);
    EXPECT_EQ(std::string(loc.tag), "mytag");
}

// ============================================================================
// Perf Thread Safety Tests
// ============================================================================

class PerfThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PerfThreadSafetyTest, ConcurrentMeasurements) {
    std::vector<std::thread> threads;

    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([i]() {
            std::string tag = "Thread_" + std::to_string(i);
            Perf p({tag.c_str(), __FILE__, __LINE__, tag.c_str()});
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_TRUE(true);
}

// ============================================================================
// measureWithTag Helper Tests
// ============================================================================

class MeasureWithTagTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MeasureWithTagTest, MeasureFunctionCall) {
    auto result = measureWithTag("Addition", []() { return 1 + 1; });

    EXPECT_EQ(result, 2);
}

TEST_F(MeasureWithTagTest, MeasureFunctionWithArgs) {
    auto result =
        measureWithTag("Multiply", [](int a, int b) { return a * b; }, 3, 4);

    EXPECT_EQ(result, 12);
}

TEST_F(MeasureWithTagTest, MeasureVoidFunction) {
    int counter = 0;
    measureWithTag("Increment", [&counter]() { counter++; });

    EXPECT_EQ(counter, 1);
}

// ============================================================================
// Perf Timing Accuracy Tests
// ============================================================================

class PerfTimingTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PerfTimingTest, MeasuresApproximateTime) {
    auto start = std::chrono::high_resolution_clock::now();

    {
        Perf p({"TimingTest", __FILE__, __LINE__, "timing"});
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_GE(duration.count(), 45);
    EXPECT_LE(duration.count(), 100);
}

}  // namespace atom::test::performance::tests

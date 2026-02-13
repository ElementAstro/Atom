/*
 * test_infrastructure_validation.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Validation Tests for Atom Test Infrastructure
Tests the test infrastructure itself to ensure all utilities
and helpers work correctly.

**************************************************/

#include <gtest/gtest.h>
#include "test_common.hpp"

namespace atom::test::validation {

// ============================================================================
// Test Data Generator Validation
// ============================================================================

class TestDataGeneratorValidation : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup validation tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(TestDataGeneratorValidation, RandomStringGeneration) {
    // Test random string generation
    auto str1 = TestDataGenerator::generateRandomString(10);
    auto str2 = TestDataGenerator::generateRandomString(10);

    EXPECT_EQ(str1.length(), 10);
    EXPECT_EQ(str2.length(), 10);
    EXPECT_NE(str1, str2);  // Should be different (very high probability)
}

TEST_F(TestDataGeneratorValidation, RandomBytesGeneration) {
    // Test random bytes generation
    auto bytes1 = TestDataGenerator::generateRandomBytes(100);
    auto bytes2 = TestDataGenerator::generateRandomBytes(100);

    EXPECT_EQ(bytes1.size(), 100);
    EXPECT_EQ(bytes2.size(), 100);
    EXPECT_NE(bytes1, bytes2);  // Should be different (very high probability)
}

TEST_F(TestDataGeneratorValidation, RandomIntegersGeneration) {
    // Test random integers generation
    auto ints = TestDataGenerator::generateRandomIntegers(50, 1, 100);

    EXPECT_EQ(ints.size(), 50);

    // Check range
    for (int value : ints) {
        EXPECT_GE(value, 1);
        EXPECT_LE(value, 100);
    }
}

// ============================================================================
// Performance Timer Validation
// ============================================================================

class PerformanceTimerValidation : public ::testing::Test {
protected:
    void SetUp() override { timer_ = std::make_unique<PerformanceTimer>(); }

    void TearDown() override { timer_.reset(); }

    std::unique_ptr<PerformanceTimer> timer_;
};

TEST_F(PerformanceTimerValidation, BasicTiming) {
    // Test basic timing functionality
    timer_->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    timer_->stop();

    double elapsed = timer_->getElapsedMilliseconds();
    EXPECT_GE(elapsed, 90.0);   // Allow some tolerance
    EXPECT_LE(elapsed, 150.0);  // Allow some tolerance
}

TEST_F(PerformanceTimerValidation, MultipleMeasurements) {
    // Test multiple timing measurements
    timer_->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    timer_->stop();
    double first_measurement = timer_->getElapsedMilliseconds();

    timer_->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    timer_->stop();
    double second_measurement = timer_->getElapsedMilliseconds();

    EXPECT_GT(second_measurement, first_measurement);
}

// ============================================================================
// File Manager Validation
// ============================================================================

class TestFileManagerValidation : public ::testing::Test {
protected:
    void SetUp() override {
        file_manager_ =
            std::make_unique<TestFileManager>("validation_test_dir");
    }

    void TearDown() override { file_manager_.reset(); }

    std::unique_ptr<TestFileManager> file_manager_;
};

TEST_F(TestFileManagerValidation, DirectoryCreation) {
    // Test directory creation
    std::string test_dir = file_manager_->getTestDirectory();
    EXPECT_TRUE(std::filesystem::exists(test_dir));
    EXPECT_TRUE(std::filesystem::is_directory(test_dir));
}

TEST_F(TestFileManagerValidation, FileCreation) {
    // Test file creation
    std::string content = "Test file content";
    std::string filepath = file_manager_->createTestFile("test.txt", content);

    EXPECT_TRUE(std::filesystem::exists(filepath));

    // Read back content
    std::ifstream file(filepath);
    std::string read_content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
    EXPECT_EQ(read_content, content);
}

// ============================================================================
// Thread Test Helper Validation
// ============================================================================

class ThreadTestHelperValidation : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup thread test validation
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(ThreadTestHelperValidation, ConcurrentExecution) {
    // Test concurrent execution
    std::atomic<int> counter{0};

    ThreadTestHelper::runConcurrentTest(
        [&counter]() {
            for (int i = 0; i < 100; ++i) {
                counter.fetch_add(1);
            }
        },
        4);

    EXPECT_EQ(counter.load(), 400);  // 4 threads * 100 increments
}

TEST_F(ThreadTestHelperValidation, ExceptionHandling) {
    // Test exception handling in concurrent tests
    EXPECT_THROW(
        {
            ThreadTestHelper::runConcurrentTest(
                []() { throw std::runtime_error("Test exception"); }, 2);
        },
        std::runtime_error);
}

// ============================================================================
// Test Macros Validation
// ============================================================================

class TestMacrosValidation : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup macro validation
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(TestMacrosValidation, PerformanceMacro) {
    // Test performance macro
    EXPECT_PERFORMANCE_BETTER_THAN(
        { std::this_thread::sleep_for(std::chrono::milliseconds(10)); },
        50);  // Should complete in less than 50ms
}

TEST_F(TestMacrosValidation, ThreadSafetyMacro) {
    // Test thread safety macro
    std::atomic<int> safe_counter{0};

    EXPECT_THREAD_SAFE({ safe_counter.fetch_add(1); }, 4);

    EXPECT_EQ(safe_counter.load(), 4);
}

// ============================================================================
// Base Test Fixture Validation
// ============================================================================

class AtomTestBaseValidation : public AtomTestBase {
protected:
    void SetUp() override {
        AtomTestBase::SetUp();
        // Additional setup for validation
    }

    void TearDown() override {
        // Additional cleanup for validation
        AtomTestBase::TearDown();
    }
};

TEST_F(AtomTestBaseValidation, BaseFixtureSetup) {
    // Test that base fixture is properly set up
    EXPECT_NE(file_manager_, nullptr);
    EXPECT_NE(timer_, nullptr);
}

TEST_F(AtomTestBaseValidation, FileManagerIntegration) {
    // Test file manager integration in base fixture
    std::string filepath =
        file_manager_->createTestFile("base_test.txt", "content");
    EXPECT_TRUE(std::filesystem::exists(filepath));
}

TEST_F(AtomTestBaseValidation, TimerIntegration) {
    // Test timer integration in base fixture
    timer_->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    timer_->stop();

    EXPECT_GT(timer_->getElapsedMilliseconds(), 0.0);
}

}  // namespace atom::test::validation

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

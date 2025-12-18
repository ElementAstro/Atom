/*
 * test_test_cli.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for CLI parsing in atom/tests/core/test_cli.hpp

**************************************************/

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "atom/tests/core/test_cli.hpp"

namespace atom::test::tests {

// ============================================================================
// ParseResult Tests
// ============================================================================

class ParseResultTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ParseResultTest, DefaultConstruction) {
    ParseResult result;

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.helpRequested);
    EXPECT_FALSE(result.listRequested);
    EXPECT_TRUE(result.errorMessage.empty());
}

TEST_F(ParseResultTest, SuccessResult) {
    ParseResult result;
    result.success = true;

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(static_cast<bool>(result));
}

TEST_F(ParseResultTest, FailureResult) {
    ParseResult result;
    result.success = false;
    result.errorMessage = "Invalid argument";

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(static_cast<bool>(result));
    EXPECT_EQ(result.errorMessage, "Invalid argument");
}

TEST_F(ParseResultTest, HelpRequested) {
    ParseResult result;
    result.helpRequested = true;

    EXPECT_TRUE(result.helpRequested);
}

TEST_F(ParseResultTest, ListRequested) {
    ParseResult result;
    result.listRequested = true;

    EXPECT_TRUE(result.listRequested);
}

// ============================================================================
// CLI Parser Tests
// ============================================================================

class CLIParserTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    std::vector<const char*> createArgs(
        std::initializer_list<const char*> args) {
        return std::vector<const char*>(args);
    }
};

TEST_F(CLIParserTest, NoArguments) {
    auto args = createArgs({"test_program"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    auto result = parser.parse(static_cast<int>(args.size()),
                               const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    EXPECT_TRUE(result.success);
}

TEST_F(CLIParserTest, HelpFlag) {
    auto args = createArgs({"test_program", "--help"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    auto result = parser.parse(static_cast<int>(args.size()),
                               const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    EXPECT_TRUE(result.helpRequested);
}

TEST_F(CLIParserTest, ShortHelpFlag) {
    auto args = createArgs({"test_program", "-h"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    auto result = parser.parse(static_cast<int>(args.size()),
                               const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    EXPECT_TRUE(result.helpRequested);
}

TEST_F(CLIParserTest, ListFlag) {
    auto args = createArgs({"test_program", "--list"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    auto result = parser.parse(static_cast<int>(args.size()),
                               const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    EXPECT_TRUE(result.listRequested);
}

TEST_F(CLIParserTest, VerboseFlag) {
    auto args = createArgs({"test_program", "--verbose"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    parser.parse(static_cast<int>(args.size()),
                 const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    EXPECT_TRUE(config.enableVerboseOutput);
}

TEST_F(CLIParserTest, DefaultNotVerbose) {
    auto args = createArgs({"test_program"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    parser.parse(static_cast<int>(args.size()),
                 const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    EXPECT_FALSE(config.enableVerboseOutput);
}

TEST_F(CLIParserTest, FilterOption) {
    auto args = createArgs({"test_program", "--filter", "TestSuite.*"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    parser.parse(static_cast<int>(args.size()),
                 const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    ASSERT_TRUE(config.testFilter.has_value());
    EXPECT_EQ(*config.testFilter, "TestSuite.*");
}

TEST_F(CLIParserTest, ParallelFlag) {
    auto args = createArgs({"test_program", "--parallel"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    parser.parse(static_cast<int>(args.size()),
                 const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    EXPECT_TRUE(config.enableParallel);
}

TEST_F(CLIParserTest, ThreadsOption) {
    auto args = createArgs({"test_program", "--threads", "8"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    parser.parse(static_cast<int>(args.size()),
                 const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    EXPECT_EQ(config.numThreads, 8);
}

TEST_F(CLIParserTest, RetryOption) {
    auto args = createArgs({"test_program", "--retry", "3"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    parser.parse(static_cast<int>(args.size()),
                 const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    EXPECT_EQ(config.maxRetries, 3);
}

TEST_F(CLIParserTest, ShuffleFlag) {
    auto args = createArgs({"test_program", "--shuffle"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    parser.parse(static_cast<int>(args.size()),
                 const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    EXPECT_TRUE(config.shuffleTests);
}

TEST_F(CLIParserTest, SeedOption) {
    auto args = createArgs({"test_program", "--seed", "12345"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    parser.parse(static_cast<int>(args.size()),
                 const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    ASSERT_TRUE(config.randomSeed.has_value());
    EXPECT_EQ(*config.randomSeed, 12345);
}

TEST_F(CLIParserTest, FailFastFlag) {
    auto args = createArgs({"test_program", "--fail-fast"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    parser.parse(static_cast<int>(args.size()),
                 const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    EXPECT_TRUE(config.failFast);
}

TEST_F(CLIParserTest, OutputFormatOption) {
    auto args = createArgs({"test_program", "--output-format", "json"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    parser.parse(static_cast<int>(args.size()),
                 const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    ASSERT_TRUE(config.outputFormat.has_value());
    EXPECT_EQ(*config.outputFormat, "json");
}

TEST_F(CLIParserTest, MultipleOptions) {
    auto args = createArgs({"test_program", "--parallel", "--threads", "4",
                            "--filter", "Unit*", "--verbose"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    auto result = parser.parse(static_cast<int>(args.size()),
                               const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(config.enableParallel);
    EXPECT_EQ(config.numThreads, 4);
    ASSERT_TRUE(config.testFilter.has_value());
    EXPECT_EQ(*config.testFilter, "Unit*");
    EXPECT_TRUE(config.enableVerboseOutput);
}

// ============================================================================
// CLI Parser Error Handling Tests
// ============================================================================

class CLIParserErrorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    std::vector<const char*> createArgs(
        std::initializer_list<const char*> args) {
        return std::vector<const char*>(args);
    }
};

TEST_F(CLIParserErrorTest, InvalidThreadCount) {
    auto args = createArgs({"test_program", "--threads", "-1"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    auto result = parser.parse(static_cast<int>(args.size()),
                               const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    // Should handle invalid thread count gracefully
    EXPECT_GE(config.numThreads, 1);
}

TEST_F(CLIParserErrorTest, InvalidRetryCount) {
    auto args = createArgs({"test_program", "--retry", "-1"});
    TestRunnerConfig config;
    auto parser = createDefaultParser();

    auto result = parser.parse(static_cast<int>(args.size()),
                               const_cast<char**>(args.data()));
    parser.applyToConfig(config);

    // Should handle invalid retry count gracefully
    EXPECT_GE(config.maxRetries, 0);
}

}  // namespace atom::test::tests

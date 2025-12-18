/*
 * test_result.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/secret/core/result.hpp"

namespace atom::secret::test {

class ResultTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResultTest, SuccessResult) {
    auto result = Result<int>::success(42);

    EXPECT_TRUE(result.isSuccess());
    EXPECT_FALSE(result.isError());
    EXPECT_EQ(result.value(), 42);
    EXPECT_EQ(result.errorCode(), ErrorCode::Success);
}

TEST_F(ResultTest, ErrorResult) {
    auto result = Result<int>::error(ErrorCode::InvalidArgument, "Test error");

    EXPECT_FALSE(result.isSuccess());
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidArgument);
    EXPECT_EQ(result.errorMessage(), "Test error");
}

TEST_F(ResultTest, StringResult) {
    auto result = Result<std::string>::success("Hello, World!");

    EXPECT_TRUE(result.isSuccess());
    EXPECT_EQ(result.value(), "Hello, World!");
}

TEST_F(ResultTest, VectorResult) {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto result = Result<std::vector<int>>::success(data);

    EXPECT_TRUE(result.isSuccess());
    EXPECT_EQ(result.value().size(), 5);
    EXPECT_EQ(result.value()[0], 1);
    EXPECT_EQ(result.value()[4], 5);
}

TEST_F(ResultTest, MoveSemantics) {
    std::string longString(1000, 'x');
    auto result = Result<std::string>::success(std::move(longString));

    EXPECT_TRUE(result.isSuccess());
    EXPECT_EQ(result.value().size(), 1000);
}

TEST_F(ResultTest, VoidResult) {
    auto success = Result<void>::success();
    EXPECT_TRUE(success.isSuccess());
    EXPECT_FALSE(success.isError());

    auto error = Result<void>::error(ErrorCode::Unknown, "Void error");
    EXPECT_FALSE(error.isSuccess());
    EXPECT_TRUE(error.isError());
    EXPECT_EQ(error.errorMessage(), "Void error");
}

TEST_F(ResultTest, ErrorWithDefaultMessage) {
    auto result = Result<int>::error(ErrorCode::EncryptionFailed);

    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::EncryptionFailed);
}

TEST_F(ResultTest, ValueOr) {
    auto success = Result<int>::success(42);
    auto error = Result<int>::error(ErrorCode::Unknown);

    EXPECT_EQ(success.valueOr(0), 42);
    EXPECT_EQ(error.valueOr(0), 0);
}

TEST_F(ResultTest, HasValue) {
    auto success = Result<int>::success(42);
    auto error = Result<int>::error(ErrorCode::Unknown);

    EXPECT_TRUE(success.hasValue());
    EXPECT_FALSE(error.hasValue());
}

}  // namespace atom::secret::test

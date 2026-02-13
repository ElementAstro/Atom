/*
 * test_breach_checker.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/secret/password/breach_checker.hpp"

namespace atom::secret::test {

class BreachCheckerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BreachCheckerTest, IsCommonBreachedPassword) {
    EXPECT_TRUE(BreachChecker::isCommonBreachedPassword("password"));
    EXPECT_TRUE(BreachChecker::isCommonBreachedPassword("123456"));
    EXPECT_TRUE(BreachChecker::isCommonBreachedPassword("qwerty"));
    EXPECT_TRUE(BreachChecker::isCommonBreachedPassword("iloveyou"));
}

TEST_F(BreachCheckerTest, IsCommonBreachedPasswordCaseInsensitive) {
    EXPECT_TRUE(BreachChecker::isCommonBreachedPassword("PASSWORD"));
    EXPECT_TRUE(BreachChecker::isCommonBreachedPassword("Password"));
    EXPECT_TRUE(BreachChecker::isCommonBreachedPassword("QWERTY"));
}

TEST_F(BreachCheckerTest, IsNotCommonBreachedPassword) {
    EXPECT_FALSE(BreachChecker::isCommonBreachedPassword("xK9#mP2$vL7@nQ4"));
    EXPECT_FALSE(
        BreachChecker::isCommonBreachedPassword("UniqueSecurePassword!@#$"));
}

TEST_F(BreachCheckerTest, EmptyPassword) {
    EXPECT_FALSE(BreachChecker::isCommonBreachedPassword(""));
}

TEST_F(BreachCheckerTest, GetHashPrefix) {
    auto result = BreachChecker::getHashPrefix("password");
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_EQ(result.value().length(), 5);
    // SHA-1 of "password" starts with "5BAA6"
    EXPECT_EQ(result.value(), "5BAA6");
}

TEST_F(BreachCheckerTest, GetHashSuffix) {
    auto result = BreachChecker::getHashSuffix("password");
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().length(), 35);  // 40 - 5 = 35
}

TEST_F(BreachCheckerTest, GetHashPrefixEmpty) {
    auto result = BreachChecker::getHashPrefix("");
    EXPECT_TRUE(result.isError());
}

TEST_F(BreachCheckerTest, CheckSuffixInResponse) {
    std::string apiResponse =
        "1E4C9B93F3F0682250B6CF8331B7EE68FD8:3\r\n"
        "1D2DA4053E34E76F6576ED1DA63134B5E2A:2\r\n"
        "1E4C9B93F3F0682250B6CF8331B7EE68FD9:1234567\r\n";

    auto result = BreachChecker::checkSuffixInResponse(
        "1E4C9B93F3F0682250B6CF8331B7EE68FD8", apiResponse);

    EXPECT_TRUE(result.isBreached);
    EXPECT_EQ(result.occurrences, 3);
}

TEST_F(BreachCheckerTest, CheckSuffixNotInResponse) {
    std::string apiResponse =
        "1E4C9B93F3F0682250B6CF8331B7EE68FD8:3\r\n"
        "1D2DA4053E34E76F6576ED1DA63134B5E2A:2\r\n";

    auto result = BreachChecker::checkSuffixInResponse(
        "NOTFOUND12345678901234567890ABCDE", apiResponse);

    EXPECT_FALSE(result.isBreached);
    EXPECT_EQ(result.occurrences, 0);
}

TEST_F(BreachCheckerTest, AddToCommonList) {
    size_t initialSize = BreachChecker::getCommonListSize();

    BreachChecker::addToCommonList("newbreachedpassword");
    EXPECT_EQ(BreachChecker::getCommonListSize(), initialSize + 1);

    EXPECT_TRUE(BreachChecker::isCommonBreachedPassword("newbreachedpassword"));

    // Adding same password again should not increase size
    BreachChecker::addToCommonList("newbreachedpassword");
    EXPECT_EQ(BreachChecker::getCommonListSize(), initialSize + 1);
}

TEST_F(BreachCheckerTest, ClearCommonList) {
    BreachChecker::clearCommonList();
    EXPECT_EQ(BreachChecker::getCommonListSize(), 0);

    EXPECT_FALSE(BreachChecker::isCommonBreachedPassword("password"));
}

}  // namespace atom::secret::test

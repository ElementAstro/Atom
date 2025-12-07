#include <gtest/gtest.h>
#include "atom/meta/time.hpp"

#include <regex>
#include <string>

namespace {

// Test fixture for time tests
class TimeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test getCompileTime function
TEST_F(TimeTest, GetCompileTimeFormat) {
    std::string compile_time = atom::meta::getCompileTime();

    // Check that the result is not empty
    EXPECT_FALSE(compile_time.empty());

    // Check format: YYYY-MM-DD HH:MM:SS
    // Example: 2024-03-15 14:30:45
    std::regex time_pattern(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})");
    EXPECT_TRUE(std::regex_match(compile_time, time_pattern))
        << "Compile time format should be YYYY-MM-DD HH:MM:SS, got: "
        << compile_time;
}

TEST_F(TimeTest, GetCompileTimeConsistency) {
    // Multiple calls should return the same value (compile time is constant)
    std::string time1 = atom::meta::getCompileTime();
    std::string time2 = atom::meta::getCompileTime();

    EXPECT_EQ(time1, time2) << "Compile time should be consistent across calls";
}

TEST_F(TimeTest, GetCompileTimeComponents) {
    std::string compile_time = atom::meta::getCompileTime();

    // Extract components
    ASSERT_GE(compile_time.length(), 19u) << "Compile time string too short";

    // Extract year (first 4 characters)
    std::string year = compile_time.substr(0, 4);
    int year_int = std::stoi(year);

    // Year should be reasonable (between 2020 and 2100)
    EXPECT_GE(year_int, 2020) << "Year seems too old: " << year_int;
    EXPECT_LE(year_int, 2100) << "Year seems too far in future: " << year_int;

    // Extract month (characters 5-6)
    std::string month = compile_time.substr(5, 2);
    int month_int = std::stoi(month);

    // Month should be 1-12
    EXPECT_GE(month_int, 1) << "Invalid month: " << month_int;
    EXPECT_LE(month_int, 12) << "Invalid month: " << month_int;

    // Extract day (characters 8-9)
    std::string day = compile_time.substr(8, 2);
    int day_int = std::stoi(day);

    // Day should be 1-31
    EXPECT_GE(day_int, 1) << "Invalid day: " << day_int;
    EXPECT_LE(day_int, 31) << "Invalid day: " << day_int;

    // Extract hour (characters 11-12)
    std::string hour = compile_time.substr(11, 2);
    int hour_int = std::stoi(hour);

    // Hour should be 0-23
    EXPECT_GE(hour_int, 0) << "Invalid hour: " << hour_int;
    EXPECT_LE(hour_int, 23) << "Invalid hour: " << hour_int;

    // Extract minute (characters 14-15)
    std::string minute = compile_time.substr(14, 2);
    int minute_int = std::stoi(minute);

    // Minute should be 0-59
    EXPECT_GE(minute_int, 0) << "Invalid minute: " << minute_int;
    EXPECT_LE(minute_int, 59) << "Invalid minute: " << minute_int;

    // Extract second (characters 17-18)
    std::string second = compile_time.substr(17, 2);
    int second_int = std::stoi(second);

    // Second should be 0-59
    EXPECT_GE(second_int, 0) << "Invalid second: " << second_int;
    EXPECT_LE(second_int, 59) << "Invalid second: " << second_int;
}

TEST_F(TimeTest, GetCompileTimeSeparators) {
    std::string compile_time = atom::meta::getCompileTime();

    ASSERT_GE(compile_time.length(), 19u);

    // Check separators
    EXPECT_EQ(compile_time[4], '-') << "Expected '-' separator after year";
    EXPECT_EQ(compile_time[7], '-') << "Expected '-' separator after month";
    EXPECT_EQ(compile_time[10], ' ')
        << "Expected ' ' separator between date and time";
    EXPECT_EQ(compile_time[13], ':') << "Expected ':' separator after hour";
    EXPECT_EQ(compile_time[16], ':') << "Expected ':' separator after minute";
}

TEST_F(TimeTest, GetCompileTimeNotEmpty) {
    // Ensure the function always returns a non-empty string
    std::string compile_time = atom::meta::getCompileTime();
    EXPECT_FALSE(compile_time.empty());
    EXPECT_GT(compile_time.length(), 0u);
}

TEST_F(TimeTest, GetCompileTimeLength) {
    // The format YYYY-MM-DD HH:MM:SS should be exactly 19 characters
    std::string compile_time = atom::meta::getCompileTime();
    EXPECT_EQ(compile_time.length(), 19u)
        << "Expected length 19 for format YYYY-MM-DD HH:MM:SS, got: "
        << compile_time.length();
}

TEST_F(TimeTest, GetCompileTimeDigits) {
    std::string compile_time = atom::meta::getCompileTime();

    ASSERT_GE(compile_time.length(), 19u);

    // Check that expected positions contain digits
    auto is_digit = [](char c) { return c >= '0' && c <= '9'; };

    // Year digits
    EXPECT_TRUE(is_digit(compile_time[0]));
    EXPECT_TRUE(is_digit(compile_time[1]));
    EXPECT_TRUE(is_digit(compile_time[2]));
    EXPECT_TRUE(is_digit(compile_time[3]));

    // Month digits
    EXPECT_TRUE(is_digit(compile_time[5]));
    EXPECT_TRUE(is_digit(compile_time[6]));

    // Day digits
    EXPECT_TRUE(is_digit(compile_time[8]));
    EXPECT_TRUE(is_digit(compile_time[9]));

    // Hour digits
    EXPECT_TRUE(is_digit(compile_time[11]));
    EXPECT_TRUE(is_digit(compile_time[12]));

    // Minute digits
    EXPECT_TRUE(is_digit(compile_time[14]));
    EXPECT_TRUE(is_digit(compile_time[15]));

    // Second digits
    EXPECT_TRUE(is_digit(compile_time[17]));
    EXPECT_TRUE(is_digit(compile_time[18]));
}

TEST_F(TimeTest, GetCompileTimeInline) {
    // Test that the function is inline (can be called multiple times without
    // linking errors) This is implicitly tested by calling it multiple times in
    // different tests
    std::string time1 = atom::meta::getCompileTime();
    std::string time2 = atom::meta::getCompileTime();
    std::string time3 = atom::meta::getCompileTime();

    EXPECT_EQ(time1, time2);
    EXPECT_EQ(time2, time3);
}

TEST_F(TimeTest, GetCompileTimeParseable) {
    std::string compile_time = atom::meta::getCompileTime();

    // Try to parse it back using standard library
    std::istringstream iss(compile_time);
    std::tm tm{};
    iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

    // Check that parsing succeeded
    EXPECT_FALSE(iss.fail())
        << "Failed to parse compile time: " << compile_time;

    // Verify parsed values are reasonable
    EXPECT_GE(tm.tm_year + 1900, 2020);
    EXPECT_GE(tm.tm_mon, 0);
    EXPECT_LE(tm.tm_mon, 11);
    EXPECT_GE(tm.tm_mday, 1);
    EXPECT_LE(tm.tm_mday, 31);
    EXPECT_GE(tm.tm_hour, 0);
    EXPECT_LE(tm.tm_hour, 23);
    EXPECT_GE(tm.tm_min, 0);
    EXPECT_LE(tm.tm_min, 59);
    EXPECT_GE(tm.tm_sec, 0);
    EXPECT_LE(tm.tm_sec, 59);
}

}  // anonymous namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

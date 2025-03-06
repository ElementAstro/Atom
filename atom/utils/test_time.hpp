// filepath: /home/max/Atom-1/atom/utils/test_time.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <regex>
#include <thread>

#include "atom/error/exception.hpp"
#include "time.hpp"

using namespace atom::utils;
using ::testing::HasSubstr;
using ::testing::MatchesRegex;

class TimeUtilsTest : public ::testing::Test {
protected:
    // Helper function to check if a timestamp string matches format YYYY-MM-DD
    // HH:MM:SS
    bool isTimestampFormatValid(const std::string& timestamp) {
        std::regex pattern(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})");
        return std::regex_match(timestamp, pattern);
    }

    // Helper function to check UTC format
    bool isUtcFormatValid(const std::string& utcStr) {
        std::regex pattern(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)");
        return std::regex_match(utcStr, pattern);
    }

    // Generate a timestamp 8 hours ahead of UTC for China time test
    std::string generateChinaTimeFromUtc(const std::string& utcTime) {
        std::tm tm = {};
        std::istringstream ss(utcTime);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

        tm.tm_hour += 8;
        if (tm.tm_hour >= 24) {
            tm.tm_hour -= 24;
            tm.tm_mday += 1;
            // Note: This simplistic approach doesn't handle month/year
            // boundaries properly
        }

        std::ostringstream os;
        os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return os.str();
    }
};

// Test validateTimestampFormat with valid format
TEST_F(TimeUtilsTest, ValidateTimestampFormatValid) {
    EXPECT_TRUE(validateTimestampFormat("2023-01-01 12:30:45"));
    EXPECT_TRUE(validateTimestampFormat("2023-12-31 23:59:59"));
}

// Test validateTimestampFormat with invalid format
TEST_F(TimeUtilsTest, ValidateTimestampFormatInvalid) {
    EXPECT_FALSE(validateTimestampFormat("2023/01/01 12:30:45"));
    EXPECT_FALSE(validateTimestampFormat("2023-01-01T12:30:45"));
    EXPECT_FALSE(validateTimestampFormat("23-1-1 12:30:45"));
    EXPECT_FALSE(validateTimestampFormat("2023-01-01 25:30:45"));
    EXPECT_FALSE(validateTimestampFormat("2023-01-01 12:60:45"));
    EXPECT_FALSE(validateTimestampFormat("2023-01-01 12:30:60"));
    EXPECT_FALSE(validateTimestampFormat(""));
    EXPECT_FALSE(validateTimestampFormat("invalid"));
}

// Test validateTimestampFormat with custom format
TEST_F(TimeUtilsTest, ValidateTimestampFormatCustomFormat) {
    EXPECT_TRUE(validateTimestampFormat("01/01/2023", "%m/%d/%Y"));
    EXPECT_TRUE(validateTimestampFormat("12:30:45", "%H:%M:%S"));
    EXPECT_FALSE(validateTimestampFormat("2023-01-01", "%m/%d/%Y"));
}

// Test getTimestampString
TEST_F(TimeUtilsTest, GetTimestampString) {
    std::string timestamp = getTimestampString();
    EXPECT_TRUE(isTimestampFormatValid(timestamp));
}

// Test convertToChinaTime with valid input
TEST_F(TimeUtilsTest, ConvertToChinaTimeValid) {
    std::string utcTime = "2023-01-01 12:00:00";
    std::string chinaTime = convertToChinaTime(utcTime);
    EXPECT_TRUE(isTimestampFormatValid(chinaTime));

    // China time should be UTC+8
    std::string expectedChina = generateChinaTimeFromUtc(utcTime);
    EXPECT_EQ(chinaTime, expectedChina);
}

// Test convertToChinaTime with edge case at day boundary
TEST_F(TimeUtilsTest, ConvertToChinaTimeDayBoundary) {
    std::string utcTime = "2023-01-01 23:00:00";
    std::string chinaTime = convertToChinaTime(utcTime);
    EXPECT_TRUE(isTimestampFormatValid(chinaTime));

    // At 23:00 UTC, China time should be 07:00 next day
    EXPECT_EQ(chinaTime, "2023-01-02 07:00:00");
}

// Test convertToChinaTime with invalid input
TEST_F(TimeUtilsTest, ConvertToChinaTimeInvalid) {
    EXPECT_THROW(convertToChinaTime(""), TimeConvertException);
    EXPECT_THROW(convertToChinaTime("invalid"), TimeConvertException);
    EXPECT_THROW(convertToChinaTime("2023/01/01 12:00:00"),
                 TimeConvertException);
}

// Test cache mechanism in convertToChinaTime
TEST_F(TimeUtilsTest, ConvertToChinaTimeCache) {
    // Call twice with same input to test cache
    std::string utcTime = "2023-01-01 12:00:00";

    auto startTime = std::chrono::high_resolution_clock::now();
    std::string firstResult = convertToChinaTime(utcTime);
    auto firstDuration = std::chrono::high_resolution_clock::now() - startTime;

    startTime = std::chrono::high_resolution_clock::now();
    std::string secondResult = convertToChinaTime(utcTime);
    auto secondDuration = std::chrono::high_resolution_clock::now() - startTime;

    EXPECT_EQ(firstResult, secondResult);

    // Second call should be faster or similar due to caching,
    // but we can't assert on exact timing as it may vary
}

// Test getChinaTimestampString
TEST_F(TimeUtilsTest, GetChinaTimestampString) {
    std::string chinaTimestamp = getChinaTimestampString();
    EXPECT_TRUE(isTimestampFormatValid(chinaTimestamp));

    // We can't easily verify the exact time difference in a unit test,
    // but we can check that it's a valid timestamp format
}

// Test timeStampToString with valid input
TEST_F(TimeUtilsTest, TimeStampToStringValid) {
    // 2023-01-01 00:00:00 UTC
    time_t timestamp = 1672531200;
    std::string result = timeStampToString(timestamp);
    EXPECT_TRUE(isTimestampFormatValid(result));

    // Test with custom format
    std::string customResult = timeStampToString(timestamp, "%Y/%m/%d");
    EXPECT_EQ(customResult, "2023/01/01");
}

// Test timeStampToString with invalid input
TEST_F(TimeUtilsTest, TimeStampToStringInvalid) {
    EXPECT_THROW(timeStampToString(-1), TimeConvertException);

    // This might cause undefined behavior, so we should expect an exception
    // Max time_t value
    EXPECT_THROW(timeStampToString(std::numeric_limits<time_t>::max()),
                 TimeConvertException);
}

// Test timeStampToString with empty format
TEST_F(TimeUtilsTest, TimeStampToStringEmptyFormat) {
    time_t timestamp = 1672531200;
    EXPECT_THROW(timeStampToString(timestamp, ""), TimeConvertException);
}

// Test toString with valid input
TEST_F(TimeUtilsTest, ToStringValid) {
    std::tm timeStruct = {};
    timeStruct.tm_year = 123;  // 2023 (1900 + 123)
    timeStruct.tm_mon = 0;     // January (0-based)
    timeStruct.tm_mday = 1;    // 1st
    timeStruct.tm_hour = 12;
    timeStruct.tm_min = 30;
    timeStruct.tm_sec = 45;

    std::string result = toString(timeStruct, "%Y-%m-%d %H:%M:%S");
    EXPECT_EQ(result, "2023-01-01 12:30:45");

    // Test with different format
    std::string hourMinResult = toString(timeStruct, "%H:%M");
    EXPECT_EQ(hourMinResult, "12:30");
}

// Test toString with invalid format
TEST_F(TimeUtilsTest, ToStringInvalidFormat) {
    std::tm timeStruct = {};
    EXPECT_THROW(toString(timeStruct, ""), TimeConvertException);
}

// Test getUtcTime
TEST_F(TimeUtilsTest, GetUtcTime) {
    std::string utcTime = getUtcTime();
    EXPECT_TRUE(isUtcFormatValid(utcTime));
}

// Test timestampToTime with valid input
TEST_F(TimeUtilsTest, TimestampToTimeValid) {
    // 2023-01-01 00:00:00 (1672531200 seconds since epoch)
    long long timestamp = 1672531200000;  // in milliseconds
    auto timeStruct = timestampToTime(timestamp);

    EXPECT_TRUE(timeStruct.has_value());
    EXPECT_EQ(timeStruct->tm_year, 123);  // 2023 - 1900
    EXPECT_EQ(timeStruct->tm_mon, 0);     // January (0-based)
    EXPECT_EQ(timeStruct->tm_mday, 1);    // 1st
    EXPECT_EQ(timeStruct->tm_hour, 0);
    EXPECT_EQ(timeStruct->tm_min, 0);
    EXPECT_EQ(timeStruct->tm_sec, 0);
}

// Test timestampToTime with invalid input
TEST_F(TimeUtilsTest, TimestampToTimeInvalid) {
    // Negative timestamp should return nullopt
    auto result = timestampToTime(-1);
    EXPECT_FALSE(result.has_value());

    // Excessively large timestamp might cause overflow
    auto largeResult = timestampToTime(std::numeric_limits<long long>::max());
    EXPECT_FALSE(largeResult.has_value());
}

// Test getElapsedMilliseconds
TEST_F(TimeUtilsTest, GetElapsedMilliseconds) {
    auto startTime = std::chrono::steady_clock::now();

    // Sleep for a known duration
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int64_t elapsed = getElapsedMilliseconds(startTime);

    // Allow some margin for timer inaccuracy
    EXPECT_GE(elapsed, 95);
    EXPECT_LE(elapsed, 150);
}

// Test getElapsedMilliseconds with different clock types
TEST_F(TimeUtilsTest, GetElapsedMillisecondsWithDifferentClocks) {
    // Test with system_clock
    auto startSystemTime = std::chrono::system_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int64_t systemElapsed =
        getElapsedMilliseconds<std::chrono::system_clock>(startSystemTime);

    // Test with high_resolution_clock
    auto startHighResTime = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int64_t highResElapsed =
        getElapsedMilliseconds<std::chrono::high_resolution_clock>(
            startHighResTime);

    // Both should be close to 100ms
    EXPECT_GE(systemElapsed, 95);
    EXPECT_LE(systemElapsed, 150);
    EXPECT_GE(highResElapsed, 95);
    EXPECT_LE(highResElapsed, 150);
}

// Test TimeFormattable concept
TEST_F(TimeUtilsTest, TimeFormattableConcept) {
    // Verify that std::tm satisfies TimeFormattable
    EXPECT_TRUE(TimeFormattable<std::tm>);

    // A type that doesn't satisfy TimeFormattable
    struct NotFormattable {};
    EXPECT_FALSE(TimeFormattable<NotFormattable>);
}

// Test thread-safety of caching mechanisms
TEST_F(TimeUtilsTest, ThreadSafetyCaching) {
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::vector<std::string> results(numThreads);

    // Multiple threads calling convertToChinaTime with same input
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([i, &results]() {
            results[i] = convertToChinaTime("2023-01-01 12:00:00");
        });
    }

    // Join all threads
    for (auto& t : threads) {
        if (t.joinable())
            t.join();
    }

    // All results should be identical
    for (int i = 1; i < numThreads; ++i) {
        EXPECT_EQ(results[0], results[i]);
    }
}

// Test with very large time values
TEST_F(TimeUtilsTest, VeryLargeTimeValues) {
    // Test with a date far in the future (if supported by time_t)
    if (sizeof(time_t) >= 8) {  // 64-bit time_t
        // Year 2100 timestamp (if in range)
        time_t future = 4102444800;  // 2100-01-01 00:00:00 UTC

        // This should not throw if 64-bit time_t is supported
        EXPECT_NO_THROW({
            std::string result = timeStampToString(future);
            EXPECT_TRUE(isTimestampFormatValid(result));
        });
    }
}

// Test handling of daylight saving time
TEST_F(TimeUtilsTest, DaylightSavingTimeHandling) {
    // This test is system-dependent and may not work on all platforms
    // It mainly verifies that the functions don't crash during DST transitions

    // March or November when DST typically starts/ends in many regions
    // We're not testing specific DST behavior, just ensuring functions work
    // during these times

    time_t marchTimestamp = 1678190400;  // 2023-03-07 00:00:00 UTC
    time_t novTimestamp = 1699315200;    // 2023-11-07 00:00:00 UTC

    EXPECT_NO_THROW({
        std::string marchResult = timeStampToString(marchTimestamp);
        std::string novResult = timeStampToString(novTimestamp);
        EXPECT_TRUE(isTimestampFormatValid(marchResult));
        EXPECT_TRUE(isTimestampFormatValid(novResult));
    });
}

// Test handling of time zone settings
TEST_F(TimeUtilsTest, TimeZoneHandling) {
// Store the current timezone
#ifdef _WIN32
    char tzEnv[100];
    size_t tzSize = 0;
    getenv_s(&tzSize, tzEnv, 100, "TZ");
    std::string originalTz = tzSize > 0 ? tzEnv : "";
#else
    char* tzEnv = getenv("TZ");
    std::string originalTz = tzEnv ? tzEnv : "";
#endif

    try {
// Set to UTC timezone
#ifdef _WIN32
        _putenv_s("TZ", "UTC");
#else
        setenv("TZ", "UTC", 1);
        tzset();
#endif

        // Get timestamp in UTC
        time_t testTime = 1672531200;  // 2023-01-01 00:00:00 UTC
        std::string utcResult = timeStampToString(testTime);

// Change to a different timezone
#ifdef _WIN32
        _putenv_s("TZ", "PST8PDT");
#else
        setenv("TZ", "America/Los_Angeles", 1);
        tzset();
#endif

        // Get timestamp in new timezone
        std::string pstResult = timeStampToString(testTime);

        // Results should differ due to timezone change
        // PST is UTC-8, so PST result should show a time 8 hours earlier
        EXPECT_NE(utcResult, pstResult);
    } catch (...) {
// Restore original timezone
#ifdef _WIN32
        if (!originalTz.empty()) {
            _putenv_s("TZ", originalTz.c_str());
        }
#else
        if (!originalTz.empty()) {
            setenv("TZ", originalTz.c_str(), 1);
        } else {
            unsetenv("TZ");
        }
        tzset();
#endif
        throw;
    }

// Restore original timezone
#ifdef _WIN32
    if (!originalTz.empty()) {
        _putenv_s("TZ", originalTz.c_str());
    }
#else
    if (!originalTz.empty()) {
        setenv("TZ", originalTz.c_str(), 1);
    } else {
        unsetenv("TZ");
    }
    tzset();
#endif
}

// Test timestamp formatting with milliseconds
TEST_F(TimeUtilsTest, TimestampWithMilliseconds) {
    // Check if getTimestampString includes milliseconds
    std::string timestamp = getTimestampString();

    // Format should be "YYYY-MM-DD HH:MM:SS.mmm"
    std::regex patternWithMs(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3})");
    EXPECT_TRUE(std::regex_match(timestamp, patternWithMs));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
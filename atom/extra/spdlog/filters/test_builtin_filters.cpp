// filepath: atom/extra/spdlog/filters/test_builtin_filters.cpp

#include <gtest/gtest.h>
#include <chrono>
#include <regex>
#include <string>
#include <thread>
#include <vector>
#include "../core/context.h"
#include "builtin_filters.h"

using modern_log::BuiltinFilters;
using modern_log::Level;
using modern_log::LogContext;

TEST(BuiltinFiltersTest, LevelFilterAllowsAboveMinLevel) {
    auto filter = BuiltinFilters::level_filter(Level::info);
    EXPECT_FALSE(filter("msg", Level::debug, LogContext{}));
    EXPECT_TRUE(filter("msg", Level::info, LogContext{}));
    EXPECT_TRUE(filter("msg", Level::warn, LogContext{}));
    EXPECT_TRUE(filter("msg", Level::error, LogContext{}));
}

TEST(BuiltinFiltersTest, LevelFilterWithTrace) {
    auto filter = BuiltinFilters::level_filter(Level::trace);
    EXPECT_TRUE(filter("msg", Level::trace, LogContext{}));
    EXPECT_TRUE(filter("msg", Level::debug, LogContext{}));
    EXPECT_TRUE(filter("msg", Level::info, LogContext{}));
    EXPECT_TRUE(filter("msg", Level::warn, LogContext{}));
    EXPECT_TRUE(filter("msg", Level::error, LogContext{}));
    EXPECT_TRUE(filter("msg", Level::critical, LogContext{}));
}

TEST(BuiltinFiltersTest, LevelFilterWithCritical) {
    auto filter = BuiltinFilters::level_filter(Level::critical);
    EXPECT_FALSE(filter("msg", Level::trace, LogContext{}));
    EXPECT_FALSE(filter("msg", Level::debug, LogContext{}));
    EXPECT_FALSE(filter("msg", Level::info, LogContext{}));
    EXPECT_FALSE(filter("msg", Level::warn, LogContext{}));
    EXPECT_FALSE(filter("msg", Level::error, LogContext{}));
    EXPECT_TRUE(filter("msg", Level::critical, LogContext{}));
}

TEST(BuiltinFiltersTest, RegexFilterInclude) {
    auto filter = BuiltinFilters::regex_filter(std::regex("foo.*bar"), true);
    EXPECT_TRUE(filter("foo123bar", Level::info, LogContext{}));
    EXPECT_FALSE(filter("something else", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, RegexFilterExclude) {
    auto filter = BuiltinFilters::regex_filter(std::regex("foo.*bar"), false);
    EXPECT_FALSE(filter("foo123bar", Level::info, LogContext{}));
    EXPECT_TRUE(filter("something else", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, RegexFilterCaseInsensitive) {
    auto filter = BuiltinFilters::regex_filter(
        std::regex("ERROR", std::regex_constants::icase), true);
    EXPECT_TRUE(filter("error occurred", Level::info, LogContext{}));
    EXPECT_TRUE(filter("ERROR OCCURRED", Level::info, LogContext{}));
    EXPECT_TRUE(filter("Error Occurred", Level::info, LogContext{}));
    EXPECT_FALSE(filter("warning occurred", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, RegexFilterComplexPattern) {
    auto filter = BuiltinFilters::regex_filter(
        std::regex(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})"), true);
    EXPECT_TRUE(filter("2023-12-25 10:30:45 Event occurred", Level::info,
                       LogContext{}));
    EXPECT_FALSE(
        filter("Event occurred at some time", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, RateLimitFilterZeroRate) {
    auto filter = BuiltinFilters::rate_limit_filter(0);
    EXPECT_FALSE(filter("msg", Level::info, LogContext{}));
    EXPECT_FALSE(filter("msg", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, RateLimitFilterSingleMessage) {
    auto filter = BuiltinFilters::rate_limit_filter(1);
    EXPECT_TRUE(filter("msg", Level::info, LogContext{}));
    EXPECT_FALSE(filter("msg", Level::info, LogContext{}));
    EXPECT_FALSE(filter("msg", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, RateLimitFilterResetAfterSecond) {
    auto filter = BuiltinFilters::rate_limit_filter(2);
    EXPECT_TRUE(filter("msg", Level::info, LogContext{}));
    EXPECT_TRUE(filter("msg", Level::info, LogContext{}));
    EXPECT_FALSE(filter("msg", Level::info, LogContext{}));

    // Wait for reset
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    EXPECT_TRUE(filter("msg", Level::info, LogContext{}));
    EXPECT_TRUE(filter("msg", Level::info, LogContext{}));
    EXPECT_FALSE(filter("msg", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, UserFilterEmptyAllowedList) {
    std::vector<std::string> allowed = {};
    auto filter = BuiltinFilters::user_filter(allowed);

    LogContext ctx;
    ctx.with_user("alice");
    EXPECT_FALSE(filter("msg", Level::info, ctx));

    // Empty context should still be allowed
    LogContext empty_ctx;
    EXPECT_TRUE(filter("msg", Level::info, empty_ctx));
}

TEST(BuiltinFiltersTest, UserFilterSingleUser) {
    std::vector<std::string> allowed = {"admin"};
    auto filter = BuiltinFilters::user_filter(allowed);

    LogContext admin_ctx, user_ctx;
    admin_ctx.with_user("admin");
    user_ctx.with_user("user");

    EXPECT_TRUE(filter("msg", Level::info, admin_ctx));
    EXPECT_FALSE(filter("msg", Level::info, user_ctx));
}

TEST(BuiltinFiltersTest, UserFilterCaseSensitive) {
    std::vector<std::string> allowed = {"Alice"};
    auto filter = BuiltinFilters::user_filter(allowed);

    LogContext ctx1, ctx2;
    ctx1.with_user("Alice");
    ctx2.with_user("alice");

    EXPECT_TRUE(filter("msg", Level::info, ctx1));
    EXPECT_FALSE(filter("msg", Level::info, ctx2));
}

TEST(BuiltinFiltersTest, TimeWindowFilterCurrentTime) {
    using clock = std::chrono::system_clock;
    auto now = clock::now();
    auto filter = BuiltinFilters::time_window_filter(now, now);

    // Should allow current time (edge case)
    EXPECT_TRUE(filter("msg", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, TimeWindowFilterFutureWindow) {
    using clock = std::chrono::system_clock;
    auto now = clock::now();
    auto filter = BuiltinFilters::time_window_filter(
        now + std::chrono::hours(1), now + std::chrono::hours(2));

    // Should not allow current time (future window)
    EXPECT_FALSE(filter("msg", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, TimeWindowFilterLargeWindow) {
    using clock = std::chrono::system_clock;
    auto now = clock::now();
    auto filter = BuiltinFilters::time_window_filter(
        now - std::chrono::hours(24), now + std::chrono::hours(24));

    EXPECT_TRUE(filter("msg", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, KeywordFilterEmptyKeywords) {
    std::vector<std::string> keywords = {};
    auto filter_include = BuiltinFilters::keyword_filter(keywords, true);
    auto filter_exclude = BuiltinFilters::keyword_filter(keywords, false);

    EXPECT_FALSE(filter_include("any message", Level::info, LogContext{}));
    EXPECT_TRUE(filter_exclude("any message", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, KeywordFilterSingleKeyword) {
    std::vector<std::string> keywords = {"error"};
    auto filter = BuiltinFilters::keyword_filter(keywords, true);

    EXPECT_TRUE(filter("An error occurred", Level::info, LogContext{}));
    EXPECT_TRUE(filter("error", Level::info, LogContext{}));
    EXPECT_FALSE(filter("warning message", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, KeywordFilterPartialMatch) {
    std::vector<std::string> keywords = {"test"};
    auto filter = BuiltinFilters::keyword_filter(keywords, true);

    EXPECT_TRUE(filter("testing", Level::info, LogContext{}));
    EXPECT_TRUE(filter("test", Level::info, LogContext{}));
    EXPECT_TRUE(filter("This is a test message", Level::info, LogContext{}));
    EXPECT_FALSE(filter("message", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, KeywordFilterCaseSensitive) {
    std::vector<std::string> keywords = {"Error"};
    auto filter = BuiltinFilters::keyword_filter(keywords, true);

    EXPECT_TRUE(filter("Error occurred", Level::info, LogContext{}));
    EXPECT_FALSE(filter("error occurred", Level::info, LogContext{}));
    EXPECT_FALSE(filter("ERROR OCCURRED", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, SamplingFilterRateGreaterThanOne) {
    auto filter = BuiltinFilters::sampling_filter(1.5);
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(filter("msg", Level::info, LogContext{}));
    }
}

TEST(BuiltinFiltersTest, SamplingFilterNegativeRate) {
    auto filter = BuiltinFilters::sampling_filter(-0.5);
    for (int i = 0; i < 10; ++i) {
        EXPECT_FALSE(filter("msg", Level::info, LogContext{}));
    }
}

TEST(BuiltinFiltersTest, SamplingFilterHalfRate) {
    auto filter = BuiltinFilters::sampling_filter(0.5);
    int allowed = 0;
    for (int i = 0; i < 100; ++i) {
        if (filter("msg", Level::info, LogContext{})) {
            ++allowed;
        }
    }
    // Should be approximately 50
    EXPECT_NEAR(allowed, 50, 10);
}

TEST(BuiltinFiltersTest, SamplingFilterVeryLowRate) {
    auto filter = BuiltinFilters::sampling_filter(0.01);
    int allowed = 0;
    for (int i = 0; i < 1000; ++i) {
        if (filter("msg", Level::info, LogContext{})) {
            ++allowed;
        }
    }
    // Should be approximately 10
    EXPECT_NEAR(allowed, 10, 5);
}

TEST(BuiltinFiltersTest, DuplicateFilterZeroWindow) {
    auto filter = BuiltinFilters::duplicate_filter(std::chrono::seconds(0));
    EXPECT_TRUE(filter("msg1", Level::info, LogContext{}));
    // With zero window, duplicates should be immediately allowed again
    EXPECT_TRUE(filter("msg1", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, DuplicateFilterDifferentLevels) {
    auto filter = BuiltinFilters::duplicate_filter(std::chrono::seconds(1));
    EXPECT_TRUE(filter("msg", Level::info, LogContext{}));
    EXPECT_FALSE(filter("msg", Level::error, LogContext{}));
    EXPECT_FALSE(filter("msg", Level::warn, LogContext{}));
}

TEST(BuiltinFiltersTest, DuplicateFilterDifferentContexts) {
    auto filter = BuiltinFilters::duplicate_filter(std::chrono::seconds(1));
    LogContext ctx1, ctx2;
    ctx1.with_user("alice");
    ctx2.with_user("bob");

    EXPECT_TRUE(filter("msg", Level::info, ctx1));
    // Same message with different context should still be filtered
    EXPECT_FALSE(filter("msg", Level::info, ctx2));
}

TEST(BuiltinFiltersTest, DuplicateFilterLongWindow) {
    auto filter = BuiltinFilters::duplicate_filter(std::chrono::hours(1));
    EXPECT_TRUE(filter("msg1", Level::info, LogContext{}));
    EXPECT_FALSE(filter("msg1", Level::info, LogContext{}));

    // Different message should pass
    EXPECT_TRUE(filter("msg2", Level::info, LogContext{}));
    EXPECT_FALSE(filter("msg2", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, DuplicateFilterCleanup) {
    auto filter = BuiltinFilters::duplicate_filter(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::milliseconds(100)));

    // Fill with messages
    for (int i = 0; i < 10; ++i) {
        std::string msg = "msg" + std::to_string(i);
        EXPECT_TRUE(filter(msg, Level::info, LogContext{}));
    }

    // Wait for cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // All messages should be allowed again after cleanup
    for (int i = 0; i < 10; ++i) {
        std::string msg = "msg" + std::to_string(i);
        EXPECT_TRUE(filter(msg, Level::info, LogContext{}));
    }
}

TEST(BuiltinFiltersTest, MultipleFiltersIndependence) {
    auto filter1 = BuiltinFilters::rate_limit_filter(2);
    auto filter2 = BuiltinFilters::rate_limit_filter(3);

    // Each filter should maintain its own state
    EXPECT_TRUE(filter1("msg", Level::info, LogContext{}));
    EXPECT_TRUE(filter1("msg", Level::info, LogContext{}));
    EXPECT_FALSE(filter1("msg", Level::info, LogContext{}));

    EXPECT_TRUE(filter2("msg", Level::info, LogContext{}));
    EXPECT_TRUE(filter2("msg", Level::info, LogContext{}));
    EXPECT_TRUE(filter2("msg", Level::info, LogContext{}));
    EXPECT_FALSE(filter2("msg", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, FilterFuncCopyable) {
    auto filter1 = BuiltinFilters::level_filter(Level::info);
    auto filter2 = filter1;  // Copy

    EXPECT_TRUE(filter1("msg", Level::info, LogContext{}));
    EXPECT_TRUE(filter2("msg", Level::info, LogContext{}));
    EXPECT_FALSE(filter1("msg", Level::debug, LogContext{}));
    EXPECT_FALSE(filter2("msg", Level::debug, LogContext{}));
}

TEST(BuiltinFiltersTest, RateLimitFilterAllowsUpToMaxPerSecond) {
    auto filter = BuiltinFilters::rate_limit_filter(3);
    int allowed = 0;
    for (int i = 0; i < 5; ++i) {
        if (filter("msg", Level::info, LogContext{}))
            ++allowed;
    }
    EXPECT_EQ(allowed, 3);

    // Wait for the next second and try again
    std::this_thread::sleep_for(std::chrono::seconds(1));
    allowed = 0;
    for (int i = 0; i < 5; ++i) {
        if (filter("msg", Level::info, LogContext{}))
            ++allowed;
    }
    EXPECT_EQ(allowed, 3);
}

TEST(BuiltinFiltersTest, UserFilterAllowsOnlySpecifiedUsers) {
    std::vector<std::string> allowed = {"alice", "bob"};
    auto filter = BuiltinFilters::user_filter(allowed);

    LogContext ctx1, ctx2, ctx3;
    ctx1.with_user("alice");
    ctx2.with_user("bob");
    ctx3.with_user("carol");

    EXPECT_TRUE(filter("msg", Level::info, ctx1));
    EXPECT_TRUE(filter("msg", Level::info, ctx2));
    EXPECT_FALSE(filter("msg", Level::info, ctx3));

    // If user_id is empty, should allow
    LogContext empty_ctx;
    EXPECT_TRUE(filter("msg", Level::info, empty_ctx));
}

TEST(BuiltinFiltersTest, TimeWindowFilterAllowsWithinWindow) {
    using clock = std::chrono::system_clock;
    auto now = clock::now();
    auto filter = BuiltinFilters::time_window_filter(
        now - std::chrono::seconds(1), now + std::chrono::seconds(1));
    EXPECT_TRUE(filter("msg", Level::info, LogContext{}));

    auto filter_past = BuiltinFilters::time_window_filter(
        now - std::chrono::seconds(3), now - std::chrono::seconds(2));
    EXPECT_FALSE(filter_past("msg", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, KeywordFilterInclude) {
    std::vector<std::string> keywords = {"foo", "bar"};
    auto filter = BuiltinFilters::keyword_filter(keywords, true);
    EXPECT_TRUE(filter("this is foo", Level::info, LogContext{}));
    EXPECT_TRUE(filter("bar is here", Level::info, LogContext{}));
    EXPECT_FALSE(filter("no keywords", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, KeywordFilterExclude) {
    std::vector<std::string> keywords = {"foo", "bar"};
    auto filter = BuiltinFilters::keyword_filter(keywords, false);
    EXPECT_FALSE(filter("this is foo", Level::info, LogContext{}));
    EXPECT_FALSE(filter("bar is here", Level::info, LogContext{}));
    EXPECT_TRUE(filter("no keywords", Level::info, LogContext{}));
}

TEST(BuiltinFiltersTest, SamplingFilterAlwaysTrueIfRateOne) {
    auto filter = BuiltinFilters::sampling_filter(1.0);
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(filter("msg", Level::info, LogContext{}));
    }
}

TEST(BuiltinFiltersTest, SamplingFilterAlwaysFalseIfRateZero) {
    auto filter = BuiltinFilters::sampling_filter(0.0);
    for (int i = 0; i < 10; ++i) {
        EXPECT_FALSE(filter("msg", Level::info, LogContext{}));
    }
}

TEST(BuiltinFiltersTest, SamplingFilterAllowsRoughlyCorrectFraction) {
    auto filter = BuiltinFilters::sampling_filter(0.2);
    int allowed = 0;
    for (int i = 0; i < 100; ++i) {
        if (filter("msg", Level::info, LogContext{}))
            ++allowed;
    }
    // Allow some tolerance due to integer division
    EXPECT_NEAR(allowed, 20, 5);
}

TEST(BuiltinFiltersTest, DuplicateFilterSuppressesDuplicatesWithinWindow) {
    auto filter = BuiltinFilters::duplicate_filter(std::chrono::seconds(2));
    EXPECT_TRUE(filter("msg1", Level::info, LogContext{}));
    EXPECT_FALSE(filter("msg1", Level::info, LogContext{}));
    EXPECT_TRUE(filter("msg2", Level::info, LogContext{}));
    EXPECT_FALSE(filter("msg2", Level::info, LogContext{}));

    // Wait for window to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_TRUE(filter("msg1", Level::info, LogContext{}));
    EXPECT_TRUE(filter("msg2", Level::info, LogContext{}));
}
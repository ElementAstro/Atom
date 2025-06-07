// filepath: atom/extra/spdlog/core/test_error.h

#include <gtest/gtest.h>
#include <string>
#include <system_error>
#include "error.h"


using modern_log::log_error_category;
using modern_log::LogError;
using modern_log::LogErrorCategory;
using modern_log::make_error_code;
using modern_log::Result;

TEST(LogErrorTest, ErrorCategoryNameIsCorrect) {
    const std::error_category& cat = log_error_category();
    EXPECT_STREQ(cat.name(), "modern_log");
}

TEST(LogErrorTest, ErrorCategoryMessageIsCorrect) {
    const LogErrorCategory& cat = log_error_category();
    EXPECT_EQ(cat.message(static_cast<int>(LogError::none)), "No error");
    EXPECT_EQ(cat.message(static_cast<int>(LogError::logger_not_found)),
              "Logger not found");
    EXPECT_EQ(cat.message(static_cast<int>(LogError::invalid_config)),
              "Invalid configuration");
    EXPECT_EQ(cat.message(static_cast<int>(LogError::file_creation_failed)),
              "Failed to create log file");
    EXPECT_EQ(cat.message(static_cast<int>(LogError::async_init_failed)),
              "Failed to initialize async logging");
    EXPECT_EQ(cat.message(static_cast<int>(LogError::sink_creation_failed)),
              "Failed to create log sink");
    EXPECT_EQ(cat.message(static_cast<int>(LogError::permission_denied)),
              "Permission denied");
    EXPECT_EQ(cat.message(static_cast<int>(LogError::disk_full)), "Disk full");
    EXPECT_EQ(cat.message(static_cast<int>(LogError::network_error)),
              "Network error");
    EXPECT_EQ(cat.message(static_cast<int>(LogError::serialization_failed)),
              "Serialization failed");
    EXPECT_EQ(cat.message(9999), "Unknown error");
}

TEST(LogErrorTest, MakeErrorCodeProducesCorrectCategoryAndValue) {
    std::error_code ec = make_error_code(LogError::file_creation_failed);
    EXPECT_EQ(ec.category().name(), std::string("modern_log"));
    EXPECT_EQ(ec.value(), static_cast<int>(LogError::file_creation_failed));
    EXPECT_EQ(ec.message(), "Failed to create log file");
}

TEST(LogErrorTest, StdErrorCodeInteroperability) {
    std::error_code ec = LogError::disk_full;
    EXPECT_EQ(ec.category().name(), std::string("modern_log"));
    EXPECT_EQ(ec.value(), static_cast<int>(LogError::disk_full));
    EXPECT_EQ(ec.message(), "Disk full");
}

TEST(LogErrorTest, ResultTypeSuccess) {
    Result<int> r = 42;
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(r.value(), 42);
}

TEST(LogErrorTest, ResultTypeError) {
    Result<int> r = std::unexpected(LogError::network_error);
    EXPECT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), LogError::network_error);
}

TEST(LogErrorTest, ErrorCodeEnumTrait) {
    // This test ensures LogError is recognized as an error_code_enum
    bool is_enum = std::is_error_code_enum<LogError>::value;
    EXPECT_TRUE(is_enum);
}
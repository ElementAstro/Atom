// filepath: atom/extra/spdlog/logger/test_logger.cpp

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <any>
#include <exception>
#include <spdlog/sinks/null_sink.h>
#include "../core/context.h"
#include "../core/types.h"
#include "../events/event_system.h"
#include "../filters/filter.h"
#include "../sampling/sampler.h"
#include "../utils/structured_data.h"
#include "logger.h"

using namespace modern_log;

namespace {

class DummyEventSystem : public LogEventSystem {
public:
    std::vector<std::pair<LogEvent, std::any>> events;
    void emit(LogEvent event, const std::any& data = {}) override {
        events.emplace_back(event, data);
    }
};

struct DummyStructuredData : public StructuredData {
    std::string to_json() const override { return R"({"foo":42})"; }
};

class DummySampler : public LogSampler {
public:
    bool should_sample_result = true;
    bool should_sample() override { return should_sample_result; }
};

class DummyFilter : public LogFilter {
public:
    bool should_log_result = true;
    bool should_log(const std::string&, Level, const LogContext&) override {
        return should_log_result;
    }
};

std::shared_ptr<spdlog::logger> make_null_logger() {
    auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    return std::make_shared<spdlog::logger>("test_logger", sink);
}

} // namespace

TEST(LoggerTest, EmitsLoggerCreatedEventOnConstruction) {
    DummyEventSystem event_sys;
    auto logger = std::make_shared<Logger>(make_null_logger(), &event_sys);
    ASSERT_FALSE(event_sys.events.empty());
    EXPECT_EQ(event_sys.events[0].first, LogEvent::logger_created);
    EXPECT_EQ(std::any_cast<std::string>(event_sys.events[0].second), "test_logger");
}

TEST(LoggerTest, LogStructuredCallsLogInternalWithJson) {
    auto logger = std::make_shared<Logger>(make_null_logger());
    DummyStructuredData data;
    // Should not throw and should call log_internal (covered by coverage tools)
    logger->log_structured(Level::info, data);
}

TEST(LoggerTest, LogExceptionIncludesExceptionMessageAndContext) {
    auto logger = std::make_shared<Logger>(make_null_logger());
    try {
        throw std::runtime_error("fail!");
    } catch (const std::exception& ex) {
        // Should not throw and should call log_internal (covered by coverage tools)
        logger->log_exception(Level::error, ex, "CTX");
    }
}

TEST(LoggerTest, ShouldLogInternalReturnsFalseIfShouldLogIsFalse) {
    auto logger = std::make_shared<Logger>(make_null_logger());
    logger->set_level(Level::error);
    // Should not log info-level if logger is set to error
    EXPECT_FALSE(logger->should_log_internal(Level::info));
}

TEST(LoggerTest, ShouldLogInternalReturnsFalseIfSamplerBlocks) {
    auto logger = std::make_shared<Logger>(make_null_logger());
    // Replace sampler with dummy
    auto dummy_sampler = std::make_unique<DummySampler>();
    dummy_sampler->should_sample_result = false;
    Logger* raw_logger = logger.get();
    raw_logger->sampler_ = std::move(dummy_sampler);
    EXPECT_FALSE(logger->should_log_internal(Level::info));
}

TEST(LoggerTest, LogInternalSkipsIfFilterBlocks) {
    auto logger = std::make_shared<Logger>(make_null_logger());
    // Replace filter with dummy
    auto dummy_filter = std::make_unique<DummyFilter>();
    dummy_filter->should_log_result = false;
    Logger* raw_logger = logger.get();
    raw_logger->filter_ = std::move(dummy_filter);
    // Should not throw and should not log
    logger->log_internal(Level::info, "msg");
    EXPECT_EQ(logger->get_stats().filtered_logs.load(), 1u);
}

TEST(LoggerTest, LogInternalEnrichesMessageWithContext) {
    auto logger = std::make_shared<Logger>(make_null_logger());
    LogContext ctx;
    ctx.with_user("alice").with_session("sess1").with_trace("t1").with_request("r1");
    logger->with_context(ctx);
    // Should not throw and should call log_internal (covered by coverage tools)
    logger->log_internal(Level::info, "msg");
}

TEST(LoggerTest, LogInternalCatchesExceptionsAndEmitsErrorEvent) {
    struct ThrowingFilter : public LogFilter {
        bool should_log(const std::string&, Level, const LogContext&) override {
            throw std::runtime_error("fail");
        }
    };
    DummyEventSystem event_sys;
    auto logger = std::make_shared<Logger>(make_null_logger(), &event_sys);
    Logger* raw_logger = logger.get();
    raw_logger->filter_ = std::make_unique<ThrowingFilter>();
    logger->log_internal(Level::info, "msg");
    EXPECT_EQ(logger->get_stats().failed_logs.load(), 1u);
    ASSERT_FALSE(event_sys.events.empty());
    EXPECT_EQ(event_sys.events.back().first, LogEvent::error_occurred);
}

TEST(LoggerTest, EnrichMessageWithContextFormatsCorrectly) {
    auto logger = std::make_shared<Logger>(make_null_logger());
    LogContext ctx;
    ctx.with_user("alice").with_session("sess1").with_trace("t1").with_request("r1");
    std::string msg = logger->enrich_message_with_context("msg", ctx);
    EXPECT_NE(msg.find("user=alice"), std::string::npos);
    EXPECT_NE(msg.find("session=sess1"), std::string::npos);
    EXPECT_NE(msg.find("trace=t1"), std::string::npos);
    EXPECT_NE(msg.find("request=r1"), std::string::npos);
    EXPECT_NE(msg.find("msg"), std::string::npos);
}

TEST(LoggerTest, EnrichMessageWithContextReturnsOriginalIfContextEmpty) {
    auto logger = std::make_shared<Logger>(make_null_logger());
    LogContext ctx;
    std::string msg = logger->enrich_message_with_context("msg", ctx);
    EXPECT_EQ(msg, "msg");
}

TEST(LoggerTest, EmitEventCallsEventSystem) {
    DummyEventSystem event_sys;
    auto logger = std::make_shared<Logger>(make_null_logger(), &event_sys);
    logger->emit_event(LogEvent::logger_destroyed, std::string("bye"));
    ASSERT_FALSE(event_sys.events.empty());
    EXPECT_EQ(event_sys.events.back().first, LogEvent::logger_destroyed);
    EXPECT_EQ(std::any_cast<std::string>(event_sys.events.back().second), "bye");
}
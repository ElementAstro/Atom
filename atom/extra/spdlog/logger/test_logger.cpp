#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <thread>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/ostream_sink.h>
#include <sstream>
#include "logger.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::InSequence;
using namespace modern_log;

// Mock classes for testing
class MockLogEventSystem : public LogEventSystem {
public:
    // Only override if the base class method is virtual!
    MOCK_METHOD(void, emit, (LogEvent event, const std::any& data), ());
    // subscribe and unsubscribe in LogEventSystem return EventId and bool, not void, and are not virtual
    MOCK_METHOD(EventId, subscribe, (LogEvent event, EventCallback callback), ());
    MOCK_METHOD(bool, unsubscribe, (LogEvent event, EventId event_id), ());
};

class MockLogFilter : public LogFilter {
public:
    // Only override if the base class method is virtual!
    MOCK_METHOD(bool, should_log, (const std::string& message, Level level, const LogContext& context), (const));
    MOCK_METHOD(void, add_filter, (FilterFunc filter), ());
    MOCK_METHOD(void, clear_filters, (), ());
};

class MockLogSampler : public LogSampler {
public:
    // Only override if the base class method is virtual!
    MOCK_METHOD(bool, should_sample, (), ());
    MOCK_METHOD(size_t, get_dropped_count, (), (const));
    MOCK_METHOD(double, get_current_rate, (), (const));
    MOCK_METHOD(void, set_strategy, (SamplingStrategy strategy, double rate), ());
    MOCK_METHOD(void, reset_stats, (), ());
};

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create an in-memory spdlog logger for testing
        log_stream = std::make_shared<std::ostringstream>();
        auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(*log_stream);
        spdlog_logger = std::make_shared<spdlog::logger>("test_logger", sink);
        spdlog_logger->set_level(spdlog::level::trace);
        
        mock_event_system = std::make_unique<NiceMock<MockLogEventSystem>>();
        event_system_ptr = mock_event_system.get();
    }

    std::shared_ptr<std::ostringstream> log_stream;
    std::shared_ptr<spdlog::logger> spdlog_logger;
    std::unique_ptr<MockLogEventSystem> mock_event_system;
    MockLogEventSystem* event_system_ptr;

    std::string getLogOutput() const {
        return log_stream->str();
    }

    void clearLogOutput() {
        log_stream->str("");
        log_stream->clear();
    }
};

TEST_F(LoggerTest, ConstructorInitializesComponents) {
    EXPECT_CALL(*mock_event_system, emit(LogEvent::logger_created, _));
    
    Logger logger(spdlog_logger, event_system_ptr);
    
    EXPECT_EQ(logger.get_spdlog_logger(), spdlog_logger);
    EXPECT_EQ(logger.get_log_type(), LogType::general);
    EXPECT_TRUE(logger.get_context().empty());
}

TEST_F(LoggerTest, BasicLoggingAtAllLevels) {
    Logger logger(spdlog_logger);
    
    logger.trace("trace message");
    logger.debug("debug message");
    logger.info("info message");
    logger.warn("warn message");
    logger.error("error message");
    logger.critical("critical message");
    
    std::string output = getLogOutput();
    EXPECT_NE(output.find("trace message"), std::string::npos);
    EXPECT_NE(output.find("debug message"), std::string::npos);
    EXPECT_NE(output.find("info message"), std::string::npos);
    EXPECT_NE(output.find("warn message"), std::string::npos);
    EXPECT_NE(output.find("error message"), std::string::npos);
    EXPECT_NE(output.find("critical message"), std::string::npos);
}

TEST_F(LoggerTest, FormattedLogging) {
    Logger logger(spdlog_logger);

    // Use format string directly with arguments
    logger.info("User {} logged in with status {}", "john", 200);

    std::string output = getLogOutput();
    EXPECT_NE(output.find("User john logged in with status 200"), std::string::npos);
}

TEST_F(LoggerTest, ContextEnrichment) {
    Logger logger(spdlog_logger);

    LogContext ctx;
    // FIX: Use chainable with_* methods instead of set_* methods
    ctx.with_user("user123")
       .with_session("session456")
       .with_trace("trace789")
       .with_request("req000");

    logger.with_context(ctx);
    logger.info("test message");

    std::string output = getLogOutput();
    EXPECT_NE(output.find("user=user123"), std::string::npos);
    EXPECT_NE(output.find("session=session456"), std::string::npos);
    EXPECT_NE(output.find("trace=trace789"), std::string::npos);
    EXPECT_NE(output.find("request=req000"), std::string::npos);
    EXPECT_NE(output.find("test message"), std::string::npos);
}

TEST_F(LoggerTest, ContextClearing) {
    Logger logger(spdlog_logger);

    LogContext ctx;
    ctx.with_user("user123");
    logger.with_context(ctx);

    logger.info("with context");
    logger.clear_context();
    logger.info("without context");

    std::string output = getLogOutput();
    EXPECT_NE(output.find("user=user123"), std::string::npos);
    // Second message should not have context
    std::string lines = output;
    size_t second_msg_pos = lines.find("without context");
    EXPECT_NE(second_msg_pos, std::string::npos);
    std::string second_line = lines.substr(second_msg_pos - 50, 100);
    EXPECT_EQ(second_line.find("user="), std::string::npos);
}

TEST_F(LoggerTest, StructuredLogging) {
    Logger logger(spdlog_logger);
    
    StructuredData data;
    data.add("key1", "value1");
    data.add("key2", 42);
    data.add("key3", true);
    
    logger.log_structured(Level::info, data);
    
    std::string output = getLogOutput();
    EXPECT_NE(output.find("STRUCTURED:"), std::string::npos);
    EXPECT_NE(output.find("key1"), std::string::npos);
    EXPECT_NE(output.find("value1"), std::string::npos);
    EXPECT_NE(output.find("key2"), std::string::npos);
    EXPECT_NE(output.find("42"), std::string::npos);
}

TEST_F(LoggerTest, ExceptionLogging) {
    Logger logger(spdlog_logger);
    
    std::runtime_error ex("test exception");
    logger.log_exception(Level::error, ex, "test context");
    
    std::string output = getLogOutput();
    EXPECT_NE(output.find("Exception: test exception"), std::string::npos);
    EXPECT_NE(output.find("Context: test context"), std::string::npos);
    EXPECT_NE(output.find("Stack trace:"), std::string::npos);
}

TEST_F(LoggerTest, ConditionalLogging) {
    Logger logger(spdlog_logger);
    
    logger.log_if(true, Level::info, "should log");
    logger.log_if(false, Level::info, "should not log");
    
    std::string output = getLogOutput();
    EXPECT_NE(output.find("should log"), std::string::npos);
    EXPECT_EQ(output.find("should not log"), std::string::npos);
}

TEST_F(LoggerTest, ScopedTiming) {
    Logger logger(spdlog_logger);
    
    {
        auto timer = logger.time_scope("test_operation");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::string output = getLogOutput();
    EXPECT_NE(output.find("test_operation took"), std::string::npos);
    EXPECT_NE(output.find("μs"), std::string::npos);
}

TEST_F(LoggerTest, BatchLogging) {
    Logger logger(spdlog_logger);
    
    logger.log_batch(Level::info, "message1", "message2", "message3");
    
    std::string output = getLogOutput();
    EXPECT_NE(output.find("message1"), std::string::npos);
    EXPECT_NE(output.find("message2"), std::string::npos);
    EXPECT_NE(output.find("message3"), std::string::npos);
}

TEST_F(LoggerTest, RangeLogging) {
    Logger logger(spdlog_logger);
    
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    logger.log_range(Level::info, "numbers", numbers);
    
    std::string output = getLogOutput();
    EXPECT_NE(output.find("numbers"), std::string::npos);
    EXPECT_NE(output.find("1"), std::string::npos);
    EXPECT_NE(output.find("5"), std::string::npos);
}

TEST_F(LoggerTest, LogLevelFiltering) {
    Logger logger(spdlog_logger);
    logger.set_level(Level::warn);
    
    logger.debug("debug message");
    logger.info("info message");
    logger.warn("warn message");
    logger.error("error message");
    
    std::string output = getLogOutput();
    EXPECT_EQ(output.find("debug message"), std::string::npos);
    EXPECT_EQ(output.find("info message"), std::string::npos);
    EXPECT_NE(output.find("warn message"), std::string::npos);
    EXPECT_NE(output.find("error message"), std::string::npos);
}

TEST_F(LoggerTest, ShouldLogChecking) {
    Logger logger(spdlog_logger);
    
    logger.set_level(Level::warn);
    
    EXPECT_FALSE(logger.should_log(Level::trace));
    EXPECT_FALSE(logger.should_log(Level::debug));
    EXPECT_FALSE(logger.should_log(Level::info));
    EXPECT_TRUE(logger.should_log(Level::warn));
    EXPECT_TRUE(logger.should_log(Level::error));
    EXPECT_TRUE(logger.should_log(Level::critical));
}

TEST_F(LoggerTest, StatisticsTracking) {
    Logger logger(spdlog_logger);
    
    logger.info("message1");
    logger.warn("message2");
    logger.error("message3");
    
    const auto& stats = logger.get_stats();
    EXPECT_EQ(stats.total_logs.load(), 3u);
    EXPECT_EQ(stats.failed_logs.load(), 0u);
}

TEST_F(LoggerTest, StatisticsReset) {
    Logger logger(spdlog_logger);
    
    logger.info("message");
    EXPECT_GT(logger.get_stats().total_logs.load(), 0u);
    
    logger.reset_stats();
    EXPECT_EQ(logger.get_stats().total_logs.load(), 0u);
}

TEST_F(LoggerTest, FlushOperation) {
    Logger logger(spdlog_logger);
    
    logger.info("test message");
    logger.flush();
    
    // Verify message is in output after flush
    std::string output = getLogOutput();
    EXPECT_NE(output.find("test message"), std::string::npos);
}

TEST_F(LoggerTest, LogTypeManagement) {
    Logger logger(spdlog_logger);
    
    EXPECT_EQ(logger.get_log_type(), LogType::general);
    
    logger.set_log_type(LogType::security);
    EXPECT_EQ(logger.get_log_type(), LogType::security);
    
    logger.set_log_type(LogType::performance);
    EXPECT_EQ(logger.get_log_type(), LogType::performance);
}

TEST_F(LoggerTest, EventSystemIntegration) {
    EXPECT_CALL(*mock_event_system, emit(LogEvent::logger_created, _));
    
    Logger logger(spdlog_logger, event_system_ptr);
    
    // Verify constructor emitted logger_created event
    ::testing::Mock::VerifyAndClearExpectations(mock_event_system.get());
}

TEST_F(LoggerTest, ThreadSafety) {
    Logger logger(spdlog_logger);

    std::vector<std::thread> threads;
    const int num_threads = 10;
    const int messages_per_thread = 100;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&logger, i, messages_per_thread]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                // FIX: Use fmt::format for formatting
                logger.info("Thread {} message {}", i, j);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    const auto& stats = logger.get_stats();
    EXPECT_EQ(stats.total_logs.load(), num_threads * messages_per_thread);
}

TEST_F(LoggerTest, ContextMerging) {
    Logger logger(spdlog_logger);

    LogContext ctx1;
    ctx1.with_user("user1")
        .with_session("session1");

    LogContext ctx2;
    ctx2.with_user("user2")  // Should override
        .with_trace("trace1");  // Should add

    logger.with_context(ctx1).with_context(ctx2);
    logger.info("test message");

    std::string output = getLogOutput();
    EXPECT_NE(output.find("user=user2"), std::string::npos);  // Overridden
    EXPECT_NE(output.find("session=session1"), std::string::npos);  // Preserved
    EXPECT_NE(output.find("trace=trace1"), std::string::npos);  // Added
}

TEST_F(LoggerTest, ContextualLogging) {
    Logger logger(spdlog_logger);

    LogContext temp_ctx;
    temp_ctx.with_user("temp_user");

    logger.log_with_context(Level::info, temp_ctx, "temporary context message");
    logger.info("normal message");

    std::string output = getLogOutput();
    EXPECT_NE(output.find("temp_user"), std::string::npos);

    // Verify the temporary context didn't affect the logger's context
    EXPECT_TRUE(logger.get_context().empty());
}

TEST_F(LoggerTest, SetFlushLevel) {
    Logger logger(spdlog_logger);
    
    logger.set_flush_level(Level::warn);
    
    // This test mainly verifies the function doesn't crash
    logger.info("info message");
    logger.warn("warn message");
    
    std::string output = getLogOutput();
    EXPECT_NE(output.find("info message"), std::string::npos);
    EXPECT_NE(output.find("warn message"), std::string::npos);
}

TEST_F(LoggerTest, FilteringIntegration) {
    Logger logger(spdlog_logger);
    
    // Add a filter that blocks messages containing "secret"
    logger.add_filter([](const std::string& msg, Level, const LogContext&) {
        return msg.find("secret") == std::string::npos;
    });
    
    logger.info("normal message");
    logger.info("secret message");
    
    std::string output = getLogOutput();
    EXPECT_NE(output.find("normal message"), std::string::npos);
    EXPECT_EQ(output.find("secret message"), std::string::npos);
    
    // Verify filtered message is counted in stats
    const auto& stats = logger.get_stats();
    EXPECT_EQ(stats.filtered_logs.load(), 1u);
}

TEST_F(LoggerTest, SamplingIntegration) {
    Logger logger(spdlog_logger);
    
    // Set sampling to 0% (drop everything)
    logger.set_sampling(SamplingStrategy::uniform, 0.0);
    
    logger.info("sampled message 1");
    logger.info("sampled message 2");
    
    std::string output = getLogOutput();
    EXPECT_EQ(output.find("sampled message"), std::string::npos);
    
    // Verify sampled messages are counted in stats
    const auto& stats = logger.get_stats();
    EXPECT_EQ(stats.sampled_logs.load(), 2u);
}

TEST_F(LoggerTest, ErrorHandlingInLogInternal) {
    // Create a logger with a bad sink to simulate errors
    auto bad_sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(std::cout);
    auto bad_logger = std::make_shared<spdlog::logger>("bad_logger", bad_sink);
    
    Logger logger(bad_logger);
    
    // This should not crash even if the underlying logger fails
    logger.info("test message");
    
    // The test mainly verifies no exceptions are thrown
}
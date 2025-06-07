// filepath: atom/extra/spdlog/events/test_event_system.cpp

#include <gtest/gtest.h>
#include <any>
#include <atomic>
#include <string>
#include <vector>
#include "../core/types.h"
#include "event_system.h"


using modern_log::LogEvent;
using modern_log::LogEventSystem;

namespace {

struct CallbackCounter {
    std::atomic<int> count{0};
    std::vector<std::any> received_data;
    void operator()(LogEvent, const std::any& data) {
        ++count;
        received_data.push_back(data);
    }
};

}  // namespace

TEST(LogEventSystemTest, SubscribeAndEmitCallsCallback) {
    LogEventSystem sys;
    int called = 0;
    auto id = sys.subscribe(LogEvent::logger_created,
                            [&](LogEvent, const std::any&) { ++called; });
    sys.emit(LogEvent::logger_created);
    EXPECT_EQ(called, 1);
    sys.emit(LogEvent::logger_created);
    EXPECT_EQ(called, 2);
}

TEST(LogEventSystemTest, EmitWithDataPassesDataToCallback) {
    LogEventSystem sys;
    std::string received;
    sys.subscribe(LogEvent::logger_created,
                  [&](LogEvent, const std::any& data) {
                      if (data.has_value()) {
                          received = std::any_cast<std::string>(data);
                      }
                  });
    sys.emit(LogEvent::logger_created, std::string("hello"));
    EXPECT_EQ(received, "hello");
}

TEST(LogEventSystemTest, UnsubscribeRemovesCallback) {
    LogEventSystem sys;
    int called = 0;
    auto id = sys.subscribe(LogEvent::logger_created,
                            [&](LogEvent, const std::any&) { ++called; });
    EXPECT_TRUE(sys.unsubscribe(LogEvent::logger_created, id));
    sys.emit(LogEvent::logger_created);
    EXPECT_EQ(called, 0);
}

TEST(LogEventSystemTest, UnsubscribeReturnsFalseIfNotFound) {
    LogEventSystem sys;
    EXPECT_FALSE(sys.unsubscribe(LogEvent::logger_created, 12345));
}

TEST(LogEventSystemTest, MultipleSubscribersAllCalled) {
    LogEventSystem sys;
    int count1 = 0, count2 = 0;
    sys.subscribe(LogEvent::logger_created,
                  [&](LogEvent, const std::any&) { ++count1; });
    sys.subscribe(LogEvent::logger_created,
                  [&](LogEvent, const std::any&) { ++count2; });
    sys.emit(LogEvent::logger_created);
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);
}

TEST(LogEventSystemTest, SubscriberCountReflectsSubscriptions) {
    LogEventSystem sys;
    EXPECT_EQ(sys.subscriber_count(LogEvent::logger_created), 0u);
    auto id1 = sys.subscribe(LogEvent::logger_created,
                             [](LogEvent, const std::any&) {});
    EXPECT_EQ(sys.subscriber_count(LogEvent::logger_created), 1u);
    auto id2 = sys.subscribe(LogEvent::logger_created,
                             [](LogEvent, const std::any&) {});
    EXPECT_EQ(sys.subscriber_count(LogEvent::logger_created), 2u);
    sys.unsubscribe(LogEvent::logger_created, id1);
    EXPECT_EQ(sys.subscriber_count(LogEvent::logger_created), 1u);
    sys.unsubscribe(LogEvent::logger_created, id2);
    EXPECT_EQ(sys.subscriber_count(LogEvent::logger_created), 0u);
}

TEST(LogEventSystemTest, ClearAllSubscriptionsRemovesAll) {
    LogEventSystem sys;
    sys.subscribe(LogEvent::logger_created, [](LogEvent, const std::any&) {});
    sys.subscribe(LogEvent::logger_destroyed, [](LogEvent, const std::any&) {});
    sys.clear_all_subscriptions();
    EXPECT_EQ(sys.subscriber_count(LogEvent::logger_created), 0u);
    EXPECT_EQ(sys.subscriber_count(LogEvent::logger_destroyed), 0u);
}

TEST(LogEventSystemTest, EmitDoesNotThrowIfCallbackThrows) {
    LogEventSystem sys;
    sys.subscribe(LogEvent::logger_created, [](LogEvent, const std::any&) {
        throw std::runtime_error("fail");
    });
    // Should not throw
    EXPECT_NO_THROW(sys.emit(LogEvent::logger_created));
}

TEST(LogEventSystemTest, SubscribeDifferentEventsAreIndependent) {
    LogEventSystem sys;
    int called1 = 0, called2 = 0;
    sys.subscribe(LogEvent::logger_created,
                  [&](LogEvent, const std::any&) { ++called1; });
    sys.subscribe(LogEvent::logger_destroyed,
                  [&](LogEvent, const std::any&) { ++called2; });
    sys.emit(LogEvent::logger_created);
    EXPECT_EQ(called1, 1);
    EXPECT_EQ(called2, 0);
    sys.emit(LogEvent::logger_destroyed);
    EXPECT_EQ(called1, 1);
    EXPECT_EQ(called2, 1);
}
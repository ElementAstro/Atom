// FILE: atom/async/test_message_bus.hpp

#include <gtest/gtest.h>
#include <asio/io_context.hpp>
#include <memory>

#include "atom/async/message_bus.hpp"

using namespace atom::async;

class MessageBusTest : public ::testing::Test {
protected:
    asio::io_context io_context;
    std::shared_ptr<MessageBus> messageBus;

    void SetUp() override { messageBus = MessageBus::createShared(io_context); }
};

TEST_F(MessageBusTest, CreateShared) { ASSERT_NE(messageBus, nullptr); }

TEST_F(MessageBusTest, PublishAndSubscribe) {
    bool called = false;
    auto token =
        messageBus->subscribe<int>("test.message", [&](const int& msg) {
            called = true;
            EXPECT_EQ(msg, 42);
        });

    messageBus->publish<int>("test.message", 42);
    io_context.run();
    EXPECT_TRUE(called);
    messageBus->unsubscribe<int>(token);
}

TEST_F(MessageBusTest, PublishWithDelay) {
    bool called = false;
    auto token =
        messageBus->subscribe<int>("test.message", [&](const int& msg) {
            called = true;
            EXPECT_EQ(msg, 42);
        });

    messageBus->publish<int>("test.message", 42,
                             std::chrono::milliseconds(100));
    io_context.run_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(called);
    messageBus->unsubscribe<int>(token);
}

TEST_F(MessageBusTest, PublishGlobal) {
    bool called = false;
    auto token =
        messageBus->subscribe<int>("test.message", [&](const int& msg) {
            called = true;
            EXPECT_EQ(msg, 42);
        });

    messageBus->publishGlobal<int>(42);
    io_context.run();
    EXPECT_TRUE(called);
    messageBus->unsubscribe<int>(token);
}

TEST_F(MessageBusTest, Unsubscribe) {
    bool called = false;
    auto token = messageBus->subscribe<int>(
        "test.message", [&](const int& msg) { called = true; });

    messageBus->unsubscribe<int>(token);
    messageBus->publish<int>("test.message", 42);
    io_context.run();
    EXPECT_FALSE(called);
}

TEST_F(MessageBusTest, UnsubscribeAll) {
    bool called = false;
    messageBus->subscribe<int>("test.message",
                               [&](const int& msg) { called = true; });

    messageBus->unsubscribeAll<int>("test.message");
    messageBus->publish<int>("test.message", 42);
    io_context.run();
    EXPECT_FALSE(called);
}

TEST_F(MessageBusTest, GetSubscriberCount) {
    auto token = messageBus->subscribe<int>("test.message", [](const int&) {});
    EXPECT_EQ(messageBus->getSubscriberCount<int>("test.message"), 1);
    messageBus->unsubscribe<int>(token);
    EXPECT_EQ(messageBus->getSubscriberCount<int>("test.message"), 0);
}

TEST_F(MessageBusTest, HasSubscriber) {
    auto token = messageBus->subscribe<int>("test.message", [](const int&) {});
    EXPECT_TRUE(messageBus->hasSubscriber<int>("test.message"));
    messageBus->unsubscribe<int>(token);
    EXPECT_FALSE(messageBus->hasSubscriber<int>("test.message"));
}

TEST_F(MessageBusTest, ClearAllSubscribers) {
    messageBus->subscribe<int>("test.message", [](const int&) {});
    messageBus->clearAllSubscribers();
    EXPECT_EQ(messageBus->getSubscriberCount<int>("test.message"), 0);
}

TEST_F(MessageBusTest, GetActiveNamespaces) {
    messageBus->subscribe<int>("test.namespace.message", [](const int&) {});
    auto namespaces = messageBus->getActiveNamespaces();
    EXPECT_EQ(namespaces.size(), 1);
    EXPECT_EQ(namespaces[0], "test.namespace");
}

TEST_F(MessageBusTest, GetMessageHistory) {
    messageBus->publish<int>("test.message", 42);
    io_context.run();
    auto history = messageBus->getMessageHistory<int>("test.message");
    ASSERT_EQ(history.size(), 1);
    EXPECT_EQ(history[0], 42);
}

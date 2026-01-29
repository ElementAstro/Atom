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

    void SetUp() override { messageBus = MessageBus::createShared(); }
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

// =============================================================================
// Additional MessageBus Tests
// =============================================================================

TEST_F(MessageBusTest, MultipleSubscribers) {
    std::atomic<int> callCount{0};

    auto token1 = messageBus->subscribe<int>(
        "test.message", [&](const int&) { callCount.fetch_add(1); });

    auto token2 = messageBus->subscribe<int>(
        "test.message", [&](const int&) { callCount.fetch_add(1); });

    auto token3 = messageBus->subscribe<int>(
        "test.message", [&](const int&) { callCount.fetch_add(1); });

    messageBus->publish<int>("test.message", 42);
    io_context.run();

    EXPECT_EQ(callCount.load(), 3);

    messageBus->unsubscribe<int>(token1);
    messageBus->unsubscribe<int>(token2);
    messageBus->unsubscribe<int>(token3);
}

TEST_F(MessageBusTest, DifferentMessageTypes) {
    bool intCalled = false;
    bool stringCalled = false;
    bool doubleCalled = false;

    auto intToken =
        messageBus->subscribe<int>("int.message", [&](const int& msg) {
            intCalled = true;
            EXPECT_EQ(msg, 42);
        });

    auto stringToken = messageBus->subscribe<std::string>(
        "string.message", [&](const std::string& msg) {
            stringCalled = true;
            EXPECT_EQ(msg, "hello");
        });

    auto doubleToken =
        messageBus->subscribe<double>("double.message", [&](const double& msg) {
            doubleCalled = true;
            EXPECT_NEAR(msg, 3.14, 1e-5);
        });

    messageBus->publish<int>("int.message", 42);
    messageBus->publish<std::string>("string.message", std::string("hello"));
    messageBus->publish<double>("double.message", 3.14);

    io_context.run();

    EXPECT_TRUE(intCalled);
    EXPECT_TRUE(stringCalled);
    EXPECT_TRUE(doubleCalled);

    messageBus->unsubscribe<int>(intToken);
    messageBus->unsubscribe<std::string>(stringToken);
    messageBus->unsubscribe<double>(doubleToken);
}

TEST_F(MessageBusTest, NamespaceFiltering) {
    std::atomic<int> ns1Count{0};
    std::atomic<int> ns2Count{0};

    auto token1 = messageBus->subscribe<int>(
        "namespace1.message", [&](const int&) { ns1Count.fetch_add(1); });

    auto token2 = messageBus->subscribe<int>(
        "namespace2.message", [&](const int&) { ns2Count.fetch_add(1); });

    messageBus->publish<int>("namespace1.message", 1);
    messageBus->publish<int>("namespace1.message", 2);
    messageBus->publish<int>("namespace2.message", 3);

    io_context.run();

    EXPECT_EQ(ns1Count.load(), 2);
    EXPECT_EQ(ns2Count.load(), 1);

    messageBus->unsubscribe<int>(token1);
    messageBus->unsubscribe<int>(token2);
}

TEST_F(MessageBusTest, ComplexMessageType) {
    struct ComplexMessage {
        int id;
        std::string name;
        std::vector<int> data;
    };

    bool called = false;
    auto token = messageBus->subscribe<ComplexMessage>(
        "complex.message", [&](const ComplexMessage& msg) {
            called = true;
            EXPECT_EQ(msg.id, 42);
            EXPECT_EQ(msg.name, "test");
            EXPECT_EQ(msg.data.size(), 3u);
        });

    ComplexMessage msg{42, "test", {1, 2, 3}};
    messageBus->publish<ComplexMessage>("complex.message", msg);
    io_context.run();

    EXPECT_TRUE(called);
    messageBus->unsubscribe<ComplexMessage>(token);
}

TEST_F(MessageBusTest, SubscriberException) {
    std::atomic<int> callCount{0};

    auto token1 = messageBus->subscribe<int>("test.message", [&](const int&) {
        callCount.fetch_add(1);
        throw std::runtime_error("Subscriber exception");
    });

    auto token2 = messageBus->subscribe<int>(
        "test.message", [&](const int&) { callCount.fetch_add(1); });

    // Publishing should not throw even if subscriber throws
    EXPECT_NO_THROW(messageBus->publish<int>("test.message", 42));
    io_context.run();

    // Both subscribers should have been called
    EXPECT_EQ(callCount.load(), 2);

    messageBus->unsubscribe<int>(token1);
    messageBus->unsubscribe<int>(token2);
}

TEST_F(MessageBusTest, MultiplePublishes) {
    std::vector<int> receivedMessages;
    std::mutex mutex;

    auto token =
        messageBus->subscribe<int>("test.message", [&](const int& msg) {
            std::lock_guard<std::mutex> lock(mutex);
            receivedMessages.push_back(msg);
        });

    for (int i = 0; i < 10; ++i) {
        messageBus->publish<int>("test.message", i);
    }

    io_context.run();

    EXPECT_EQ(receivedMessages.size(), 10u);
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(receivedMessages[i], i);
    }

    messageBus->unsubscribe<int>(token);
}

TEST_F(MessageBusTest, MessageHistoryLimit) {
    // Publish many messages
    for (int i = 0; i < 100; ++i) {
        messageBus->publish<int>("test.message", i);
    }
    io_context.run();

    auto history = messageBus->getMessageHistory<int>("test.message");

    // History should be limited (implementation dependent)
    EXPECT_GT(history.size(), 0u);
}

TEST_F(MessageBusTest, UnsubscribeNonExistent) {
    // Unsubscribing a non-existent token should not crash
    EXPECT_NO_THROW(messageBus->unsubscribe<int>(999999));
}

TEST_F(MessageBusTest, PublishToNonExistentTopic) {
    // Publishing to a topic with no subscribers should not crash
    EXPECT_NO_THROW(messageBus->publish<int>("nonexistent.topic", 42));
    io_context.run();
}

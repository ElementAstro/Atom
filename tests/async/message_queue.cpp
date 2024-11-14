// FILE: atom/async/test_message_queue.hpp

#include <gtest/gtest.h>
#include <asio/io_context.hpp>
#include <memory>

#include "atom/async/message_queue.hpp"

using namespace atom::async;

class MessageQueueTest : public ::testing::Test {
protected:
    asio::io_context io_context;
    std::shared_ptr<MessageQueue<int>> messageQueue;

    void SetUp() override {
        messageQueue = std::make_shared<MessageQueue<int>>(io_context);
    }
};

TEST_F(MessageQueueTest, Subscribe) {
    bool called = false;
    messageQueue->subscribe(
        [&](const int& msg) {
            (void)msg;  // Avoid unused parameter warning
            called = true;
            EXPECT_EQ(msg, 42);
        },
        "test_subscriber");

    messageQueue->publish(42);
    io_context.run();
    EXPECT_TRUE(called);
}

TEST_F(MessageQueueTest, Unsubscribe) {
    bool called = false;
    auto callback = [&](const int& msg) {
        (void)msg;  // Avoid unused parameter warning
        called = true;
    };

    messageQueue->subscribe(callback, "test_subscriber");
    messageQueue->unsubscribe(callback);

    messageQueue->publish(42);
    io_context.run();
    EXPECT_FALSE(called);
}

TEST_F(MessageQueueTest, PublishWithPriority) {
    std::vector<int> receivedMessages;
    messageQueue->subscribe(
        [&](const int& msg) { receivedMessages.push_back(msg); }, "subscriber1",
        1);

    messageQueue->subscribe(
        [&](const int& msg) { receivedMessages.push_back(msg); }, "subscriber2",
        2);

    messageQueue->publish(1, 1);
    messageQueue->publish(2, 2);
    io_context.run();

    ASSERT_EQ(receivedMessages.size(), 2);
    EXPECT_EQ(receivedMessages[0], 2);
    EXPECT_EQ(receivedMessages[1], 1);
}

TEST_F(MessageQueueTest, StartAndStopProcessing) {
    bool called = false;
    messageQueue->subscribe(
        [&](const int& msg) {
            (void)msg;  // Avoid unused parameter warning
            called = true;
        },
        "test_subscriber");

    messageQueue->publish(42);
    messageQueue->stopProcessing();
    io_context.run();
    EXPECT_FALSE(called);

    messageQueue->startProcessing();
    messageQueue->publish(42);
    io_context.run();
    EXPECT_TRUE(called);
}

TEST_F(MessageQueueTest, GetMessageCount) {
    EXPECT_EQ(messageQueue->getMessageCount(), 0);
    messageQueue->publish(42);
    EXPECT_EQ(messageQueue->getMessageCount(), 1);
}

TEST_F(MessageQueueTest, GetSubscriberCount) {
    EXPECT_EQ(messageQueue->getSubscriberCount(), 0);
    messageQueue->subscribe([](const int& msg) { (void)msg; },
                            "test_subscriber");
    EXPECT_EQ(messageQueue->getSubscriberCount(), 1);
}

TEST_F(MessageQueueTest, CancelMessages) {
    bool called = false;
    messageQueue->subscribe(
        [&](const int& msg) {
            (void)msg;  // Avoid unused parameter warning
            called = true;
        },
        "test_subscriber");

    messageQueue->publish(42);
    messageQueue->cancelMessages([](const int& msg) { return msg == 42; });
    io_context.run();
    EXPECT_FALSE(called);
}

TEST_F(MessageQueueTest, ApplyFilter) {
    bool called = false;
    messageQueue->subscribe(
        [&](const int& msg) {
            (void)msg;  // Avoid unused parameter warning
            called = true;
        },
        "test_subscriber", 0, [](const int& msg) { return msg == 42; });

    messageQueue->publish(43);
    io_context.run();
    EXPECT_FALSE(called);

    messageQueue->publish(42);
    io_context.run();
    EXPECT_TRUE(called);
}

TEST_F(MessageQueueTest, HandleTimeout) {
    bool called = false;
    messageQueue->subscribe(
        [&](const int& msg) {
            (void)msg;  // Avoid unused parameter warning
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            called = true;
        },
        "test_subscriber", 0, nullptr, std::chrono::milliseconds(100));

    messageQueue->publish(42);
    io_context.run();
    EXPECT_FALSE(called);
}

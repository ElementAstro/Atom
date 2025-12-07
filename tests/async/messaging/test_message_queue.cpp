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
        messageQueue = std::make_shared<MessageQueue<int>>(1024);
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

// =============================================================================
// Additional MessageQueue Tests
// =============================================================================

TEST_F(MessageQueueTest, MultipleSubscribers) {
    std::atomic<int> callCount{0};

    messageQueue->subscribe([&](const int&) { callCount.fetch_add(1); },
                            "subscriber1");

    messageQueue->subscribe([&](const int&) { callCount.fetch_add(1); },
                            "subscriber2");

    messageQueue->subscribe([&](const int&) { callCount.fetch_add(1); },
                            "subscriber3");

    messageQueue->publish(42);
    io_context.run();

    EXPECT_EQ(callCount.load(), 3);
}

TEST_F(MessageQueueTest, ClearQueue) {
    messageQueue->publish(1);
    messageQueue->publish(2);
    messageQueue->publish(3);

    EXPECT_EQ(messageQueue->getMessageCount(), 3);

    messageQueue->clear();

    EXPECT_EQ(messageQueue->getMessageCount(), 0);
}

TEST_F(MessageQueueTest, QueueCapacity) {
    auto smallQueue = std::make_shared<MessageQueue<int>>(5);

    // Fill the queue
    for (int i = 0; i < 10; ++i) {
        smallQueue->publish(i);
    }

    // Queue should handle overflow gracefully
    EXPECT_LE(smallQueue->getMessageCount(), 10);
}

TEST_F(MessageQueueTest, SubscriberWithName) {
    bool called = false;
    messageQueue->subscribe([&](const int&) { called = true; },
                            "named_subscriber");

    messageQueue->publish(42);
    io_context.run();

    EXPECT_TRUE(called);
}

TEST_F(MessageQueueTest, MultipleFilters) {
    std::atomic<int> evenCount{0};
    std::atomic<int> oddCount{0};

    messageQueue->subscribe([&](const int&) { evenCount.fetch_add(1); },
                            "even_subscriber", 0,
                            [](const int& msg) { return msg % 2 == 0; });

    messageQueue->subscribe([&](const int&) { oddCount.fetch_add(1); },
                            "odd_subscriber", 0,
                            [](const int& msg) { return msg % 2 != 0; });

    for (int i = 0; i < 10; ++i) {
        messageQueue->publish(i);
    }

    io_context.run();

    EXPECT_EQ(evenCount.load(), 5);  // 0, 2, 4, 6, 8
    EXPECT_EQ(oddCount.load(), 5);   // 1, 3, 5, 7, 9
}

TEST_F(MessageQueueTest, SubscriberException) {
    std::atomic<int> callCount{0};

    messageQueue->subscribe(
        [&](const int&) {
            callCount.fetch_add(1);
            throw std::runtime_error("Subscriber exception");
        },
        "throwing_subscriber");

    messageQueue->subscribe([&](const int&) { callCount.fetch_add(1); },
                            "normal_subscriber");

    // Publishing should not throw even if subscriber throws
    EXPECT_NO_THROW(messageQueue->publish(42));
    io_context.run();

    // Both subscribers should have been called
    EXPECT_EQ(callCount.load(), 2);
}

TEST_F(MessageQueueTest, ComplexMessageType) {
    struct ComplexMessage {
        int id;
        std::string name;
        std::vector<int> data;
    };

    auto complexQueue = std::make_shared<MessageQueue<ComplexMessage>>(1024);
    bool called = false;

    complexQueue->subscribe(
        [&](const ComplexMessage& msg) {
            called = true;
            EXPECT_EQ(msg.id, 42);
            EXPECT_EQ(msg.name, "test");
            EXPECT_EQ(msg.data.size(), 3u);
        },
        "complex_subscriber");

    ComplexMessage msg{42, "test", {1, 2, 3}};
    complexQueue->publish(msg);
    io_context.run();

    EXPECT_TRUE(called);
}

TEST_F(MessageQueueTest, RapidPublish) {
    std::atomic<int> receivedCount{0};

    messageQueue->subscribe([&](const int&) { receivedCount.fetch_add(1); },
                            "rapid_subscriber");

    const int numMessages = 100;
    for (int i = 0; i < numMessages; ++i) {
        messageQueue->publish(i);
    }

    io_context.run();

    EXPECT_EQ(receivedCount.load(), numMessages);
}

TEST_F(MessageQueueTest, PriorityOrdering) {
    std::vector<int> receivedOrder;
    std::mutex mutex;

    messageQueue->subscribe(
        [&](const int& msg) {
            std::lock_guard<std::mutex> lock(mutex);
            receivedOrder.push_back(msg);
        },
        "priority_subscriber");

    // Publish with different priorities
    messageQueue->publish(1, 1);   // Low priority
    messageQueue->publish(2, 10);  // High priority
    messageQueue->publish(3, 5);   // Medium priority

    io_context.run();

    // High priority should be processed first
    EXPECT_EQ(receivedOrder.size(), 3u);
    EXPECT_EQ(receivedOrder[0], 2);  // Priority 10
    EXPECT_EQ(receivedOrder[1], 3);  // Priority 5
    EXPECT_EQ(receivedOrder[2], 1);  // Priority 1
}

TEST_F(MessageQueueTest, UnsubscribeAll) {
    messageQueue->subscribe([](const int&) {}, "sub1");
    messageQueue->subscribe([](const int&) {}, "sub2");
    messageQueue->subscribe([](const int&) {}, "sub3");

    EXPECT_EQ(messageQueue->getSubscriberCount(), 3);

    messageQueue->unsubscribeAll();

    EXPECT_EQ(messageQueue->getSubscriberCount(), 0);
}

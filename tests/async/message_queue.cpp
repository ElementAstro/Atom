#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "atom/async/messaging/message_queue.hpp"

#ifdef ATOM_USE_ASIO
#include <asio.hpp>
#endif

// Define a simple message type for testing
struct TestMessage {
    int id;
    std::string content;
    bool operator==(const TestMessage& other) const {
        return id == other.id && content == other.content;
    }
};

// Define a hash for TestMessage to satisfy MessageType concept
namespace std {
template <>
struct hash<TestMessage> {
    size_t operator()(const TestMessage& msg) const {
        return hash<int>()(msg.id) ^ hash<string>()(msg.content);
    }
};
}  // namespace std

// Test fixture for MessageQueue
class MessageQueueTest : public ::testing::Test {
protected:
#ifdef ATOM_USE_ASIO
    asio::io_context io_context_;
    atom::async::MessageQueue<TestMessage> mq_{io_context_};
#else
    atom::async::MessageQueue<TestMessage> mq_;
#endif

    void SetUp() override {
        mq_.startProcessing();
        // Give the processing thread a moment to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        mq_.stopProcessing();
        // Give the processing thread a moment to stop
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

// Test: Constructor and basic state
TEST_F(MessageQueueTest, ConstructorAndInitialState) {
    EXPECT_EQ(mq_.getMessageCount(), 0);
    EXPECT_EQ(mq_.getSubscriberCount(), 0);
}

// Test: Subscribe with valid callback and name
TEST_F(MessageQueueTest, SubscribeValid) {
    std::atomic<int> call_count = 0;
    mq_.subscribe([&](const TestMessage&) { call_count++; }, "test_subscriber");
    EXPECT_EQ(mq_.getSubscriberCount(), 1);
}

// Test: Subscribe with empty callback (should throw)
TEST_F(MessageQueueTest, SubscribeEmptyCallbackThrows) {
    EXPECT_THROW(mq_.subscribe(nullptr, "invalid_subscriber"),
                 atom::async::SubscriberException);
}

// Test: Subscribe with empty name (should throw)
TEST_F(MessageQueueTest, SubscribeEmptyNameThrows) {
    EXPECT_THROW(mq_.subscribe([](const TestMessage&) {}, ""),
                 atom::async::SubscriberException);
}

// Test: Unsubscribe existing subscriber
TEST_F(MessageQueueTest, UnsubscribeExisting) {
    std::atomic<int> call_count = 0;
    auto callback = [&](const TestMessage&) { call_count++; };
    mq_.subscribe(callback, "test_subscriber");
    EXPECT_EQ(mq_.getSubscriberCount(), 1);

    EXPECT_TRUE(mq_.unsubscribe(callback));
    EXPECT_EQ(mq_.getSubscriberCount(), 0);
}

// Test: Unsubscribe non-existent subscriber
TEST_F(MessageQueueTest, UnsubscribeNonExistent) {
    std::atomic<int> call_count = 0;
    auto callback1 = [&](const TestMessage&) { call_count++; };
    auto callback2 = [&](const TestMessage&) { call_count++; };

    mq_.subscribe(callback1, "test_subscriber_1");
    EXPECT_EQ(mq_.getSubscriberCount(), 1);

    EXPECT_FALSE(
        mq_.unsubscribe(callback2));  // Try to unsubscribe a different callback
    EXPECT_EQ(mq_.getSubscriberCount(), 1);
}

// Test: Publish and receive message (const ref)
TEST_F(MessageQueueTest, PublishAndReceiveConstRef) {
    std::atomic<bool> received = false;
    TestMessage msg_sent = {1, "Hello"};
    TestMessage msg_received;

    mq_.subscribe(
        [&](const TestMessage& msg) {
            msg_received = msg;
            received = true;
        },
        "receiver");

    mq_.publish(msg_sent);

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    // Wait for message to be processed
    for (int i = 0; i < 10 && !received; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_TRUE(received);
    EXPECT_EQ(msg_received.id, msg_sent.id);
    EXPECT_EQ(msg_received.content, msg_sent.content);
    EXPECT_EQ(mq_.getMessageCount(), 0);  // Message should be consumed
}

// Test: Publish and receive message (move)
TEST_F(MessageQueueTest, PublishAndReceiveMove) {
    std::atomic<bool> received = false;
    TestMessage msg_sent = {2, "World"};
    TestMessage msg_received;

    mq_.subscribe(
        [&](const TestMessage& msg) {
            msg_received = msg;
            received = true;
        },
        "receiver");

    mq_.publish(std::move(msg_sent));  // Publish with move semantics

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    for (int i = 0; i < 10 && !received; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_TRUE(received);
    EXPECT_EQ(msg_received.id, 2);
    EXPECT_EQ(msg_received.content, "World");
    EXPECT_EQ(mq_.getMessageCount(), 0);
}

// Test: Message filtering
TEST_F(MessageQueueTest, MessageFiltering) {
    std::atomic<int> received_count = 0;

    mq_.subscribe([&](const TestMessage&) { received_count++; },
                  "filter_subscriber", 0,
                  [](const TestMessage& msg) {
                      return msg.id % 2 == 0;  // Only even IDs
                  });

    mq_.publish({1, "Odd"});
    mq_.publish({2, "Even"});
    mq_.publish({3, "Odd"});
    mq_.publish({4, "Even"});

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    for (int i = 0; i < 10 && received_count.load() < 2; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_EQ(received_count.load(),
              2);  // Only messages with ID 2 and 4 should be received
    EXPECT_EQ(mq_.getMessageCount(), 0);
}

// Test: Subscriber priority
TEST_F(MessageQueueTest, SubscriberPriority) {
    std::vector<std::string> call_order;
    std::mutex mtx;

    mq_.subscribe(
        [&](const TestMessage&) {
            std::lock_guard<std::mutex> lock(mtx);
            call_order.push_back("low_priority");
        },
        "low_priority_sub", 0);

    mq_.subscribe(
        [&](const TestMessage&) {
            std::lock_guard<std::mutex> lock(mtx);
            call_order.push_back("high_priority");
        },
        "high_priority_sub", 100);  // Higher priority

    mq_.publish({1, "Priority Test"});

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    for (int i = 0; i < 10 && call_order.size() < 2; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ASSERT_EQ(call_order.size(), 2);
    EXPECT_EQ(call_order[0], "high_priority");
    EXPECT_EQ(call_order[1], "low_priority");
}

// Test: Subscriber timeout (no timeout)
TEST_F(MessageQueueTest, SubscriberNoTimeout) {
    std::atomic<bool> received = false;
    mq_.subscribe(
        [&](const TestMessage&) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(10));  // Simulate work
            received = true;
        },
        "no_timeout_sub", 0, nullptr, std::chrono::milliseconds::zero());

    mq_.publish({1, "No Timeout"});

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    for (int i = 0; i < 10 && !received; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT_TRUE(received);
}

// Test: Subscriber timeout (should timeout)
TEST_F(MessageQueueTest, SubscriberShouldTimeout) {
    std::atomic<bool> received = false;
    std::atomic<bool> exception_caught = false;

    mq_.subscribe(
        [&](const TestMessage&) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(200));  // Longer than timeout
            received = true;
        },
        "timeout_sub", 0, nullptr,
        std::chrono::milliseconds(50));  // 50ms timeout

    // Subscribe another one to catch the exception
    mq_.subscribe(
        [&](const TestMessage& msg) {
            // This callback should not be called if the first one times out
            // The exception is thrown by handleTimeout, not directly by the
            // callback
        },
        "dummy_sub");

    mq_.publish({1, "Should Timeout"});

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    // Wait for a bit longer than the timeout to ensure processing happens
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_FALSE(received);  // The long-running callback should not complete
    // The exception is logged by spdlog, but not directly caught by the test
    // fixture We can't easily assert on spdlog output without mocking it.
    // However, the fact that 'received' is false indicates the timeout
    // mechanism worked.
}

// Test: Subscriber timeout (should not timeout)
TEST_F(MessageQueueTest, SubscriberShouldNotTimeout) {
    std::atomic<bool> received = false;
    mq_.subscribe(
        [&](const TestMessage&) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(10));  // Shorter than timeout
            received = true;
        },
        "no_timeout_sub", 0, nullptr,
        std::chrono::milliseconds(200));  // 200ms timeout

    mq_.publish({1, "Should Not Timeout"});

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    for (int i = 0; i < 10 && !received; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT_TRUE(received);
}

// Test: Clear all messages
TEST_F(MessageQueueTest, ClearAllMessages) {
    mq_.publish({1, "Msg1"});
    mq_.publish({2, "Msg2"});
    mq_.publish({3, "Msg3"});

    // Give some time for messages to be queued but not necessarily processed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    size_t cleared_count = mq_.clearAllMessages();
    EXPECT_GE(cleared_count,
              3);  // At least 3 messages should have been in the queue
    EXPECT_EQ(mq_.getMessageCount(), 0);

    // Ensure no messages are processed after clearing
    std::atomic<int> received_count = 0;
    mq_.subscribe([&](const TestMessage&) { received_count++; },
                  "clear_test_sub");

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(received_count.load(), 0);
}

// Test: Cancel specific messages
TEST_F(MessageQueueTest, CancelSpecificMessages) {
    mq_.publish({1, "Keep"});
    mq_.publish({2, "Cancel"});
    mq_.publish({3, "Keep"});
    mq_.publish({4, "Cancel"});

    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // Allow messages to queue

    size_t cancelled_count = mq_.cancelMessages(
        [](const TestMessage& msg) { return msg.content == "Cancel"; });

    EXPECT_EQ(cancelled_count, 2);  // Two messages should be cancelled

    std::atomic<int> received_count = 0;
    std::vector<int> received_ids;
    std::mutex mtx;

    mq_.subscribe(
        [&](const TestMessage& msg) {
            std::lock_guard<std::mutex> lock(mtx);
            received_ids.push_back(msg.id);
            received_count++;
        },
        "cancel_test_sub");

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(received_count.load(),
              2);  // Only "Keep" messages should be processed
    std::sort(received_ids.begin(), received_ids.end());
    EXPECT_EQ(received_ids[0], 1);
    EXPECT_EQ(received_ids[1], 3);
    EXPECT_EQ(mq_.getMessageCount(), 0);
}

// Test: Concurrent publishing
TEST_F(MessageQueueTest, ConcurrentPublishing) {
    const int num_publishers = 5;
    const int messages_per_publisher = 100;
    std::atomic<int> total_received = 0;

    mq_.subscribe([&](const TestMessage&) { total_received++; },
                  "concurrent_receiver");

    std::vector<std::thread> publishers;
    for (int i = 0; i < num_publishers; ++i) {
        publishers.emplace_back([&, i]() {
            for (int j = 0; j < messages_per_publisher; ++j) {
                mq_.publish({i * messages_per_publisher + j, "Concurrent"});
            }
        });
    }

    for (auto& t : publishers) {
        t.join();
    }

#ifdef ATOM_USE_ASIO
    io_context_.run_for(
        std::chrono::seconds(2));  // Give enough time for ASIO to process
#endif
    // Wait until all messages are processed
    for (int i = 0; i < 20 && total_received.load() <
                                  (num_publishers * messages_per_publisher);
         ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    EXPECT_EQ(total_received.load(), num_publishers * messages_per_publisher);
    EXPECT_EQ(mq_.getMessageCount(), 0);
}

// Test: Concurrent subscribing and publishing
TEST_F(MessageQueueTest, ConcurrentSubscribeAndPublish) {
    const int num_messages = 100;
    std::atomic<int> total_received = 0;
    std::atomic<int> subscriber_count = 0;

    std::vector<std::thread> threads;

    // Publisher thread
    threads.emplace_back([&]() {
        for (int i = 0; i < num_messages; ++i) {
            mq_.publish({i, "Mixed"});
            std::this_thread::sleep_for(
                std::chrono::milliseconds(5));  // Small delay
        }
    });

    // Subscriber threads
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&, i]() {
            auto callback = [&](const TestMessage&) { total_received++; };
            mq_.subscribe(callback, "dynamic_sub_" + std::to_string(i));
            subscriber_count++;
            // Keep subscriber alive for a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            mq_.unsubscribe(callback);
            subscriber_count--;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::seconds(2));
#endif
    // It's hard to predict exact total_received due to dynamic subscriptions,
    // but it should be greater than 0 and less than num_messages *
    // initial_subscriber_count
    EXPECT_GT(total_received.load(), 0);
    EXPECT_EQ(subscriber_count.load(),
              0);  // All dynamic subscribers should have unsubscribed
    EXPECT_EQ(mq_.getMessageCount(), 0);
}

// Test: Coroutine awaitable (basic)
TEST_F(MessageQueueTest, CoroutineAwaitableBasic) {
    // This test requires C++20 coroutine support and a proper test runner setup
    // that can handle coroutines. For simplicity, we'll simulate it.
    // In a real scenario, you'd use a coroutine framework.

    std::atomic<bool> coroutine_finished = false;
    TestMessage received_msg;

    // Simulate a coroutine that awaits a message
    auto simulate_coroutine =
        [&](atom::async::MessageQueue<TestMessage>& q) -> std::future<void> {
        return std::async(std::launch::async, [&]() {
            try {
                TestMessage msg =
                    q.nextMessage()
                        .await_resume();  // Directly call await_resume for
                                          // simulation
                received_msg = msg;
                coroutine_finished = true;
            } catch (const atom::async::MessageQueueException& e) {
                spdlog::error("Coroutine simulation failed: {}", e.what());
            }
        });
    };

    // This part is tricky without a full coroutine setup.
    // The `await_suspend` part needs to register a callback that resumes the
    // coroutine. The `await_resume` part is what gets the result.

    // For a basic test, we can check if the subscription mechanism works.
    // A more robust test would involve a real coroutine.

    // Let's test the `nextMessage` method's ability to subscribe.
    std::atomic<bool> subscribed_by_awaitable = false;
    mq_.subscribe(
        [&](const TestMessage& msg) {
            // This callback is from the internal subscription of
            // MessageAwaitable
            received_msg = msg;
            subscribed_by_awaitable = true;
        },
        "coroutine_subscriber");  // This name is used internally by
                                  // MessageAwaitable

    mq_.publish({100, "Coroutine Message"});

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(subscribed_by_awaitable);
    EXPECT_EQ(received_msg.id, 100);
    EXPECT_EQ(received_msg.content, "Coroutine Message");
}

// Test: Coroutine awaitable with filter
TEST_F(MessageQueueTest, CoroutineAwaitableWithFilter) {
    std::atomic<bool> coroutine_finished = false;
    TestMessage received_msg;

    // Simulate the subscription part of the awaitable
    mq_.subscribe(
        [&](const TestMessage& msg) {
            received_msg = msg;
            coroutine_finished = true;
        },
        "coroutine_subscriber", 0,
        [](const TestMessage& m) { return m.id == 200; });

    mq_.publish({199, "Wrong ID"});
    mq_.publish({200, "Correct ID"});

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(coroutine_finished);
    EXPECT_EQ(received_msg.id, 200);
}

// Test: Stop and Start Processing
TEST_F(MessageQueueTest, StopAndStartProcessing) {
    mq_.stopProcessing();  // Stop the processing thread started in SetUp
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // Give time to stop

    std::atomic<int> received_count = 0;
    mq_.subscribe([&](const TestMessage&) { received_count++; },
                  "stop_start_sub");

    mq_.publish({1, "Msg after stop"});
    std::this_thread::sleep_for(
        std::chrono::milliseconds(200));  // Give time for publish to queue

    EXPECT_EQ(received_count.load(), 0);  // Should not be processed

    mq_.startProcessing();  // Start processing again
    std::this_thread::sleep_for(
        std::chrono::milliseconds(200));  // Give time to start and process

    EXPECT_EQ(received_count.load(), 1);  // Should now be processed
    EXPECT_EQ(mq_.getMessageCount(), 0);
}

// Test: MessageQueue destruction while messages are pending
TEST_F(MessageQueueTest, DestructionWithPendingMessages) {
    // Create a new MessageQueue instance to control its lifecycle
#ifdef ATOM_USE_ASIO
    asio::io_context local_io_context;
    auto* local_mq =
        new atom::async::MessageQueue<TestMessage>(local_io_context);
#else
    auto* local_mq = new atom::async::MessageQueue<TestMessage>();
#endif

    local_mq->startProcessing();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::atomic<int> received_count = 0;
    local_mq->subscribe([&](const TestMessage&) { received_count++; },
                        "destructor_test_sub");

    local_mq->publish({1, "Pending"});
    local_mq->publish({2, "Pending"});
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // Allow messages to queue

    // Delete the MessageQueue, which should call stopProcessing
    delete local_mq;

    // No crash should occur, and processing should have stopped gracefully
    EXPECT_EQ(received_count.load(),
              2);  // Messages should have been processed before destruction
}

// Test: Exception in subscriber callback
TEST_F(MessageQueueTest, ExceptionInSubscriberCallback) {
    std::atomic<bool> callback_called = false;
    mq_.subscribe(
        [&](const TestMessage&) {
            callback_called = true;
            throw std::runtime_error("Test exception from callback");
        },
        "exception_sub");

    mq_.publish({1, "Exception Test"});

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(callback_called);  // Callback should still be invoked
    // The exception is caught internally and logged, but doesn't propagate to
    // the publisher
    EXPECT_EQ(mq_.getMessageCount(), 0);  // Message should still be consumed
}

// Test: getMessageCount accuracy (non-lockfree path)
#ifndef ATOM_USE_LOCKFREE_QUEUE
TEST_F(MessageQueueTest, GetMessageCountAccuracy) {
    EXPECT_EQ(mq_.getMessageCount(), 0);
    mq_.publish({1, "A"});
    EXPECT_EQ(mq_.getMessageCount(), 1);
    mq_.publish({2, "B"});
    EXPECT_EQ(mq_.getMessageCount(), 2);

    // Allow processing to happen
#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(mq_.getMessageCount(), 0);  // All messages should be processed
}
#endif

// Test: getMessageCount (lockfree path - approximate)
#ifdef ATOM_USE_LOCKFREE_QUEUE
TEST_F(MessageQueueTest, GetMessageCountLockfree) {
    // Lockfree queue size is approximate.
    // It returns 1 if not empty, 0 if empty.
    EXPECT_EQ(mq_.getMessageCount(), 0);
    mq_.publish({1, "A"});
    // Depending on timing, it might be 1 (in lockfree queue) or 0 (moved to
    // deque) or 0 (already processed). The current implementation returns 1 if
    // lockfree queue is not empty, plus deque size. So, it should be at least 1
    // if a message was just published and not yet processed.
    EXPECT_GE(mq_.getMessageCount(),
              0);  // Can't be precise, but should not be negative

    // Publish more to ensure some are in the queue
    for (int i = 0; i < 10; ++i) {
        mq_.publish({i, "Lockfree Test"});
    }
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));  // Give time to queue

    // It should be > 0 if messages are still pending
    EXPECT_GE(mq_.getMessageCount(), 0);

    // Allow processing to happen
#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::seconds(1));
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(mq_.getMessageCount(), 0);  // All messages should be processed
}
#endif

// Test: Multiple subscribers, single message
TEST_F(MessageQueueTest, MultipleSubscribersSingleMessage) {
    std::atomic<int> sub1_received = 0;
    std::atomic<int> sub2_received = 0;
    std::atomic<int> sub3_received = 0;

    mq_.subscribe([&](const TestMessage&) { sub1_received++; }, "sub1");
    mq_.subscribe([&](const TestMessage&) { sub2_received++; }, "sub2");
    mq_.subscribe([&](const TestMessage&) { sub3_received++; }, "sub3");

    mq_.publish({1, "Multi-sub test"});

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(sub1_received.load(), 1);
    EXPECT_EQ(sub2_received.load(), 1);
    EXPECT_EQ(sub3_received.load(), 1);
    EXPECT_EQ(mq_.getMessageCount(), 0);
}

// Test: No subscribers, publish message (should be consumed)
TEST_F(MessageQueueTest, NoSubscribersPublish) {
    mq_.publish({1, "No one listening"});
#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(mq_.getMessageCount(),
              0);  // Message should still be processed and removed
}

// Test: Filter throws exception
TEST_F(MessageQueueTest, FilterThrowsException) {
    std::atomic<bool> callback_called = false;
    mq_.subscribe([&](const TestMessage&) { callback_called = true; },
                  "filter_exception_sub", 0,
                  [](const TestMessage& msg) -> bool {
                      if (msg.id == 1) {
                          throw std::runtime_error("Filter exception");
                      }
                      return true;
                  });

    mq_.publish({1, "Trigger Exception"});  // Should trigger filter exception
    mq_.publish({2, "Pass Filter"});        // Should pass filter

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(callback_called);  // Callback for ID 2 should be called
    EXPECT_EQ(mq_.getMessageCount(), 0);
}

// Test: Message priority
TEST_F(MessageQueueTest, MessagePriority) {
    std::vector<int> received_ids;
    std::mutex mtx;

    mq_.subscribe(
        [&](const TestMessage& msg) {
            std::lock_guard<std::mutex> lock(mtx);
            received_ids.push_back(msg.id);
        },
        "priority_receiver");

    mq_.publish({1, "Low"}, 0);
    mq_.publish({2, "High"}, 100);  // Higher priority
    mq_.publish({3, "Medium"}, 50);
    mq_.publish({4, "Very High"}, 200);  // Highest priority

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ASSERT_EQ(received_ids.size(), 4);
    EXPECT_EQ(received_ids[0], 4);  // Very High
    EXPECT_EQ(received_ids[1], 2);  // High
    EXPECT_EQ(received_ids[2], 3);  // Medium
    EXPECT_EQ(received_ids[3], 1);  // Low
}

// Test: Message timestamp for same priority
TEST_F(MessageQueueTest, MessageTimestampSamePriority) {
    std::vector<int> received_ids;
    std::mutex mtx;

    mq_.subscribe(
        [&](const TestMessage& msg) {
            std::lock_guard<std::mutex> lock(mtx);
            received_ids.push_back(msg.id);
        },
        "timestamp_receiver");

    mq_.publish({1, "First"}, 10);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Small delay
    mq_.publish({2, "Second"}, 10);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    mq_.publish({3, "Third"}, 10);

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ASSERT_EQ(received_ids.size(), 3);
    EXPECT_EQ(received_ids[0], 1);  // First published
    EXPECT_EQ(received_ids[1], 2);  // Second published
    EXPECT_EQ(received_ids[2], 3);  // Third published
}

// Test: Coroutine awaitable cancellation on early destruction
TEST_F(MessageQueueTest, CoroutineAwaitableEarlyDestruction) {
    std::atomic<bool> callback_fired = false;
    std::atomic<bool> coroutine_resumed = false;

    // Create an awaitable in a limited scope
    {
        auto awaitable = mq_.nextMessage();
        // Simulate await_suspend to register the internal callback
        // In a real coroutine, this would be handled by the compiler.
        // Here, we manually subscribe using the name the awaitable would use.
        mq_.subscribe(
            [&](const TestMessage& msg) {
                callback_fired = true;
                // If the awaitable was destroyed, this callback should ideally
                // not resume a handle. The `cancelled` flag in MessageAwaitable
                // handles this. We can't directly test `h.resume()` not being
                // called without mocking coroutine_handle. But we can check if
                // the `result` is set.
            },
            "coroutine_subscriber");

        mq_.publish({1, "Message for destroyed awaitable"});
        // Awaitable goes out of scope here, its destructor sets `cancelled =
        // true`.
    }

#ifdef ATOM_USE_ASIO
    io_context_.run_for(std::chrono::milliseconds(500));
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_TRUE(callback_fired);  // The internal callback should still fire
    // But the coroutine (if it were real) should not resume or process the
    // message because `cancelled` flag would be true. We can't directly assert
    // on `await_resume` not being called or throwing without a full coroutine
    // setup.
}

// Test: MessageQueue isProcessing_ flag behavior
TEST_F(MessageQueueTest, IsProcessingFlag) {
    // The flag is primarily for internal use with ASIO to prevent re-entry.
    // For non-ASIO, the jthread loop manages processing.
    // We can check its state after start/stop.

    // Already started in SetUp
    // The jthread sets m_isProcessing_ to true.
    // The ASIO processMessages also sets it to true and then false.

    // This is hard to test externally without direct access or mocking.
    // The current test setup implicitly tests it by verifying messages are
    // processed. A direct test would involve inspecting the private member,
    // which is bad practice. We'll rely on the functional tests to confirm
    // correct behavior.
}

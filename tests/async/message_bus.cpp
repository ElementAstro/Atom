#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <asio/io_context.hpp>
#include <asio/thread_pool.hpp>
#include <atomic>
#include <string>
#include <thread>

#include "message_bus.hpp"

using namespace atom::async;
using namespace std::chrono_literals;

// Define a simple message struct
struct TestMessage {
    int value;
    std::string name;

    bool operator==(const TestMessage& other) const {
        return value == other.value && name == other.name;
    }
};

// Define another message struct
struct AnotherMessage {
    double data;
};

// Test fixture for MessageBus
class MessageBusTest : public ::testing::Test {
protected:
    asio::io_context io_context;
    std::unique_ptr<MessageBus> bus;
    asio::thread_pool pool{1};  // For running io_context

    void SetUp() override {
#ifdef ATOM_USE_ASIO
        bus = std::make_unique<MessageBus>(io_context);
#else
        bus = std::make_unique<MessageBus>();
#endif
        // Run io_context in a separate thread for async operations
        io_context_thread = std::thread([this]() { io_context.run(); });
    }

    void TearDown() override {
        io_context.stop();
        if (io_context_thread.joinable()) {
            io_context_thread.join();
        }
#ifdef ATOM_USE_LOCKFREE_QUEUE
        // Ensure processing is stopped and thread joined if it was started
        bus->stopMessageProcessing();
#endif
    }

    std::thread io_context_thread;
};

// Test: Basic publish and subscribe (synchronous)
TEST_F(MessageBusTest, BasicPublishSubscribe) {
    int receivedValue = 0;
    bool handlerCalled = false;

    (void)bus->subscribe<TestMessage>(
        "test.message",
        [&](const TestMessage& msg) {
            receivedValue = msg.value;
            handlerCalled = true;
        },
        false  // Synchronous handler
    );

    bus->publish<TestMessage>("test.message", {123, "hello"});

    EXPECT_TRUE(handlerCalled);
    EXPECT_EQ(receivedValue, 123);
}

// Test: Publish and subscribe with different message types
TEST_F(MessageBusTest, DifferentMessageTypes) {
    int receivedInt = 0;
    double receivedDouble = 0.0;

    (void)bus->subscribe<TestMessage>(
        "int.message", [&](const TestMessage& msg) { receivedInt = msg.value; },
        false);
    (void)bus->subscribe<AnotherMessage>(
        "double.message",
        [&](const AnotherMessage& msg) { receivedDouble = msg.data; }, false);

    bus->publish<TestMessage>("int.message", {456, "test"});
    bus->publish<AnotherMessage>("double.message", {789.0});

    EXPECT_EQ(receivedInt, 456);
    EXPECT_EQ(receivedDouble, 789.0);
}

// Test: Multiple subscribers to the same message
TEST_F(MessageBusTest, MultipleSubscribers) {
    int count = 0;
    (void)bus->subscribe<TestMessage>(
        "multi.message", [&](const TestMessage&) { count++; }, false);
    (void)bus->subscribe<TestMessage>(
        "multi.message", [&](const TestMessage&) { count++; }, false);
    (void)bus->subscribe<TestMessage>(
        "multi.message", [&](const TestMessage&) { count++; }, false);

    bus->publish<TestMessage>("multi.message", {1, "a"});
    EXPECT_EQ(count, 3);
}

// Test: Unsubscribe using token
TEST_F(MessageBusTest, UnsubscribeByToken) {
    int callCount = 0;
    auto token1 = bus->subscribe<TestMessage>(
        "unsubscribe.message", [&](const TestMessage&) { callCount++; }, false);
    auto token2 = bus->subscribe<TestMessage>(
        "unsubscribe.message", [&](const TestMessage&) { callCount++; }, false);

    bus->publish<TestMessage>("unsubscribe.message", {1, "a"});
    EXPECT_EQ(callCount, 2);

    bus->unsubscribe<TestMessage>(token1);
    bus->publish<TestMessage>("unsubscribe.message", {1, "a"});
    EXPECT_EQ(callCount, 3);  // Only token2's handler should be called

    bus->unsubscribe<TestMessage>(token2);
    bus->publish<TestMessage>("unsubscribe.message", {1, "a"});
    EXPECT_EQ(callCount, 3);  // No handlers should be called now
}

// Test: Unsubscribe all for a specific message name
TEST_F(MessageBusTest, UnsubscribeAllByName) {
    int callCount = 0;
    (void)bus->subscribe<TestMessage>(
        "all.message", [&](const TestMessage&) { callCount++; }, false);
    (void)bus->subscribe<TestMessage>(
        "all.message", [&](const TestMessage&) { callCount++; }, false);
    (void)bus->subscribe<AnotherMessage>(
        "all.message", [&](const AnotherMessage&) { callCount += 10; }, false);

    bus->publish<TestMessage>("all.message", {1, "a"});
    EXPECT_EQ(callCount, 2);

    bus->unsubscribeAll<TestMessage>("all.message");
    bus->publish<TestMessage>("all.message", {1, "a"});
    EXPECT_EQ(callCount, 2);  // TestMessage handlers should not be called

    bus->publish<AnotherMessage>("all.message", {1.0});
    EXPECT_EQ(callCount, 12);  // AnotherMessage handler should still be called
}

// Test: Subscribe with 'once' option
TEST_F(MessageBusTest, SubscribeOnce) {
    int callCount = 0;
    (void)bus->subscribe<TestMessage>(
        "once.message", [&](const TestMessage&) { callCount++; }, false, true);

    bus->publish<TestMessage>("once.message", {1, "a"});
    EXPECT_EQ(callCount, 1);

    bus->publish<TestMessage>("once.message", {1, "a"});
    EXPECT_EQ(callCount, 1);  // Should not be called again
}

// Test: Subscribe with filter
TEST_F(MessageBusTest, SubscribeWithFilter) {
    int receivedValue = 0;
    (void)bus->subscribe<TestMessage>(
        "filter.message",
        [&](const TestMessage& msg) { receivedValue = msg.value; }, false,
        false, [&](const TestMessage& msg) { return msg.value > 50; });

    bus->publish<TestMessage>("filter.message", {30, "low"});
    EXPECT_EQ(receivedValue, 0);  // Filter should block this

    bus->publish<TestMessage>("filter.message", {70, "high"});
    EXPECT_EQ(receivedValue, 70);  // Filter should allow this
}

// Test: Publish with delay (non-Asio)
TEST_F(MessageBusTest, PublishWithDelayNonAsio) {
#ifndef ATOM_USE_ASIO
    std::atomic<int> receivedValue = 0;
    (void)bus->subscribe<TestMessage>(
        "delayed.message",
        [&](const TestMessage& msg) { receivedValue = msg.value; }, false);

    bus->publish<TestMessage>("delayed.message", {99, "delayed"},
                              100ms);  // 100ms delay

    EXPECT_EQ(receivedValue, 0);  // Should not have been received immediately
    std::this_thread::sleep_for(150ms);  // Wait for message to be processed
    EXPECT_EQ(receivedValue, 99);
#else
    // This test is specifically for non-Asio delayed publish, skip if Asio is
    // used
    GTEST_SKIP()
        << "Skipping PublishWithDelayNonAsio test as ATOM_USE_ASIO is defined.";
#endif
}

// Test: Publish with delay (Asio)
TEST_F(MessageBusTest, PublishWithDelayAsio) {
#ifdef ATOM_USE_ASIO
    std::atomic<int> receivedValue = 0;
    (void)bus->subscribe<TestMessage>(
        "delayed.message.asio",
        [&](const TestMessage& msg) { receivedValue = msg.value; }, false);

    bus->publish<TestMessage>("delayed.message.asio", {100, "delayed_asio"},
                              100ms);  // 100ms delay

    EXPECT_EQ(receivedValue, 0);  // Should not have been received immediately
    std::this_thread::sleep_for(150ms);  // Wait for message to be processed
    EXPECT_EQ(receivedValue, 100);
#else
    // This test is specifically for Asio delayed publish, skip if Asio is not
    // used
    GTEST_SKIP() << "Skipping PublishWithDelayAsio test as ATOM_USE_ASIO is "
                    "not defined.";
#endif
}

// Test: Publish to namespace subscribers
TEST_F(MessageBusTest, NamespaceSubscription) {
    int count = 0;
    (void)bus->subscribe<TestMessage>(
        "my.namespace", [&](const TestMessage&) { count++; },
        false);  // Subscribes to namespace
    (void)bus->subscribe<TestMessage>(
        "my.namespace.sub", [&](const TestMessage&) { count += 10; },
        false);  // Subscribes to specific name

    bus->publish<TestMessage>("my.namespace.event1", {1, "event1"});
    EXPECT_EQ(count, 1);  // Only namespace handler should be called

    bus->publish<TestMessage>("my.namespace.sub", {2, "event2"});
    EXPECT_EQ(count,
              12);  // Both namespace and specific handler should be called
}

// Test: Clear all subscribers
TEST_F(MessageBusTest, ClearAllSubscribers) {
    int callCount = 0;
    (void)bus->subscribe<TestMessage>(
        "clear.message", [&](const TestMessage&) { callCount++; }, false);
    (void)bus->subscribe<AnotherMessage>(
        "another.clear.message",
        [&](const AnotherMessage&) { callCount += 10; }, false);

    bus->publish<TestMessage>("clear.message", {1, "a"});
    bus->publish<AnotherMessage>("another.clear.message", {1.0});
    EXPECT_EQ(callCount, 11);

    bus->clearAllSubscribers();
    callCount = 0;  // Reset count to check if new publishes are ignored

    bus->publish<TestMessage>("clear.message", {1, "a"});
    bus->publish<AnotherMessage>("another.clear.message", {1.0});
    EXPECT_EQ(callCount, 0);  // No handlers should be called after clearing
}

// Test: Get subscriber count
TEST_F(MessageBusTest, GetSubscriberCount) {
    EXPECT_EQ(bus->getSubscriberCount<TestMessage>("non.existent"), 0);

    (void)bus->subscribe<TestMessage>(
        "count.message", [](const TestMessage&) {}, false);
    EXPECT_EQ(bus->getSubscriberCount<TestMessage>("count.message"), 1);

    (void)bus->subscribe<TestMessage>(
        "count.message", [](const TestMessage&) {}, false);
    EXPECT_EQ(bus->getSubscriberCount<TestMessage>("count.message"), 2);

    (void)bus->subscribe<AnotherMessage>(
        "count.message", [](const AnotherMessage&) {}, false);
    EXPECT_EQ(bus->getSubscriberCount<TestMessage>("count.message"),
              2);  // Different type, same name
    EXPECT_EQ(bus->getSubscriberCount<AnotherMessage>("count.message"), 1);
}

// Test: Has subscriber
TEST_F(MessageBusTest, HasSubscriber) {
    EXPECT_FALSE(bus->hasSubscriber<TestMessage>("non.existent"));

    (void)bus->subscribe<TestMessage>(
        "has.message", [](const TestMessage&) {}, false);
    EXPECT_TRUE(bus->hasSubscriber<TestMessage>("has.message"));

    bus->unsubscribeAll<TestMessage>("has.message");
    EXPECT_FALSE(bus->hasSubscriber<TestMessage>("has.message"));
}

// Test: Message history
TEST_F(MessageBusTest, MessageHistory) {
    bus->publish<TestMessage>("history.message", {1, "first"});
    bus->publish<TestMessage>("history.message", {2, "second"});
    bus->publish<TestMessage>("history.message", {3, "third"});

    auto history = bus->getMessageHistory<TestMessage>("history.message");
    EXPECT_EQ(history.size(), 3);
    EXPECT_EQ(history[0].value, 1);
    EXPECT_EQ(history[1].value, 2);
    EXPECT_EQ(history[2].value, 3);

    // Test history limit
    for (int i = 0; i < 150; ++i) {
        bus->publish<TestMessage>("long.history", {i, "data"});
    }
    auto longHistory = bus->getMessageHistory<TestMessage>("long.history");
    EXPECT_EQ(longHistory.size(), MessageBus::K_MAX_HISTORY_SIZE);
    EXPECT_EQ(longHistory[0].value,
              50);  // Should contain the last 100 messages
    EXPECT_EQ(longHistory[99].value, 149);
}

// Test: Global publish
TEST_F(MessageBusTest, GlobalPublish) {
    int count1 = 0;
    int count2 = 0;
    int count3 = 0;

    (void)bus->subscribe<TestMessage>(
        "global.msg1", [&](const TestMessage&) { count1++; }, false);
    (void)bus->subscribe<TestMessage>(
        "global.msg2", [&](const TestMessage&) { count2++; }, false);
    (void)bus->subscribe<AnotherMessage>(
        "global.msg3", [&](const AnotherMessage&) { count3++; }, false);

    bus->publishGlobal<TestMessage>({10, "global"});

    // Give some time for async operations if any, though these are sync
    std::this_thread::sleep_for(10ms);

    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);
    EXPECT_EQ(count3, 0);  // Should not be called for AnotherMessage
}

// Test: Get active namespaces
TEST_F(MessageBusTest, GetActiveNamespaces) {
    (void)bus->subscribe<TestMessage>(
        "ns1.event", [](const TestMessage&) {}, false);
    (void)bus->subscribe<TestMessage>(
        "ns2.sub.event", [](const TestMessage&) {}, false);
    (void)bus->subscribe<AnotherMessage>(
        "ns1.other", [](const AnotherMessage&) {}, false);
    (void)bus->subscribe<TestMessage>(
        "standalone", [](const TestMessage&) {}, false);

    auto namespaces = bus->getActiveNamespaces();
    std::sort(namespaces.begin(), namespaces.end());

    std::vector<std::string> expectedNamespaces = {"ns1", "ns2", "standalone"};
    std::sort(expectedNamespaces.begin(), expectedNamespaces.end());

    EXPECT_EQ(namespaces, expectedNamespaces);
}

// Test: Statistics
TEST_F(MessageBusTest, GetStatistics) {
    auto stats = bus->getStatistics();
    EXPECT_EQ(stats.subscriberCount, 0);
    EXPECT_EQ(stats.typeCount, 0);
    EXPECT_EQ(stats.namespaceCount, 0);
    EXPECT_EQ(stats.historyTotalMessages, 0);

    (void)bus->subscribe<TestMessage>(
        "stat.msg1", [](const TestMessage&) {}, false);
    (void)bus->subscribe<TestMessage>(
        "stat.msg1", [](const TestMessage&) {}, false);
    (void)bus->subscribe<AnotherMessage>(
        "stat.msg2", [](const AnotherMessage&) {}, false);

    stats = bus->getStatistics();
    EXPECT_EQ(stats.subscriberCount, 3);
    EXPECT_EQ(stats.typeCount, 2);  // TestMessage and AnotherMessage
    EXPECT_EQ(stats.namespaceCount,
              2);  // "stat" and "stat" (from stat.msg1 and stat.msg2)

    bus->publish<TestMessage>("stat.msg1", {1, "a"});
    bus->publish<AnotherMessage>("stat.msg2", {2.0});

    stats = bus->getStatistics();
    EXPECT_EQ(stats.historyTotalMessages, 2);
}

// Test: Exception handling in handlers/filters
TEST_F(MessageBusTest, HandlerFilterExceptions) {
    // This test primarily checks that exceptions don't crash the bus,
    // but are logged. We can't directly assert on spdlog output without
    // mocking spdlog, so this is more of a crash-prevention test.
    (void)bus->subscribe<TestMessage>(
        "exception.message",
        [&](const TestMessage& msg) {
            if (msg.value == 1) {
                throw std::runtime_error("Handler error!");
            }
        },
        false, false,
        [&](const TestMessage& msg) {
            if (msg.value == 2) {
                throw std::runtime_error("Filter error!");
            }
            return true;
        });

    // Publish a message that triggers handler exception
    EXPECT_NO_THROW(
        bus->publish<TestMessage>("exception.message", {1, "test"}));

    // Publish a message that triggers filter exception
    EXPECT_NO_THROW(
        bus->publish<TestMessage>("exception.message", {2, "test"}));

    // Publish a message that works fine
    EXPECT_NO_THROW(
        bus->publish<TestMessage>("exception.message", {3, "test"}));
}

// Test: Thread safety of publish/subscribe (basic)
TEST_F(MessageBusTest, ThreadSafetyBasic) {
    std::atomic<int> counter = 0;
    const int num_threads = 5;
    const int messages_per_thread = 100;

    (void)bus->subscribe<TestMessage>(
        "thread.safe.message", [&](const TestMessage&) { counter++; }, false);

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                bus->publish<TestMessage>("thread.safe.message", {j, "data"});
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Give some time for any pending async operations if ATOM_USE_ASIO is
    // defined
    std::this_thread::sleep_for(50ms);

    EXPECT_EQ(counter, num_threads * messages_per_thread);
}

#if defined(ATOM_COROUTINE_SUPPORT) && defined(ATOM_USE_ASIO)
// Test: Coroutine support (receiveAsync)
TEST_F(MessageBusTest, CoroutineReceiveAsync) {
    auto coro_func = [&](std::string name,
                         int expected_value) -> asio::awaitable<void> {
        try {
            TestMessage msg = co_await bus->receiveAsync<TestMessage>(name);
            EXPECT_EQ(msg.value, expected_value);
            EXPECT_EQ(msg.name, "coro_test");
        } catch (const MessageBusException& e) {
            FAIL() << "Coroutine failed to receive message: " << e.what();
        }
    };

    // Run the coroutine
    asio::co_spawn(io_context, coro_func("coro.message", 42), asio::detached);

    // Publish the message after a short delay to ensure coroutine is awaiting
    asio::post(io_context, [&]() {
        bus->publish<TestMessage>("coro.message", {42, "coro_test"});
    });

    // Give io_context time to process
    std::this_thread::sleep_for(100ms);
}

// Test: Coroutine receiveAsync with no message (should throw)
TEST_F(MessageBusTest, CoroutineReceiveAsyncNoMessage) {
    auto coro_func = [&](std::string name) -> asio::awaitable<void> {
        try {
            co_await bus->receiveAsync<TestMessage>(name);
            FAIL() << "Coroutine should have thrown MessageBusException";
        } catch (const MessageBusException& e) {
            EXPECT_STREQ(e.what(), "No message received in coroutine");
        } catch (const std::exception& e) {
            FAIL() << "Unexpected exception: " << e.what();
        }
    };

    // Run the coroutine
    asio::co_spawn(io_context, coro_func("nonexistent.coro.message"),
                   asio::detached);

    // Give io_context time to process and for the awaitable to clean up
    std::this_thread::sleep_for(100ms);
}

// Test: Coroutine receiveAsync cleanup on destruction
TEST_F(MessageBusTest, CoroutineReceiveAsyncCleanup) {
    // This is hard to test directly without inspecting internal state.
    // The destructor of MessageAwaitable calls unsubscribe.
    // We can check if the subscriber count goes down.
    EXPECT_EQ(bus->getSubscriberCount<TestMessage>("cleanup.message"), 0);

    // Create a future to hold the coroutine result
    std::promise<void> promise;
    std::future<void> future = promise.get_future();

    auto coro_func = [&](std::string name,
                         std::promise<void>& p) -> asio::awaitable<void> {
        try {
            // Await a message that will never come
            co_await bus->receiveAsync<TestMessage>(name);
            p.set_value();  // Should not be reached
        } catch (const MessageBusException&) {
            p.set_value();  // Expected exception on no message
        } catch (...) {
            p.set_exception(std::current_exception());
        }
    };

    asio::co_spawn(io_context, coro_func("cleanup.message", promise),
                   asio::detached);

    // Give time for subscription to register
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(bus->getSubscriberCount<TestMessage>("cleanup.message"), 1);

    // Let the coroutine complete (by throwing or being destroyed)
    // In a real scenario, if the coroutine handle is destroyed, it should clean
    // up. Here, we let it run to its expected exception path.
    future.wait_for(
        200ms);  // Wait for the coroutine to finish (and unsubscribe)

    // After the coroutine finishes (or is destroyed), the subscription should
    // be gone
    EXPECT_EQ(bus->getSubscriberCount<TestMessage>("cleanup.message"), 0);
}

#endif  // ATOM_COROUTINE_SUPPORT && ATOM_USE_ASIO

#ifdef ATOM_USE_LOCKFREE_QUEUE
// Test: Lock-free queue processing
TEST_F(MessageBusTest, LockFreeQueueProcessing) {
    std::atomic<int> receivedCount = 0;
    (void)bus->subscribe<TestMessage>(
        "lockfree.message", [&](const TestMessage&) { receivedCount++; },
        false);

    // Publish messages, they should go into the queue
    for (int i = 0; i < 50; ++i) {
        bus->publish<TestMessage>("lockfree.message", {i, "data"});
    }

    // Give some time for the processing thread to pick up messages
    std::this_thread::sleep_for(200ms);

    EXPECT_EQ(receivedCount, 50);

    // Test fallback to synchronous processing if queue is full
    // This is hard to reliably test as queue size is dynamic and depends on
    // Boost.Lockfree implementation. We can try to flood it and check if
    // messages are still processed.
    receivedCount = 0;
    for (int i = 0; i < 2000; ++i) {  // Publish more than queue capacity (1024)
        bus->publish<TestMessage>("lockfree.message", {i, "flood"});
    }
    std::this_thread::sleep_for(500ms);  // Give ample time
    EXPECT_EQ(receivedCount,
              2000);  // All should be processed, either via queue or fallback
}

// Test: start/stop message processing
TEST_F(MessageBusTest, StartStopMessageProcessing) {
    std::atomic<int> receivedCount = 0;
    (void)bus->subscribe<TestMessage>(
        "startstop.message", [&](const TestMessage&) { receivedCount++; },
        false);

    bus->stopMessageProcessing();  // Ensure it's stopped

    bus->publish<TestMessage>("startstop.message", {1, "a"});
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(receivedCount, 0);  // Should not be processed if stopped

    bus->startMessageProcessing();  // Start processing
    bus->publish<TestMessage>("startstop.message", {2, "b"});
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(receivedCount,
              1);  // Should be processed now (the second message)

    // The first message might be processed if it fell back to sync publish,
    // but if it was queued before stop, it might be processed after start.
    // For this test, we assume it was not processed.
    // Let's re-verify by publishing another message after start.
    bus->publish<TestMessage>("startstop.message", {3, "c"});
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(receivedCount, 2);  // Now two messages processed after start
}

#endif  // ATOM_USE_LOCKFREE_QUEUE

// Test: Subscribing with empty name
TEST_F(MessageBusTest, SubscribeEmptyName) {
    EXPECT_THROW((void)bus->subscribe<TestMessage>(
                     "", [](const TestMessage&) {}, false),
                 MessageBusException);
}

// Test: Subscribing with null handler
TEST_F(MessageBusTest, SubscribeNullHandler) {
    EXPECT_THROW((void)bus->subscribe<TestMessage>("test.name", nullptr, false),
                 MessageBusException);
}

// Test: Publishing with empty name
TEST_F(MessageBusTest, PublishEmptyName) {
    EXPECT_THROW(bus->publish<TestMessage>("", {1, "a"}), MessageBusException);
}

// Test: Max subscribers per message
TEST_F(MessageBusTest, MaxSubscribersPerMessage) {
    for (size_t i = 0; i < MessageBus::K_MAX_SUBSCRIBERS_PER_MESSAGE; ++i) {
        (void)bus->subscribe<TestMessage>(
            "max.subscribers", [](const TestMessage&) {}, false);
    }
    EXPECT_EQ(bus->getSubscriberCount<TestMessage>("max.subscribers"),
              MessageBus::K_MAX_SUBSCRIBERS_PER_MESSAGE);

    // Next subscription should throw
    EXPECT_THROW((void)bus->subscribe<TestMessage>(
                     "max.subscribers", [](const TestMessage&) {}, false),
                 MessageBusException);
}

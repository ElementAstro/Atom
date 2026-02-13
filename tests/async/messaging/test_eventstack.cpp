#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include "atom/async/messaging/eventstack.hpp"

TEST(EventStackTest, PushEvent) {
    atom::async::EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);

    ASSERT_EQ(stack.size(), 3);
}

TEST(EventStackTest, PopEvent) {
    atom::async::EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);

    ASSERT_EQ(stack.popEvent().value(), 3);
    ASSERT_EQ(stack.popEvent().value(), 2);
    ASSERT_EQ(stack.popEvent().value(), 1);
    ASSERT_TRUE(stack.popEvent().has_value() == false);
}

TEST(EventStackTest, IsEmpty) {
    atom::async::EventStack<int> stack;

    ASSERT_TRUE(stack.isEmpty());

    stack.pushEvent(1);
    ASSERT_FALSE(stack.isEmpty());
}

TEST(EventStackTest, Size) {
    atom::async::EventStack<int> stack;

    ASSERT_EQ(stack.size(), 0);

    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);

    ASSERT_EQ(stack.size(), 3);
}

TEST(EventStackTest, ClearEvents) {
    atom::async::EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);

    ASSERT_EQ(stack.size(), 3);

    stack.clearEvents();

    ASSERT_EQ(stack.size(), 0);
}

TEST(EventStackTest, PeekTopEvent) {
    atom::async::EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);

    ASSERT_EQ(stack.peekTopEvent().value(), 3);
    ASSERT_EQ(stack.size(), 3);
}

TEST(EventStackTest, CopyStack) {
    atom::async::EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);

    atom::async::EventStack<int> copiedStack = stack.copyStack();

    ASSERT_EQ(copiedStack.size(), 3);
    ASSERT_EQ(copiedStack.peekTopEvent().value(), 3);
}

TEST(EventStackTest, FilterEvents) {
    atom::async::EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);

    stack.filterEvents([](const int& event) { return event % 2 == 0; });

    ASSERT_EQ(stack.size(), 1);
    ASSERT_EQ(stack.peekTopEvent().value(), 2);
}

TEST(EventStackTest, SerializeStack) {
    atom::async::EventStack<std::string> stack;
    stack.pushEvent("event1");
    stack.pushEvent("event2");
    stack.pushEvent("event3");

    std::string serializedStack = stack.serializeStack();

    ASSERT_EQ(serializedStack, "event1;event2;event3;");
}

TEST(EventStackTest, DeserializeStack) {
    atom::async::EventStack<std::string> stack;
    std::string serializedData = "event1;event2;event3;";

    stack.deserializeStack(serializedData);

    ASSERT_EQ(stack.size(), 3);
    ASSERT_EQ(stack.peekTopEvent().value(), "event3");
}

TEST(EventStackTest, RemoveDuplicates) {
    atom::async::EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(2);
    stack.pushEvent(3);
    stack.pushEvent(3);

    ASSERT_EQ(stack.size(), 5);

    stack.removeDuplicates();

    ASSERT_EQ(stack.size(), 3);
}

TEST(EventStackTest, SortEvents) {
    atom::async::EventStack<int> stack;
    stack.pushEvent(3);
    stack.pushEvent(1);
    stack.pushEvent(2);

    stack.sortEvents([](const int& a, const int& b) { return a < b; });

    ASSERT_EQ(stack.peekTopEvent().value(), 1);
}

TEST(EventStackTest, ReverseEvents) {
    atom::async::EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);

    stack.reverseEvents();

    ASSERT_EQ(stack.peekTopEvent().value(), 3);
}

TEST(EventStackTest, CountEvents) {
    atom::async::EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(2);
    stack.pushEvent(3);

    ASSERT_EQ(stack.countEvents([](const int& event) { return event == 2; }),
              2);
}

TEST(EventStackTest, FindEvent) {
    atom::async::EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);

    ASSERT_EQ(
        stack.findEvent([](const int& event) { return event == 2; }).value(),
        2);
}

TEST(EventStackTest, AnyEvent) {
    atom::async::EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);

    ASSERT_TRUE(stack.anyEvent([](const int& event) { return event > 2; }));
}

TEST(EventStackTest, AllEvents) {
    atom::async::EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);

    ASSERT_TRUE(stack.allEvents([](const int& event) { return event >= 1; }));
}

// =============================================================================
// Additional EventStack Tests
// =============================================================================

TEST(EventStackTest, ComplexType) {
    struct Event {
        int id;
        std::string name;

        bool operator==(const Event& other) const {
            return id == other.id && name == other.name;
        }
    };

    atom::async::EventStack<Event> stack;
    stack.pushEvent({1, "event1"});
    stack.pushEvent({2, "event2"});
    stack.pushEvent({3, "event3"});

    ASSERT_EQ(stack.size(), 3);

    auto top = stack.peekTopEvent();
    ASSERT_TRUE(top.has_value());
    ASSERT_EQ(top.value().id, 3);
    ASSERT_EQ(top.value().name, "event3");
}

TEST(EventStackTest, EmptyStackOperations) {
    atom::async::EventStack<int> stack;

    ASSERT_TRUE(stack.isEmpty());
    ASSERT_FALSE(stack.popEvent().has_value());
    ASSERT_FALSE(stack.peekTopEvent().has_value());
    ASSERT_EQ(stack.size(), 0);
}

TEST(EventStackTest, ConcurrentPush) {
    atom::async::EventStack<int> stack;
    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int eventsPerThread = 100;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&stack, t, eventsPerThread]() {
            for (int i = 0; i < eventsPerThread; ++i) {
                stack.pushEvent(t * 1000 + i);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    ASSERT_EQ(stack.size(), numThreads * eventsPerThread);
}

TEST(EventStackTest, ConcurrentPushPop) {
    atom::async::EventStack<int> stack;
    std::atomic<int> pushCount{0};
    std::atomic<int> popCount{0};

    std::vector<std::thread> threads;
    const int numThreads = 10;
    const int opsPerThread = 100;

    // Push threads
    for (int t = 0; t < numThreads / 2; ++t) {
        threads.emplace_back([&stack, &pushCount, opsPerThread]() {
            for (int i = 0; i < opsPerThread; ++i) {
                stack.pushEvent(i);
                pushCount.fetch_add(1);
            }
        });
    }

    // Pop threads
    for (int t = 0; t < numThreads / 2; ++t) {
        threads.emplace_back([&stack, &popCount, opsPerThread]() {
            for (int i = 0; i < opsPerThread; ++i) {
                if (stack.popEvent().has_value()) {
                    popCount.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Drain remaining
    while (stack.popEvent().has_value()) {
        popCount.fetch_add(1);
    }

    ASSERT_EQ(pushCount.load(), popCount.load());
}

TEST(EventStackTest, FilterEventsComplex) {
    atom::async::EventStack<int> stack;
    for (int i = 1; i <= 10; ++i) {
        stack.pushEvent(i);
    }

    // Keep only values > 5
    stack.filterEvents([](const int& event) { return event > 5; });

    ASSERT_EQ(stack.size(), 5);

    // All remaining should be > 5
    while (auto event = stack.popEvent()) {
        ASSERT_GT(event.value(), 5);
    }
}

TEST(EventStackTest, TransformEvents) {
    atom::async::EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);

    // Transform: multiply by 2
    stack.transformEvents([](int& event) { event *= 2; });

    ASSERT_EQ(stack.popEvent().value(), 6);  // 3 * 2
    ASSERT_EQ(stack.popEvent().value(), 4);  // 2 * 2
    ASSERT_EQ(stack.popEvent().value(), 2);  // 1 * 2
}

TEST(EventStackTest, MergeStacks) {
    atom::async::EventStack<int> stack1;
    stack1.pushEvent(1);
    stack1.pushEvent(2);

    atom::async::EventStack<int> stack2;
    stack2.pushEvent(3);
    stack2.pushEvent(4);

    stack1.mergeStack(stack2);

    ASSERT_EQ(stack1.size(), 4);
}

TEST(EventStackTest, NoneEvent) {
    atom::async::EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);

    ASSERT_TRUE(stack.noneEvent([](const int& event) { return event > 10; }));
    ASSERT_FALSE(stack.noneEvent([](const int& event) { return event == 2; }));
}

TEST(EventStackTest, LargeStack) {
    atom::async::EventStack<int> stack;
    const int numEvents = 10000;

    for (int i = 0; i < numEvents; ++i) {
        stack.pushEvent(i);
    }

    ASSERT_EQ(stack.size(), numEvents);

    // Pop all
    int count = 0;
    while (stack.popEvent().has_value()) {
        count++;
    }

    ASSERT_EQ(count, numEvents);
    ASSERT_TRUE(stack.isEmpty());
}

TEST(EventStackTest, StringEvents) {
    atom::async::EventStack<std::string> stack;
    stack.pushEvent("first");
    stack.pushEvent("second");
    stack.pushEvent("third");

    ASSERT_EQ(stack.peekTopEvent().value(), "third");
    ASSERT_EQ(stack.popEvent().value(), "third");
    ASSERT_EQ(stack.popEvent().value(), "second");
    ASSERT_EQ(stack.popEvent().value(), "first");
}

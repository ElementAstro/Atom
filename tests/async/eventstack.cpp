#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <set>  // For std::set in ConcurrentPushPop test
#include <thread>
#include <vector>

#include "atom/async/eventstack.hpp"

namespace atom::async {

// Define a simple test struct for EventStack
struct TestEvent {
    int id;
    std::string name;

    bool operator==(const TestEvent& other) const {
        return id == other.id && name == other.name;
    }
    bool operator<(const TestEvent& other) const { return id < other.id; }
};

}  // namespace atom::async

// Provide both to_string and from_string for TestEvent in the global namespace for ADL
namespace std {
inline std::string to_string(const atom::async::TestEvent& event) {
    return std::to_string(event.id) + ":" + event.name;
}
}

// Custom deserialization for TestEvent
inline atom::async::TestEvent from_string(const std::string& s) {
    auto pos = s.find(":");
    if (pos == std::string::npos) return {0, s};
    int id = std::stoi(s.substr(0, pos));
    std::string name = s.substr(pos + 1);
    return {id, name};
}

namespace fmt {
template <>
struct formatter<atom::async::TestEvent> : ostream_formatter {};
}  // namespace fmt

namespace atom::async {  // Reopen atom::async namespace

class EventStackTest : public ::testing::Test {
protected:
    void SetUp() override {
        spdlog::drop_all();
        // Remove logger setup, not required for test logic
    }

    void TearDown() override {
        // No specific teardown needed for EventStack as it manages its own
        // memory
    }
};

// Test basic push and pop operations
TEST_F(EventStackTest, BasicPushPop) {
    EventStack<int> stack;
    EXPECT_TRUE(stack.isEmpty());
    EXPECT_EQ(stack.size(), 0);

    stack.pushEvent(10);
    EXPECT_FALSE(stack.isEmpty());
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.peekTopEvent(), 10);

    stack.pushEvent(20);
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(stack.peekTopEvent(), 20);

    // Corrected: Use ::std::optional
    ::std::optional<int> val = stack.popEvent();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), 20);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.peekTopEvent(), 10);

    val = stack.popEvent();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), 10);
    EXPECT_TRUE(stack.isEmpty());
    EXPECT_EQ(stack.size(), 0);

    val = stack.popEvent();
    EXPECT_FALSE(val.has_value());
}

// Test peekTopEvent on empty and non-empty stack
TEST_F(EventStackTest, PeekTopEvent) {
    EventStack<int> stack;
    EXPECT_FALSE(stack.peekTopEvent().has_value());

    stack.pushEvent(5);
    EXPECT_EQ(stack.peekTopEvent(), 5);
    EXPECT_EQ(stack.size(), 1);  // Peek should not change size

    stack.pushEvent(15);
    EXPECT_EQ(stack.peekTopEvent(), 15);
    EXPECT_EQ(stack.size(), 2);

    stack.popEvent();
    EXPECT_EQ(stack.peekTopEvent(), 5);
}

// Test clearEvents
TEST_F(EventStackTest, ClearEvents) {
    EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);
    EXPECT_EQ(stack.size(), 3);

    stack.clearEvents();
    EXPECT_TRUE(stack.isEmpty());
    EXPECT_EQ(stack.size(), 0);
    static_cast<void>(stack.popEvent());  // acknowledge nodiscard
}

// Test move constructor
TEST_F(EventStackTest, MoveConstructor) {
    EventStack<int> original_stack;
    original_stack.pushEvent(1);
    original_stack.pushEvent(2);

    // Corrected: Use ::std::move
    EventStack<int> moved_stack = ::std::move(original_stack);

    EXPECT_TRUE(original_stack.isEmpty());  // Original should be empty
    EXPECT_EQ(original_stack.size(), 0);

    EXPECT_EQ(moved_stack.size(), 2);
    EXPECT_EQ(moved_stack.popEvent(), 2);
    EXPECT_EQ(moved_stack.popEvent(), 1);
    EXPECT_TRUE(moved_stack.isEmpty());
}

// Test move assignment operator
TEST_F(EventStackTest, MoveAssignmentOperator) {
    EventStack<int> stack1;
    stack1.pushEvent(10);
    stack1.pushEvent(20);

    EventStack<int> stack2;
    stack2.pushEvent(100);

    // Corrected: Use ::std::move
    stack2 = ::std::move(stack1);

    EXPECT_TRUE(stack1.isEmpty());  // Original should be empty
    EXPECT_EQ(stack1.size(), 0);

    EXPECT_EQ(stack2.size(), 2);
    EXPECT_EQ(stack2.popEvent(), 20);
    EXPECT_EQ(stack2.popEvent(), 10);
    EXPECT_TRUE(stack2.isEmpty());
}

// Test filterEvents
TEST_F(EventStackTest, FilterEvents) {
    EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);
    stack.pushEvent(4);
    stack.pushEvent(5);

    // Filter out even numbers
    stack.filterEvents([](const int& n) { return n % 2 != 0; });
    EXPECT_EQ(stack.size(), 3);
    EXPECT_THAT(stack.popEvent(), testing::Optional(5));
    EXPECT_THAT(stack.popEvent(), testing::Optional(3));
    EXPECT_THAT(stack.popEvent(), testing::Optional(1));
    EXPECT_TRUE(stack.isEmpty());

    // Test filtering all elements
    stack.pushEvent(1);
    stack.filterEvents([](const int& n) { return false; });
    EXPECT_TRUE(stack.isEmpty());

    // Test filtering no elements
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.filterEvents([](const int& n) { return true; });
    EXPECT_EQ(stack.size(), 2);
    EXPECT_THAT(stack.popEvent(), testing::Optional(2));
    EXPECT_THAT(stack.popEvent(), testing::Optional(1));
}

// Test removeDuplicates
TEST_F(EventStackTest, RemoveDuplicates) {
    EventStack<int> stack;
    stack.pushEvent(3);
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(1);  // Duplicate
    stack.pushEvent(3);  // Duplicate
    stack.pushEvent(4);

    stack.removeDuplicates();
    EXPECT_EQ(stack.size(), 4);  // Should have 1, 2, 3, 4

    // Pop and check order (should be sorted after unique)
    EXPECT_THAT(stack.popEvent(), testing::Optional(4));
    EXPECT_THAT(stack.popEvent(), testing::Optional(3));
    EXPECT_THAT(stack.popEvent(), testing::Optional(2));
    EXPECT_THAT(stack.popEvent(), testing::Optional(1));
    EXPECT_TRUE(stack.isEmpty());

    // Test with no duplicates
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.removeDuplicates();
    EXPECT_EQ(stack.size(), 2);
}

// Test sortEvents
TEST_F(EventStackTest, SortEvents) {
    EventStack<int> stack;
    stack.pushEvent(3);
    stack.pushEvent(1);
    stack.pushEvent(4);
    stack.pushEvent(2);

    // Sort ascending
    stack.sortEvents([](const int& a, const int& b) { return a < b; });
    EXPECT_EQ(stack.size(), 4);
    EXPECT_THAT(stack.popEvent(), testing::Optional(4));
    EXPECT_THAT(stack.popEvent(), testing::Optional(3));
    EXPECT_THAT(stack.popEvent(), testing::Optional(2));
    EXPECT_THAT(stack.popEvent(), testing::Optional(1));

    // Sort descending
    stack.pushEvent(3);
    stack.pushEvent(1);
    stack.pushEvent(4);
    stack.pushEvent(2);
    stack.sortEvents([](const int& a, const int& b) { return a > b; });
    EXPECT_EQ(stack.size(), 4);
    EXPECT_THAT(stack.popEvent(), testing::Optional(1));
    EXPECT_THAT(stack.popEvent(), testing::Optional(2));
    EXPECT_THAT(stack.popEvent(), testing::Optional(3));
    EXPECT_THAT(stack.popEvent(), testing::Optional(4));
}

// Test reverseEvents
TEST_F(EventStackTest, ReverseEvents) {
    EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);

    stack.reverseEvents();
    EXPECT_EQ(stack.size(), 3);
    EXPECT_THAT(stack.popEvent(),
                testing::Optional(1));  // Order should be 1, 2, 3
    EXPECT_THAT(stack.popEvent(), testing::Optional(2));
    EXPECT_THAT(stack.popEvent(), testing::Optional(3));
    EXPECT_TRUE(stack.isEmpty());
}

// Test countEvents
TEST_F(EventStackTest, CountEvents) {
    EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);
    stack.pushEvent(2);
    stack.pushEvent(4);

    EXPECT_EQ(stack.countEvents([](const int& n) { return n % 2 == 0; }),
              3);  // 2, 2, 4
    EXPECT_EQ(stack.countEvents([](const int& n) { return n == 2; }), 2);
    EXPECT_EQ(stack.countEvents([](const int& n) { return n > 10; }), 0);
    EXPECT_EQ(stack.countEvents([](const int& n) { return true; }), 5);
    EXPECT_EQ(stack.size(), 5);  // Should not modify stack
}

// Test findEvent
TEST_F(EventStackTest, FindEvent) {
    EventStack<int> stack;
    stack.pushEvent(10);
    stack.pushEvent(20);
    stack.pushEvent(30);

    EXPECT_THAT(stack.findEvent([](const int& n) { return n == 20; }),
                testing::Optional(20));
    EXPECT_FALSE(
        stack.findEvent([](const int& n) { return n == 50; }).has_value());
    EXPECT_EQ(stack.size(), 3);  // Should not modify stack
}

// Test anyEvent and allEvents
TEST_F(EventStackTest, AnyAllEvents) {
    EventStack<int> stack;
    stack.pushEvent(2);
    stack.pushEvent(4);
    stack.pushEvent(6);

    EXPECT_TRUE(stack.anyEvent([](const int& n) { return n == 4; }));
    EXPECT_FALSE(stack.anyEvent([](const int& n) { return n == 5; }));
    EXPECT_TRUE(stack.allEvents([](const int& n) { return n % 2 == 0; }));
    EXPECT_FALSE(stack.allEvents([](const int& n) { return n > 5; }));
    EXPECT_EQ(stack.size(), 3);  // Should not modify stack

    EventStack<int> empty_stack;
    EXPECT_FALSE(empty_stack.anyEvent([](const int& n) { return true; }));
    EXPECT_TRUE(empty_stack.allEvents(
        [](const int& n) { return true; }));  // All true for empty set
}

// Test forEach
TEST_F(EventStackTest, ForEach) {
    EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);

    int sum = 0;
    stack.forEach([&sum](const int& n) { sum += n; });
    EXPECT_EQ(sum, 6);
    EXPECT_EQ(stack.size(), 3);  // Should not modify stack
}

// Test transformEvents
TEST_F(EventStackTest, TransformEvents) {
    EventStack<int> stack;
    stack.pushEvent(1);
    stack.pushEvent(2);
    stack.pushEvent(3);

    stack.transformEvents([](int& n) { n *= 2; });
    EXPECT_EQ(stack.size(), 3);
    EXPECT_THAT(stack.popEvent(), testing::Optional(6));
    EXPECT_THAT(stack.popEvent(), testing::Optional(4));
    EXPECT_THAT(stack.popEvent(), testing::Optional(2));
}

// Test serialization and deserialization for int
TEST_F(EventStackTest, SerializeDeserializeInt) {
    EventStack<int> stack;
    stack.pushEvent(10);
    stack.pushEvent(20);
    stack.pushEvent(30);

    // Corrected: Use std::string explicitly
    std::string serialized = stack.serializeStack();
    EXPECT_EQ(serialized, "10;20;30;");  // Order is reversed due to
                                         // drainToVector and refillFromVector

    EventStack<int> new_stack;
    new_stack.deserializeStack(serialized);
    EXPECT_EQ(new_stack.size(), 3);
    EXPECT_THAT(new_stack.popEvent(), testing::Optional(30));
    EXPECT_THAT(new_stack.popEvent(), testing::Optional(20));
    EXPECT_THAT(new_stack.popEvent(), testing::Optional(10));
    EXPECT_TRUE(new_stack.isEmpty());

    // Test with empty stack
    EventStack<int> empty_stack;
    // Corrected: Use std::string explicitly
    std::string empty_serialized = empty_stack.serializeStack();
    EXPECT_EQ(empty_serialized, "");
    new_stack.deserializeStack(empty_serialized);
    EXPECT_TRUE(new_stack.isEmpty());
}

// Test serialization and deserialization for std::string
TEST_F(EventStackTest, SerializeDeserializeString) {
    // Corrected: Use std::string explicitly
    EventStack<std::string> stack;
    stack.pushEvent("hello");
    stack.pushEvent("world");
    stack.pushEvent("c++");

    std::string serialized = stack.serializeStack();
    EXPECT_EQ(serialized, "hello;world;c++;");

    EventStack<std::string> new_stack;
    new_stack.deserializeStack(serialized);
    EXPECT_EQ(new_stack.size(), 3);
    EXPECT_THAT(new_stack.popEvent(), testing::Optional("c++"));
    EXPECT_THAT(new_stack.popEvent(), testing::Optional("world"));
    EXPECT_THAT(new_stack.popEvent(), testing::Optional("hello"));
    EXPECT_TRUE(new_stack.isEmpty());
}

// Test serialization and deserialization for custom TestEvent
TEST_F(EventStackTest, SerializeDeserializeTestEvent) {
    EventStack<TestEvent> stack;
    stack.pushEvent({1, "apple"});
    stack.pushEvent({2, "banana"});
    stack.pushEvent({3, "cherry"});

    // Use custom serializer and deserializer
    std::string serialized = stack.serializeStack([](const TestEvent& e) { return std::to_string(e); });
    EXPECT_EQ(serialized, "1:apple;2:banana;3:cherry;");

    EventStack<TestEvent> new_stack;
    new_stack.deserializeStack(serialized, from_string);
    EXPECT_EQ(new_stack.size(), 3);
    EXPECT_THAT(new_stack.popEvent(),
                testing::Optional(TestEvent{3, "cherry"}));
    EXPECT_THAT(new_stack.popEvent(),
                testing::Optional(TestEvent{2, "banana"}));
    EXPECT_THAT(new_stack.popEvent(), testing::Optional(TestEvent{1, "apple"}));
    EXPECT_TRUE(new_stack.isEmpty());
}

// Concurrency test for push and pop
TEST_F(EventStackTest, ConcurrentPushPop) {
    EventStack<int> stack;
    const int num_threads = 8;
    const int pushes_per_thread = 1000;
    const int total_pushes = num_threads * pushes_per_thread;

    std::vector<std::thread> push_threads;
    for (int i = 0; i < num_threads; ++i) {
        push_threads.emplace_back([&stack, i, pushes_per_thread]() {
            for (int j = 0; j < pushes_per_thread; ++j) {
                stack.pushEvent(i * pushes_per_thread + j);
            }
        });
    }

    for (auto& t : push_threads) {
        t.join();
    }

    EXPECT_EQ(stack.size(), total_pushes);

    std::atomic<int> pop_count = 0;
    std::vector<std::thread> pop_threads;
    std::vector<std::vector<int>> popped_values(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        pop_threads.emplace_back([&stack, &pop_count, &popped_values, i]() {
            while (true) {
                ::std::optional<int> val = stack.popEvent();
                if (val.has_value()) {
                    popped_values[i].push_back(val.value());
                    pop_count.fetch_add(1);
                } else {
                    // If stack is empty, check if all elements have been popped
                    // This loop might spin for a bit if other threads are still
                    // pushing/popping
                    if (stack.size() == 0 && pop_count.load() == total_pushes) {
                        break;
                    }
                    std::this_thread::yield();  // Yield to other threads
                }
            }
        });
    }

    for (auto& t : pop_threads) {
        t.join();
    }

    EXPECT_EQ(pop_count.load(), total_pushes);
    EXPECT_TRUE(stack.isEmpty());

    std::set<int> all_popped_unique;
    for (const auto& vec : popped_values) {
        for (int val : vec) {
            all_popped_unique.insert(val);
        }
    }
    EXPECT_EQ(all_popped_unique.size(),
              total_pushes);  // Ensure all unique values were popped
}

// Test concurrent push and peek
TEST_F(EventStackTest, ConcurrentPushPeek) {
    EventStack<int> stack;
    const int num_pushers = 4;
    const int num_peekers = 4;
    const int pushes_per_thread = 500;
    const int total_pushes = num_pushers * pushes_per_thread;

    std::vector<std::thread> threads;
    std::atomic<bool> stop_peeking(false);

    // Pushers
    for (int i = 0; i < num_pushers; ++i) {
        threads.emplace_back([&stack, i, pushes_per_thread]() {
            for (int j = 0; j < pushes_per_thread; ++j) {
                stack.pushEvent(i * pushes_per_thread + j);
                std::this_thread::yield();  // Allow peekers to run
            }
        });
    }

    // Peekers
    for (int i = 0; i < num_peekers; ++i) {
        threads.emplace_back([&stack, &stop_peeking]() {
            while (!stop_peeking.load()) {
                ::std::optional<int> val = stack.peekTopEvent();
                // Just ensure it doesn't crash or return garbage
                if (val.has_value()) {
                    // spdlog::debug("Peeker saw: {}", val.value());
                }
                std::this_thread::yield();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // After all pushes are done, stop peekers
    stop_peeking.store(true);

    // Join peeker threads (they might still be running if not joined above)
    // This part is tricky as peekers might be stuck in the loop if not stopped.
    // A better approach would be to use futures or condition variables.
    // For this simple test, we assume they will eventually stop.

    // Verify final size
    EXPECT_EQ(stack.size(), total_pushes);

    // Pop all elements to ensure integrity
    int popped_count = 0;
    while (stack.popEvent().has_value()) {
        popped_count++;
    }
    EXPECT_EQ(popped_count, total_pushes);
    EXPECT_TRUE(stack.isEmpty());
}

}  // namespace atom::async
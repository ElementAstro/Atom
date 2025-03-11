#include <gtest/gtest.h>
#include <vector>

#include "atom/memory/ring.hpp"

using namespace atom::memory;

class RingBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

TEST_F(RingBufferTest, Constructor) {
    EXPECT_THROW(RingBuffer<int> buffer(0), std::invalid_argument);
    RingBuffer<int> buffer(10);
    EXPECT_EQ(buffer.capacity(), 10);
    EXPECT_EQ(buffer.size(), 0);
}

TEST_F(RingBufferTest, PushAndPop) {
    RingBuffer<int> buffer(3);
    EXPECT_TRUE(buffer.push(1));
    EXPECT_TRUE(buffer.push(2));
    EXPECT_TRUE(buffer.push(3));
    EXPECT_FALSE(buffer.push(4));  // Buffer should be full

    EXPECT_EQ(buffer.size(), 3);
    EXPECT_EQ(buffer.pop(), 1);
    EXPECT_EQ(buffer.pop(), 2);
    EXPECT_EQ(buffer.pop(), 3);
    EXPECT_EQ(buffer.pop(), std::nullopt);  // Buffer should be empty
}

TEST_F(RingBufferTest, PushOverwrite) {
    RingBuffer<int> buffer(3);
    buffer.pushOverwrite(1);
    buffer.pushOverwrite(2);
    buffer.pushOverwrite(3);
    buffer.pushOverwrite(4);  // Should overwrite the oldest element

    EXPECT_EQ(buffer.size(), 3);
    EXPECT_EQ(buffer.pop(), 2);
    EXPECT_EQ(buffer.pop(), 3);
    EXPECT_EQ(buffer.pop(), 4);
}

TEST_F(RingBufferTest, FullAndEmpty) {
    RingBuffer<int> buffer(2);
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());

    buffer.push(1);
    buffer.push(2);
    EXPECT_FALSE(buffer.empty());
    EXPECT_TRUE(buffer.full());

    buffer.pop();
    EXPECT_FALSE(buffer.full());
    EXPECT_FALSE(buffer.empty());

    buffer.pop();
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
}

TEST_F(RingBufferTest, FrontAndBack) {
    RingBuffer<int> buffer(3);
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);

    EXPECT_EQ(buffer.front(), 1);
    EXPECT_EQ(buffer.back(), 3);

    buffer.pop();
    EXPECT_EQ(buffer.front(), 2);
    EXPECT_EQ(buffer.back(), 3);
}

TEST_F(RingBufferTest, Contains) {
    RingBuffer<int> buffer(3);
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);

    EXPECT_TRUE(buffer.contains(1));
    EXPECT_TRUE(buffer.contains(2));
    EXPECT_TRUE(buffer.contains(3));
    EXPECT_FALSE(buffer.contains(4));
}

TEST_F(RingBufferTest, View) {
    RingBuffer<int> buffer(3);
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);

    auto view = buffer.view();
    EXPECT_EQ(view.size(), 3);
    EXPECT_EQ(view[0], 1);
    EXPECT_EQ(view[1], 2);
    EXPECT_EQ(view[2], 3);
}

TEST_F(RingBufferTest, Iterator) {
    RingBuffer<int> buffer(3);
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);

    std::vector<int> elements;
    for (const auto& item : buffer) {
        elements.push_back(item);
    }

    EXPECT_EQ(elements.size(), 3);
    EXPECT_EQ(elements[0], 1);
    EXPECT_EQ(elements[1], 2);
    EXPECT_EQ(elements[2], 3);
}

TEST_F(RingBufferTest, Resize) {
    RingBuffer<int> buffer(3);
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);

    buffer.resize(5);
    EXPECT_EQ(buffer.capacity(), 5);
    EXPECT_EQ(buffer.size(), 3);

    buffer.push(4);
    buffer.push(5);
    EXPECT_EQ(buffer.size(), 5);

    EXPECT_THROW(
        buffer.resize(2),
        std::runtime_error);  // Cannot resize to smaller than current size
}

TEST_F(RingBufferTest, At) {
    RingBuffer<int> buffer(3);
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);

    EXPECT_EQ(buffer.at(0), 1);
    EXPECT_EQ(buffer.at(1), 2);
    EXPECT_EQ(buffer.at(2), 3);
    EXPECT_EQ(buffer.at(3), std::nullopt);  // Out of bounds
}

TEST_F(RingBufferTest, ForEach) {
    RingBuffer<int> buffer(3);
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);

    buffer.forEach([](int& item) { item *= 2; });

    EXPECT_EQ(buffer.pop(), 2);
    EXPECT_EQ(buffer.pop(), 4);
    EXPECT_EQ(buffer.pop(), 6);
}

TEST_F(RingBufferTest, RemoveIf) {
    RingBuffer<int> buffer(5);
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);
    buffer.push(4);
    buffer.push(5);

    buffer.removeIf([](int item) {
        return item % 2 == 0;  // Remove even numbers
    });

    EXPECT_EQ(buffer.size(), 3);
    EXPECT_EQ(buffer.pop(), 1);
    EXPECT_EQ(buffer.pop(), 3);
    EXPECT_EQ(buffer.pop(), 5);
}

TEST_F(RingBufferTest, Rotate) {
    RingBuffer<int> buffer(5);
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);
    buffer.push(4);
    buffer.push(5);

    buffer.rotate(2);  // Rotate left by 2
    EXPECT_EQ(buffer.pop(), 3);
    EXPECT_EQ(buffer.pop(), 4);
    EXPECT_EQ(buffer.pop(), 5);
    EXPECT_EQ(buffer.pop(), 1);
    EXPECT_EQ(buffer.pop(), 2);

    buffer.push(1);
    buffer.push(2);
    buffer.push(3);
    buffer.push(4);
    buffer.push(5);

    buffer.rotate(-2);  // Rotate right by 2
    EXPECT_EQ(buffer.pop(), 4);
    EXPECT_EQ(buffer.pop(), 5);
    EXPECT_EQ(buffer.pop(), 1);
    EXPECT_EQ(buffer.pop(), 2);
    EXPECT_EQ(buffer.pop(), 3);
}

// Test with a custom class
class TestObject {
public:
    TestObject(int v = 0) : value(v) {}

    int getValue() const { return value; }
    void setValue(int v) { value = v; }

    bool operator==(const TestObject& other) const {
        return value == other.value;
    }

private:
    int value;
};

TEST_F(RingBufferTest, CustomObjectStorage) {
    RingBuffer<TestObject> buffer(3);

    buffer.push(TestObject(1));
    buffer.push(TestObject(2));
    buffer.push(TestObject(3));

    auto front = buffer.front();
    ASSERT_TRUE(front.has_value());
    EXPECT_EQ(front->getValue(), 1);

    auto back = buffer.back();
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(back->getValue(), 3);
}

// Test edge case: pushing and popping at capacity boundary
TEST_F(RingBufferTest, CapacityBoundary) {
    RingBuffer<int> buffer(3);

    // Fill the buffer
    EXPECT_TRUE(buffer.push(1));
    EXPECT_TRUE(buffer.push(2));
    EXPECT_TRUE(buffer.push(3));

    // Buffer should be full
    EXPECT_TRUE(buffer.full());
    EXPECT_FALSE(buffer.push(4));

    // Pop one item
    auto popped = buffer.pop();
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(*popped, 1);

    // Should be able to push again
    EXPECT_TRUE(buffer.push(4));

    // Buffer should be full again
    EXPECT_TRUE(buffer.full());

    // Check contents
    EXPECT_EQ(buffer.pop(), 2);
    EXPECT_EQ(buffer.pop(), 3);
    EXPECT_EQ(buffer.pop(), 4);
    EXPECT_EQ(buffer.pop(), std::nullopt);
}

// Test edge case: wrap-around behavior
TEST_F(RingBufferTest, WrapAround) {
    RingBuffer<int> buffer(3);

    // Fill buffer
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);

    // Pop two items
    buffer.pop();
    buffer.pop();

    // Push two more items, should wrap around
    buffer.push(4);
    buffer.push(5);

    // Check the contents
    auto view = buffer.view();
    EXPECT_EQ(view.size(), 3);
    EXPECT_EQ(view[0], 3);
    EXPECT_EQ(view[1], 4);
    EXPECT_EQ(view[2], 5);
}

// Test clearing empty buffer
TEST_F(RingBufferTest, ClearEmpty) {
    RingBuffer<int> buffer(3);

    // Clear empty buffer
    buffer.clear();
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.size(), 0);

    // Push and pop to ensure buffer still works
    buffer.push(1);
    EXPECT_EQ(buffer.size(), 1);
    EXPECT_EQ(buffer.pop(), 1);
    EXPECT_TRUE(buffer.empty());
}

// Test multiple push/pop cycles
TEST_F(RingBufferTest, MultipleCycles) {
    RingBuffer<int> buffer(2);

    for (int cycle = 0; cycle < 5; ++cycle) {
        // Fill buffer
        EXPECT_TRUE(buffer.push(cycle * 2 + 1));
        EXPECT_TRUE(buffer.push(cycle * 2 + 2));

        // Buffer should be full now
        EXPECT_TRUE(buffer.full());
        EXPECT_FALSE(buffer.push(999));

        // Empty buffer
        EXPECT_EQ(buffer.pop(), cycle * 2 + 1);
        EXPECT_EQ(buffer.pop(), cycle * 2 + 2);

        // Buffer should be empty now
        EXPECT_TRUE(buffer.empty());
        EXPECT_EQ(buffer.pop(), std::nullopt);
    }
}

// Test pushOverwrite edge cases
TEST_F(RingBufferTest, PushOverwriteEdgeCases) {
    RingBuffer<int> buffer(3);

    // First fill buffer normally
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);

    // Now start overwriting
    buffer.pushOverwrite(4);
    buffer.pushOverwrite(5);
    buffer.pushOverwrite(6);

    // Check contents - should have overwritten everything
    EXPECT_EQ(buffer.size(), 3);
    EXPECT_EQ(buffer.pop(), 4);
    EXPECT_EQ(buffer.pop(), 5);
    EXPECT_EQ(buffer.pop(), 6);
    EXPECT_TRUE(buffer.empty());

    // Test with empty buffer
    buffer.pushOverwrite(7);
    EXPECT_EQ(buffer.size(), 1);
    EXPECT_EQ(buffer.pop(), 7);
}

// Test thread safety with concurrent operations
#include <atomic>
#include <thread>

TEST_F(RingBufferTest, ConcurrentAccess) {
    RingBuffer<int> buffer(1000);
    std::atomic<bool> stop{false};
    std::atomic<int> producer_count{0};
    std::atomic<int> consumer_count{0};

    // Producer thread
    auto producer = [&]() {
        for (int i = 1; i <= 10000 && !stop; ++i) {
            if (buffer.push(i)) {
                producer_count++;
            }
        }
    };

    // Consumer thread
    auto consumer = [&]() {
        while (!stop || !buffer.empty()) {
            auto value = buffer.pop();
            if (value) {
                consumer_count++;
            }
        }
    };

    // Start threads
    std::thread producer_thread(producer);
    std::thread consumer_thread(consumer);

    // Let them run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Signal stop and wait for threads
    stop = true;
    producer_thread.join();
    consumer_thread.join();

    // Verify all items were consumed
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(producer_count.load(), consumer_count.load());
}

// Test forEach with modification
TEST_F(RingBufferTest, ForEachModification) {
    RingBuffer<TestObject> buffer(3);

    buffer.push(TestObject(1));
    buffer.push(TestObject(2));
    buffer.push(TestObject(3));

    buffer.forEach([](TestObject& obj) { obj.setValue(obj.getValue() * 10); });

    // Verify values were modified
    auto view = buffer.view();
    EXPECT_EQ(view.size(), 3);
    EXPECT_EQ(view[0].getValue(), 10);
    EXPECT_EQ(view[1].getValue(), 20);
    EXPECT_EQ(view[2].getValue(), 30);
}

// Test removeIf with different predicates
TEST_F(RingBufferTest, RemoveIfVariations) {
    RingBuffer<int> buffer(10);
    for (int i = 1; i <= 10; ++i) {
        buffer.push(i);
    }

    // Remove odd numbers
    buffer.removeIf([](int x) { return x % 2 != 0; });

    // Should have only even numbers
    EXPECT_EQ(buffer.size(), 5);
    auto view = buffer.view();
    for (size_t i = 0; i < view.size(); ++i) {
        EXPECT_EQ(view[i] % 2, 0);
    }

    // Remove numbers greater than 6
    buffer.removeIf([](int x) { return x > 6; });

    // Should have 2, 4, 6 left
    EXPECT_EQ(buffer.size(), 3);
    EXPECT_EQ(buffer.at(0), 2);
    EXPECT_EQ(buffer.at(1), 4);
    EXPECT_EQ(buffer.at(2), 6);
}

// Test rotation with empty buffer and zero rotation
TEST_F(RingBufferTest, RotateEdgeCases) {
    RingBuffer<int> buffer(5);

    // Rotate empty buffer
    buffer.rotate(3);
    EXPECT_TRUE(buffer.empty());

    // Fill buffer
    for (int i = 1; i <= 5; ++i) {
        buffer.push(i);
    }

    // Rotate by 0
    buffer.rotate(0);
    auto view = buffer.view();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(view[i], i + 1);
    }

    // Rotate by full size
    buffer.rotate(5);
    view = buffer.view();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(view[i], i + 1);
    }

    // Rotate by size + 1 (should be same as rotating by 1)
    buffer.rotate(6);
    view = buffer.view();
    EXPECT_EQ(view[0], 2);
    EXPECT_EQ(view[1], 3);
    EXPECT_EQ(view[2], 4);
    EXPECT_EQ(view[3], 5);
    EXPECT_EQ(view[4], 1);
}

// Test buffer with move-only types
#include <memory>

TEST_F(RingBufferTest, MoveOnlyTypes) {
    RingBuffer<std::unique_ptr<int>> buffer(3);

    buffer.push(std::make_unique<int>(1));
    buffer.push(std::make_unique<int>(2));
    buffer.push(std::make_unique<int>(3));

    // Can't use view() with move-only types directly

    // Test pop
    auto ptr1 = buffer.pop();
    ASSERT_TRUE(ptr1.has_value());
    EXPECT_EQ(**ptr1, 1);

    auto ptr2 = buffer.pop();
    ASSERT_TRUE(ptr2.has_value());
    EXPECT_EQ(**ptr2, 2);

    auto ptr3 = buffer.pop();
    ASSERT_TRUE(ptr3.has_value());
    EXPECT_EQ(**ptr3, 3);

    EXPECT_TRUE(buffer.empty());
}

// Test ring buffer resize edge cases
TEST_F(RingBufferTest, ResizeEdgeCases) {
    RingBuffer<int> buffer(3);
    buffer.push(1);
    buffer.push(2);

    // Resize to same size
    buffer.resize(3);
    EXPECT_EQ(buffer.capacity(), 3);
    EXPECT_EQ(buffer.size(), 2);
    EXPECT_EQ(*buffer.at(0), 1);
    EXPECT_EQ(*buffer.at(1), 2);

    // Resize to equal current size
    buffer.resize(2);
    EXPECT_EQ(buffer.capacity(), 2);
    EXPECT_EQ(buffer.size(), 2);
    EXPECT_EQ(*buffer.at(0), 1);
    EXPECT_EQ(*buffer.at(1), 2);

    // Try to resize to less than current size
    EXPECT_THROW({ buffer.resize(1); }, std::runtime_error);

    // Resize to much larger
    buffer.resize(100);
    EXPECT_EQ(buffer.capacity(), 100);
    EXPECT_EQ(buffer.size(), 2);
    EXPECT_EQ(*buffer.at(0), 1);
    EXPECT_EQ(*buffer.at(1), 2);
}

// Test with large amount of data
TEST_F(RingBufferTest, LargeDataSet) {
    const size_t bufferSize = 10000;
    RingBuffer<int> buffer(bufferSize);

    // Fill the buffer
    for (size_t i = 0; i < bufferSize; ++i) {
        EXPECT_TRUE(buffer.push(static_cast<int>(i)));
    }

    EXPECT_TRUE(buffer.full());
    EXPECT_EQ(buffer.size(), bufferSize);

    // Check first and last elements
    EXPECT_EQ(*buffer.front(), 0);
    EXPECT_EQ(*buffer.back(), static_cast<int>(bufferSize - 1));

    // Check random access
    for (size_t i = 0; i < 100; ++i) {
        size_t idx = rand() % bufferSize;
        EXPECT_EQ(*buffer.at(idx), static_cast<int>(idx));
    }

    // Pop half the elements
    for (size_t i = 0; i < bufferSize / 2; ++i) {
        auto val = buffer.pop();
        ASSERT_TRUE(val.has_value());
        EXPECT_EQ(*val, static_cast<int>(i));
    }

    // Push more elements
    for (size_t i = 0; i < bufferSize / 2; ++i) {
        EXPECT_TRUE(buffer.push(static_cast<int>(bufferSize + i)));
    }

    // Check that the buffer now contains second half of original data
    // followed by the newly added data
    auto view = buffer.view();
    EXPECT_EQ(view.size(), bufferSize);

    for (size_t i = 0; i < bufferSize / 2; ++i) {
        EXPECT_EQ(view[i], static_cast<int>(bufferSize / 2 + i));
    }

    for (size_t i = 0; i < bufferSize / 2; ++i) {
        EXPECT_EQ(view[bufferSize / 2 + i], static_cast<int>(bufferSize + i));
    }
}

// Test copy constructor and assignment
TEST_F(RingBufferTest, CopyAndMove) {
    // RingBuffer is not copyable or movable by default since std::mutex is not
    // This test would fail to compile if uncommented

    // RingBuffer<int> buffer1(5);
    // buffer1.push(1);
    // buffer1.push(2);

    // Copy construction
    // RingBuffer<int> buffer2 = buffer1;  // Should not compile

    // Copy assignment
    // RingBuffer<int> buffer3(5);
    // buffer3 = buffer1;  // Should not compile

    // Move construction
    // RingBuffer<int> buffer4 = std::move(buffer1);  // Should not compile

    // Move assignment
    // RingBuffer<int> buffer5(5);
    // buffer5 = std::move(buffer1);  // Should not compile

    // Just to have an assertion
    // SUCCEED("RingBuffer correctly prevents copying and moving");
}

// Test extreme cases
TEST_F(RingBufferTest, ExtremeCases) {
    // Test with size 1
    RingBuffer<int> singleBuffer(1);

    EXPECT_TRUE(singleBuffer.push(42));
    EXPECT_FALSE(singleBuffer.push(43));

    EXPECT_EQ(*singleBuffer.front(), 42);
    EXPECT_EQ(*singleBuffer.back(), 42);

    singleBuffer.pushOverwrite(43);
    EXPECT_EQ(*singleBuffer.front(), 43);

    EXPECT_EQ(*singleBuffer.pop(), 43);
    EXPECT_TRUE(singleBuffer.empty());

    // Test with very large buffer
    const size_t largeSize = 1 << 20;  // 1 million entries
    RingBuffer<char> largeBuffer(largeSize);

    // We don't actually fill it, just make sure it can be created and used
    for (size_t i = 0; i < 100; ++i) {
        EXPECT_TRUE(largeBuffer.push(static_cast<char>('A' + (i % 26))));
    }

    EXPECT_EQ(largeBuffer.size(), 100);
    EXPECT_EQ(*largeBuffer.front(), 'A');
}

// Test the behavior of the iterator with an empty buffer
TEST_F(RingBufferTest, EmptyIterator) {
    RingBuffer<int> buffer(5);

    // Empty buffer should have begin == end
    auto begin = buffer.begin();
    auto end = buffer.end();
    EXPECT_EQ(begin, end);

    // Verify no iterations happen
    int count = 0;
    for (auto it = buffer.begin(); it != buffer.end(); ++it) {
        count++;
    }
    EXPECT_EQ(count, 0);

    // Add one element and test iteration
    buffer.push(42);
    count = 0;
    for (auto it = buffer.begin(); it != buffer.end(); ++it) {
        count++;
        EXPECT_EQ(*it, 42);
    }
    EXPECT_EQ(count, 1);
}
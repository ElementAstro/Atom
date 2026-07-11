#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "atom/type/deque.hpp"

#include <stdexcept>
#include <string>
#include <type_traits>

using namespace atom::type;

// Builds a test value of the parameter type from an int. Using
// makeVal<TypeParam>(int) directly narrows int->char for std::string
// (ill-formed); this helper produces a valid value for both int and
// std::string.
template <typename T>
static T makeVal(int i) {
    if constexpr (std::is_same_v<T, std::string>) {
        return std::to_string(i);
    } else {
        return static_cast<T>(i);
    }
}

// Test fixture for CircularBuffer
template <typename T>
class CircularBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup if needed
    }

    void TearDown() override {
        // Common teardown if needed
    }
};

using MyTypes = ::testing::Types<int, std::string>;
TYPED_TEST_SUITE(CircularBufferTest, MyTypes);

TYPED_TEST(CircularBufferTest, DefaultConstructor) {
    CircularBuffer<TypeParam> cb;
    ASSERT_EQ(cb.size(), 0);
    ASSERT_EQ(cb.capacity(), 16);  // Default capacity
    ASSERT_TRUE(cb.empty());
    ASSERT_FALSE(cb.full());
}

TYPED_TEST(CircularBufferTest, CustomCapacityConstructor) {
    CircularBuffer<TypeParam> cb(5);
    ASSERT_EQ(cb.size(), 0);
    ASSERT_EQ(cb.capacity(), 5);
    ASSERT_TRUE(cb.empty());
    ASSERT_FALSE(cb.full());
}

TYPED_TEST(CircularBufferTest, PushBack) {
    CircularBuffer<TypeParam> cb(3);
    cb.push_back(makeVal<TypeParam>(1));
    ASSERT_EQ(cb.size(), 1);
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(1));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(1));

    cb.push_back(makeVal<TypeParam>(2));
    ASSERT_EQ(cb.size(), 2);
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(1));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(2));

    cb.push_back(makeVal<TypeParam>(3));
    ASSERT_EQ(cb.size(), 3);
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(1));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(3));
    ASSERT_TRUE(cb.full());
}

TYPED_TEST(CircularBufferTest, PushFront) {
    CircularBuffer<TypeParam> cb(3);
    cb.push_front(makeVal<TypeParam>(1));
    ASSERT_EQ(cb.size(), 1);
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(1));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(1));

    cb.push_front(makeVal<TypeParam>(2));
    ASSERT_EQ(cb.size(), 2);
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(2));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(1));

    cb.push_front(makeVal<TypeParam>(3));
    ASSERT_EQ(cb.size(), 3);
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(3));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(1));
    ASSERT_TRUE(cb.full());
}

TYPED_TEST(CircularBufferTest, PopFront) {
    CircularBuffer<TypeParam> cb(3);
    cb.push_back(makeVal<TypeParam>(1));
    cb.push_back(makeVal<TypeParam>(2));
    cb.push_back(makeVal<TypeParam>(3));

    cb.pop_front();
    ASSERT_EQ(cb.size(), 2);
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(2));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(3));

    cb.pop_front();
    ASSERT_EQ(cb.size(), 1);
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(3));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(3));

    cb.pop_front();
    ASSERT_EQ(cb.size(), 0);
    ASSERT_TRUE(cb.empty());

    ASSERT_THROW(cb.pop_front(), std::runtime_error);
}

TYPED_TEST(CircularBufferTest, PopBack) {
    CircularBuffer<TypeParam> cb(3);
    cb.push_back(makeVal<TypeParam>(1));
    cb.push_back(makeVal<TypeParam>(2));
    cb.push_back(makeVal<TypeParam>(3));

    cb.pop_back();
    ASSERT_EQ(cb.size(), 2);
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(1));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(2));

    cb.pop_back();
    ASSERT_EQ(cb.size(), 1);
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(1));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(1));

    cb.pop_back();
    ASSERT_EQ(cb.size(), 0);
    ASSERT_TRUE(cb.empty());

    ASSERT_THROW(cb.pop_back(), std::runtime_error);
}

TYPED_TEST(CircularBufferTest, FrontAndBackAccess) {
    CircularBuffer<TypeParam> cb(5);
    ASSERT_THROW(cb.front(), std::runtime_error);
    ASSERT_THROW(cb.back(), std::runtime_error);

    cb.push_back(makeVal<TypeParam>(10));
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(10));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(10));

    cb.push_back(makeVal<TypeParam>(20));
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(10));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(20));

    cb.push_front(makeVal<TypeParam>(5));
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(5));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(20));
}

TYPED_TEST(CircularBufferTest, IndexedAccessOperator) {
    CircularBuffer<TypeParam> cb(5);
    cb.push_back(makeVal<TypeParam>(10));
    cb.push_back(makeVal<TypeParam>(20));
    cb.push_back(makeVal<TypeParam>(30));

    ASSERT_EQ(cb[0], makeVal<TypeParam>(10));
    ASSERT_EQ(cb[1], makeVal<TypeParam>(20));
    ASSERT_EQ(cb[2], makeVal<TypeParam>(30));

    cb.pop_front();  // 20, 30
    ASSERT_EQ(cb[0], makeVal<TypeParam>(20));
    ASSERT_EQ(cb[1], makeVal<TypeParam>(30));

    cb.push_back(makeVal<TypeParam>(40));  // 20, 30, 40
    ASSERT_EQ(cb[2], makeVal<TypeParam>(40));

    // Test const version
    const CircularBuffer<TypeParam>& const_cb = cb;
    ASSERT_EQ(const_cb[0], makeVal<TypeParam>(20));
}

TYPED_TEST(CircularBufferTest, IndexedAccessAt) {
    CircularBuffer<TypeParam> cb(5);
    cb.push_back(makeVal<TypeParam>(10));
    cb.push_back(makeVal<TypeParam>(20));

    ASSERT_EQ(cb.at(0), makeVal<TypeParam>(10));
    ASSERT_EQ(cb.at(1), makeVal<TypeParam>(20));
    ASSERT_THROW(cb.at(2), std::out_of_range);
    ASSERT_THROW(cb.at(100), std::out_of_range);

    // Test const version
    const CircularBuffer<TypeParam>& const_cb = cb;
    ASSERT_EQ(const_cb.at(0), makeVal<TypeParam>(10));
    ASSERT_THROW(const_cb.at(2), std::out_of_range);
}

TYPED_TEST(CircularBufferTest, Clear) {
    CircularBuffer<TypeParam> cb(5);
    cb.push_back(makeVal<TypeParam>(1));
    cb.push_back(makeVal<TypeParam>(2));
    cb.push_back(makeVal<TypeParam>(3));

    ASSERT_EQ(cb.size(), 3);
    ASSERT_FALSE(cb.empty());

    cb.clear();
    ASSERT_EQ(cb.size(), 0);
    ASSERT_TRUE(cb.empty());
    ASSERT_THROW(cb.front(), std::runtime_error);
}

TYPED_TEST(CircularBufferTest, AutoResizePushBack) {
    CircularBuffer<TypeParam> cb(2, true);  // Capacity 2, auto_resize true
    cb.push_back(makeVal<TypeParam>(1));
    cb.push_back(makeVal<TypeParam>(2));
    ASSERT_EQ(cb.size(), 2);
    ASSERT_EQ(cb.capacity(), 2);

    cb.push_back(makeVal<TypeParam>(3));  // Should trigger resize
    ASSERT_EQ(cb.size(), 3);
    ASSERT_EQ(cb.capacity(), 4);  // Capacity should double
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(1));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(3));

    cb.push_back(makeVal<TypeParam>(4));
    ASSERT_EQ(cb.size(), 4);
    ASSERT_EQ(cb.capacity(), 4);

    cb.push_back(makeVal<TypeParam>(5));  // Should trigger resize again
    ASSERT_EQ(cb.size(), 5);
    ASSERT_EQ(cb.capacity(), 8);
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(1));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(5));
}

TYPED_TEST(CircularBufferTest, AutoResizePushFront) {
    CircularBuffer<TypeParam> cb(2, true);  // Capacity 2, auto_resize true
    cb.push_front(makeVal<TypeParam>(1));
    cb.push_front(makeVal<TypeParam>(2));
    ASSERT_EQ(cb.size(), 2);
    ASSERT_EQ(cb.capacity(), 2);

    cb.push_front(makeVal<TypeParam>(3));  // Should trigger resize
    ASSERT_EQ(cb.size(), 3);
    ASSERT_EQ(cb.capacity(), 4);  // Capacity should double
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(3));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(1));

    cb.push_front(makeVal<TypeParam>(4));
    ASSERT_EQ(cb.size(), 4);
    ASSERT_EQ(cb.capacity(), 4);

    cb.push_front(makeVal<TypeParam>(5));  // Should trigger resize again
    ASSERT_EQ(cb.size(), 5);
    ASSERT_EQ(cb.capacity(), 8);
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(5));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(1));
}

TYPED_TEST(CircularBufferTest, NoAutoResizeOverwritePushBack) {
    CircularBuffer<TypeParam> cb(3, false);  // Capacity 3, auto_resize false
    cb.push_back(makeVal<TypeParam>(1));
    cb.push_back(makeVal<TypeParam>(2));
    cb.push_back(makeVal<TypeParam>(3));
    ASSERT_EQ(cb.size(), 3);
    ASSERT_TRUE(cb.full());
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(1));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(3));

    cb.push_back(makeVal<TypeParam>(4));  // Should overwrite 1
    ASSERT_EQ(cb.size(), 3);     // Size remains 3
    ASSERT_TRUE(cb.full());
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(2));  // Oldest element (1) is gone
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(4));   // Newest element is 4

    cb.push_back(makeVal<TypeParam>(5));  // Should overwrite 2
    ASSERT_EQ(cb.size(), 3);
    ASSERT_TRUE(cb.full());
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(3));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(5));
}

TYPED_TEST(CircularBufferTest, NoAutoResizeOverwritePushFront) {
    CircularBuffer<TypeParam> cb(3, false);  // Capacity 3, auto_resize false
    cb.push_front(makeVal<TypeParam>(1));
    cb.push_front(makeVal<TypeParam>(2));
    cb.push_front(makeVal<TypeParam>(3));
    ASSERT_EQ(cb.size(), 3);
    ASSERT_TRUE(cb.full());
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(3));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(1));

    cb.push_front(makeVal<TypeParam>(4));  // Should overwrite 1 (back element)
    ASSERT_EQ(cb.size(), 3);      // Size remains 3
    ASSERT_TRUE(cb.full());
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(4));  // Newest element is 4
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(2));   // Oldest element (1) is gone

    cb.push_front(makeVal<TypeParam>(5));  // Should overwrite 2
    ASSERT_EQ(cb.size(), 3);
    ASSERT_TRUE(cb.full());
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(5));
    ASSERT_EQ(cb.back(), makeVal<TypeParam>(3));
}

TYPED_TEST(CircularBufferTest, Reserve) {
    CircularBuffer<TypeParam> cb(5);
    ASSERT_EQ(cb.capacity(), 5);

    cb.reserve(10);
    ASSERT_EQ(cb.capacity(), 10);
    ASSERT_EQ(cb.size(), 0);  // Size should remain 0

    cb.push_back(makeVal<TypeParam>(1));
    cb.reserve(20);
    ASSERT_EQ(cb.capacity(), 20);
    ASSERT_EQ(cb.size(), 1);
    ASSERT_EQ(cb.front(), makeVal<TypeParam>(1));

    cb.reserve(5);  // Should not shrink
    ASSERT_EQ(cb.capacity(), 20);
}

TYPED_TEST(CircularBufferTest, CopyConstructor) {
    CircularBuffer<TypeParam> cb1(5);
    cb1.push_back(makeVal<TypeParam>(1));
    cb1.push_back(makeVal<TypeParam>(2));
    cb1.push_back(makeVal<TypeParam>(3));

    CircularBuffer<TypeParam> cb2 = cb1;  // Copy constructor
    ASSERT_EQ(cb2.size(), cb1.size());
    ASSERT_EQ(cb2.capacity(), cb1.capacity());
    ASSERT_EQ(cb2.front(), cb1.front());
    ASSERT_EQ(cb2.back(), cb1.back());
    ASSERT_EQ(cb2[0], cb1[0]);
    ASSERT_EQ(cb2[1], cb1[1]);
    ASSERT_EQ(cb2[2], cb1[2]);

    // Ensure deep copy
    cb1.push_back(makeVal<TypeParam>(4));
    ASSERT_NE(cb1.size(), cb2.size());
    ASSERT_EQ(cb2.size(), 3);
}

TYPED_TEST(CircularBufferTest, CopyAssignment) {
    CircularBuffer<TypeParam> cb1(5);
    cb1.push_back(makeVal<TypeParam>(1));
    cb1.push_back(makeVal<TypeParam>(2));

    CircularBuffer<TypeParam> cb2(10);
    cb2.push_back(makeVal<TypeParam>(100));
    cb2.push_back(makeVal<TypeParam>(120));
    cb2.push_back(makeVal<TypeParam>(125));

    cb2 = cb1;  // Copy assignment
    ASSERT_EQ(cb2.size(), cb1.size());
    ASSERT_EQ(cb2.capacity(), cb1.capacity());
    ASSERT_EQ(cb2.front(), cb1.front());
    ASSERT_EQ(cb2.back(), cb1.back());
    ASSERT_EQ(cb2[0], cb1[0]);
    ASSERT_EQ(cb2[1], cb1[1]);

    // Ensure deep copy
    cb1.push_back(makeVal<TypeParam>(3));
    ASSERT_NE(cb1.size(), cb2.size());
    ASSERT_EQ(cb2.size(), 2);
}

TYPED_TEST(CircularBufferTest, MoveConstructor) {
    CircularBuffer<TypeParam> cb1(5);
    cb1.push_back(makeVal<TypeParam>(1));
    cb1.push_back(makeVal<TypeParam>(2));
    cb1.push_back(makeVal<TypeParam>(3));

    CircularBuffer<TypeParam> cb2 = std::move(cb1);  // Move constructor
    ASSERT_EQ(cb2.size(), 3);
    ASSERT_EQ(cb2.capacity(), 5);
    ASSERT_EQ(cb2.front(), makeVal<TypeParam>(1));
    ASSERT_EQ(cb2.back(), makeVal<TypeParam>(3));

    // Original object should be in a valid but unspecified state
    ASSERT_EQ(cb1.size(), 0);
    ASSERT_EQ(cb1.capacity(), 0);  // Or some other default/empty state
    ASSERT_TRUE(cb1.empty());
}

TYPED_TEST(CircularBufferTest, MoveAssignment) {
    CircularBuffer<TypeParam> cb1(5);
    cb1.push_back(makeVal<TypeParam>(1));
    cb1.push_back(makeVal<TypeParam>(2));

    CircularBuffer<TypeParam> cb2(10);
    cb2.push_back(makeVal<TypeParam>(100));
    cb2.push_back(makeVal<TypeParam>(120));
    cb2.push_back(makeVal<TypeParam>(125));

    cb2 = std::move(cb1);  // Move assignment
    ASSERT_EQ(cb2.size(), 2);
    ASSERT_EQ(cb2.capacity(), 5);
    ASSERT_EQ(cb2.front(), makeVal<TypeParam>(1));
    ASSERT_EQ(cb2.back(), makeVal<TypeParam>(2));

    // Original object should be in a valid but unspecified state
    ASSERT_EQ(cb1.size(), 0);
    ASSERT_EQ(cb1.capacity(), 0);
    ASSERT_TRUE(cb1.empty());
}

// Test fixture for ChunkedDeque
template <typename T>
class ChunkedDequeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup if needed
    }

    void TearDown() override {
        // Common teardown if needed
    }
};

TYPED_TEST_SUITE(ChunkedDequeTest, MyTypes);

TYPED_TEST(ChunkedDequeTest, DefaultConstructor) {
    ChunkedDeque<TypeParam> dq;
    ASSERT_EQ(dq.size(), 0);
    ASSERT_TRUE(dq.empty());
}

TYPED_TEST(ChunkedDequeTest, PushBack) {
    ChunkedDeque<TypeParam, 4> dq;  // Small chunk size for easier testing
    dq.push_back(makeVal<TypeParam>(1));
    ASSERT_EQ(dq.size(), 1);
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(1));
    ASSERT_EQ(dq.back(), makeVal<TypeParam>(1));

    dq.push_back(makeVal<TypeParam>(2));
    dq.push_back(makeVal<TypeParam>(3));
    dq.push_back(makeVal<TypeParam>(4));
    ASSERT_EQ(dq.size(), 4);
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(1));
    ASSERT_EQ(dq.back(), makeVal<TypeParam>(4));

    dq.push_back(makeVal<TypeParam>(5));  // Should trigger new chunk
    ASSERT_EQ(dq.size(), 5);
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(1));
    ASSERT_EQ(dq.back(), makeVal<TypeParam>(5));
    ASSERT_EQ(dq[0], makeVal<TypeParam>(1));
    ASSERT_EQ(dq[4], makeVal<TypeParam>(5));
}

TYPED_TEST(ChunkedDequeTest, PushFront) {
    ChunkedDeque<TypeParam, 4> dq;  // Small chunk size for easier testing
    dq.push_front(makeVal<TypeParam>(1));
    ASSERT_EQ(dq.size(), 1);
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(1));
    ASSERT_EQ(dq.back(), makeVal<TypeParam>(1));

    dq.push_front(makeVal<TypeParam>(2));
    dq.push_front(makeVal<TypeParam>(3));
    dq.push_front(makeVal<TypeParam>(4));
    ASSERT_EQ(dq.size(), 4);
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(4));
    ASSERT_EQ(dq.back(), makeVal<TypeParam>(1));

    dq.push_front(makeVal<TypeParam>(5));  // Should trigger new chunk
    ASSERT_EQ(dq.size(), 5);
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(5));
    ASSERT_EQ(dq.back(), makeVal<TypeParam>(1));
    ASSERT_EQ(dq[0], makeVal<TypeParam>(5));
    ASSERT_EQ(dq[4], makeVal<TypeParam>(1));
}

TYPED_TEST(ChunkedDequeTest, PopBack) {
    ChunkedDeque<TypeParam, 4> dq;
    for (int i = 0; i < 10; ++i) {
        dq.push_back(makeVal<TypeParam>(i));
    }
    ASSERT_EQ(dq.size(), 10);
    ASSERT_EQ(dq.back(), makeVal<TypeParam>(9));

    dq.pop_back();
    ASSERT_EQ(dq.size(), 9);
    ASSERT_EQ(dq.back(), makeVal<TypeParam>(8));

    for (int i = 0; i < 8; ++i) {
        dq.pop_back();
    }
    ASSERT_EQ(dq.size(), 1);
    ASSERT_EQ(dq.back(), makeVal<TypeParam>(0));

    dq.pop_back();
    ASSERT_EQ(dq.size(), 0);
    ASSERT_TRUE(dq.empty());
    ASSERT_THROW(dq.pop_back(), std::runtime_error);
}

TYPED_TEST(ChunkedDequeTest, PopFront) {
    ChunkedDeque<TypeParam, 4> dq;
    for (int i = 0; i < 10; ++i) {
        dq.push_back(makeVal<TypeParam>(i));
    }
    ASSERT_EQ(dq.size(), 10);
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(0));

    dq.pop_front();
    ASSERT_EQ(dq.size(), 9);
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(1));

    for (int i = 0; i < 8; ++i) {
        dq.pop_front();
    }
    ASSERT_EQ(dq.size(), 1);
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(9));

    dq.pop_front();
    ASSERT_EQ(dq.size(), 0);
    ASSERT_TRUE(dq.empty());
    ASSERT_THROW(dq.pop_front(), std::runtime_error);
}

TYPED_TEST(ChunkedDequeTest, FrontAndBackAccess) {
    ChunkedDeque<TypeParam> dq;
    ASSERT_THROW(dq.front(), std::runtime_error);
    ASSERT_THROW(dq.back(), std::runtime_error);

    dq.push_back(makeVal<TypeParam>(10));
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(10));
    ASSERT_EQ(dq.back(), makeVal<TypeParam>(10));

    dq.push_back(makeVal<TypeParam>(20));
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(10));
    ASSERT_EQ(dq.back(), makeVal<TypeParam>(20));

    dq.push_front(makeVal<TypeParam>(5));
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(5));
    ASSERT_EQ(dq.back(), makeVal<TypeParam>(20));
}

TYPED_TEST(ChunkedDequeTest, IndexedAccessOperator) {
    ChunkedDeque<TypeParam, 4> dq;
    for (int i = 0; i < 10; ++i) {
        dq.push_back(makeVal<TypeParam>(i * 10));
    }  // 0, 10, 20, 30, 40, 50, 60, 70, 80, 90

    ASSERT_EQ(dq[0], makeVal<TypeParam>(0));
    ASSERT_EQ(dq[3], makeVal<TypeParam>(30));
    ASSERT_EQ(dq[4], makeVal<TypeParam>(40));  // Across chunk boundary
    ASSERT_EQ(dq[9], makeVal<TypeParam>(90));

    dq.pop_front();  // 10, 20, ..., 90
    ASSERT_EQ(dq[0], makeVal<TypeParam>(10));
    ASSERT_EQ(dq[8], makeVal<TypeParam>(90));

    dq.push_front(makeVal<TypeParam>(-10));  // -10, 10, 20, ..., 90
    ASSERT_EQ(dq[0], makeVal<TypeParam>(-10));
    ASSERT_EQ(dq[1], makeVal<TypeParam>(10));

    // Test const version
    const ChunkedDeque<TypeParam, 4>& const_dq = dq;
    ASSERT_EQ(const_dq[0], makeVal<TypeParam>(-10));
    ASSERT_EQ(const_dq[9], makeVal<TypeParam>(90));
}

TYPED_TEST(ChunkedDequeTest, Clear) {
    ChunkedDeque<TypeParam> dq;
    for (int i = 0; i < 100; ++i) {
        dq.push_back(makeVal<TypeParam>(i));
    }
    ASSERT_EQ(dq.size(), 100);
    ASSERT_FALSE(dq.empty());

    dq.clear();
    ASSERT_EQ(dq.size(), 0);
    ASSERT_TRUE(dq.empty());
    ASSERT_THROW(dq.front(), std::runtime_error);
}

TYPED_TEST(ChunkedDequeTest, LargeNumberOfElements) {
    ChunkedDeque<TypeParam, 64> dq;  // Larger chunk size
    const int num_elements = 10000;

    for (int i = 0; i < num_elements; ++i) {
        dq.push_back(makeVal<TypeParam>(i));
    }
    ASSERT_EQ(dq.size(), num_elements);
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(0));
    ASSERT_EQ(dq.back(), makeVal<TypeParam>(num_elements - 1));
    ASSERT_EQ(dq[num_elements / 2], makeVal<TypeParam>(num_elements / 2));

    for (int i = 0; i < num_elements / 2; ++i) {
        dq.pop_front();
    }
    ASSERT_EQ(dq.size(), num_elements / 2);
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(num_elements / 2));
    ASSERT_EQ(dq.back(), makeVal<TypeParam>(num_elements - 1));

    for (int i = 0; i < num_elements / 2; ++i) {
        dq.pop_back();
    }
    ASSERT_EQ(dq.size(), 0);
    ASSERT_TRUE(dq.empty());
}

TYPED_TEST(ChunkedDequeTest, MixedPushPop) {
    ChunkedDeque<TypeParam, 8> dq;
    for (int i = 0; i < 20; ++i) {
        dq.push_back(makeVal<TypeParam>(i));
    }  // 0..19

    for (int i = 0; i < 5; ++i) {
        dq.pop_front();  // 5..19
    }
    ASSERT_EQ(dq.size(), 15);
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(5));

    for (int i = 0; i < 5; ++i) {
        dq.pop_back();  // 5..14
    }
    ASSERT_EQ(dq.size(), 10);
    ASSERT_EQ(dq.back(), makeVal<TypeParam>(14));

    for (int i = 0; i < 10; ++i) {
        dq.push_front(makeVal<TypeParam>(-i - 1));  // -10..-1, 5..14
    }
    ASSERT_EQ(dq.size(), 20);
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(-10));
    ASSERT_EQ(dq.back(), makeVal<TypeParam>(14));
    ASSERT_EQ(dq[9], makeVal<TypeParam>(-1));
    ASSERT_EQ(dq[10], makeVal<TypeParam>(5));
}

// Test with move semantics
TYPED_TEST(ChunkedDequeTest, PushBackMove) {
    ChunkedDeque<TypeParam, 4> dq;
    TypeParam val1 = makeVal<TypeParam>(1);
    dq.push_back(std::move(val1));
    ASSERT_EQ(dq.size(), 1);
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(1));
}

TYPED_TEST(ChunkedDequeTest, PushFrontMove) {
    ChunkedDeque<TypeParam, 4> dq;
    TypeParam val1 = makeVal<TypeParam>(1);
    dq.push_front(std::move(val1));
    ASSERT_EQ(dq.size(), 1);
    ASSERT_EQ(dq.front(), makeVal<TypeParam>(1));
}

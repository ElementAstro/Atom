#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <thread>
#include <vector>


#include "atom/type/static_vector.hpp"

using namespace atom::type;

// Test fixture for StaticVector tests
class StaticVectorTest : public ::testing::Test {
protected:
    // Common capacities for testing
    static constexpr std::size_t SmallCapacity = 5;
    static constexpr std::size_t MediumCapacity = 20;
    static constexpr std::size_t LargeCapacity = 1000;

    // Common test vectors
    StaticVector<int, SmallCapacity> emptyIntVector;
    StaticVector<int, SmallCapacity> smallIntVector{1, 2, 3};
    StaticVector<std::string, MediumCapacity> stringVector{"one", "two",
                                                           "three"};

    // Set up vectors with specific values for specific tests
    void SetUp() override {
        // Fill int test vector with sequential values for certain tests
        sequentialIntVector.clear();
        for (int i = 0; i < static_cast<int>(MediumCapacity); ++i) {
            sequentialIntVector.pushBack(i);
        }
    }

    StaticVector<int, MediumCapacity> sequentialIntVector;
};

// Construction tests
TEST_F(StaticVectorTest, DefaultConstruction) {
    StaticVector<int, SmallCapacity> vec;
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.size(), 0);
    EXPECT_EQ(vec.capacity(), SmallCapacity);
}

TEST_F(StaticVectorTest, ValueConstruction) {
    StaticVector<int, SmallCapacity> vec(3, 42);
    EXPECT_EQ(vec.size(), 3);
    for (std::size_t i = 0; i < vec.size(); ++i) {
        EXPECT_EQ(vec[i], 42);
    }
}

TEST_F(StaticVectorTest, SizeConstruction) {
    StaticVector<int, SmallCapacity> vec(3);
    EXPECT_EQ(vec.size(), 3);
    // Elements should be default initialized
    for (std::size_t i = 0; i < vec.size(); ++i) {
        EXPECT_EQ(vec[i], 0);
    }
}

TEST_F(StaticVectorTest, InitializerListConstruction) {
    StaticVector<int, SmallCapacity> vec{1, 2, 3, 4};
    EXPECT_EQ(vec.size(), 4);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 3);
    EXPECT_EQ(vec[3], 4);
}

TEST_F(StaticVectorTest, RangeConstruction) {
    std::vector<int> stdVec{5, 6, 7};
    StaticVector<int, SmallCapacity> vec(stdVec.begin(), stdVec.end());
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 5);
    EXPECT_EQ(vec[1], 6);
    EXPECT_EQ(vec[2], 7);
}

TEST_F(StaticVectorTest, CopyConstruction) {
    StaticVector<int, SmallCapacity> original{1, 2, 3};
    StaticVector<int, SmallCapacity> copy(original);
    EXPECT_EQ(copy.size(), 3);
    for (std::size_t i = 0; i < copy.size(); ++i) {
        EXPECT_EQ(copy[i], original[i]);
    }
}

TEST_F(StaticVectorTest, MoveConstruction) {
    StaticVector<int, SmallCapacity> original{1, 2, 3};
    StaticVector<int, SmallCapacity> moved(std::move(original));
    EXPECT_EQ(moved.size(), 3);
    EXPECT_EQ(moved[0], 1);
    EXPECT_EQ(moved[1], 2);
    EXPECT_EQ(moved[2], 3);
    EXPECT_EQ(original.size(), 0);  // After move, original should be empty
}

// Assignment tests
TEST_F(StaticVectorTest, CopyAssignment) {
    StaticVector<int, SmallCapacity> vec1{1, 2, 3};
    StaticVector<int, SmallCapacity> vec2;
    vec2 = vec1;
    EXPECT_EQ(vec2.size(), 3);
    for (std::size_t i = 0; i < vec2.size(); ++i) {
        EXPECT_EQ(vec2[i], vec1[i]);
    }
}

TEST_F(StaticVectorTest, MoveAssignment) {
    StaticVector<int, SmallCapacity> vec1{1, 2, 3};
    StaticVector<int, SmallCapacity> vec2;
    vec2 = std::move(vec1);
    EXPECT_EQ(vec2.size(), 3);
    EXPECT_EQ(vec2[0], 1);
    EXPECT_EQ(vec2[1], 2);
    EXPECT_EQ(vec2[2], 3);
    EXPECT_EQ(vec1.size(), 0);  // After move, vec1 should be empty
}

TEST_F(StaticVectorTest, InitializerListAssignment) {
    StaticVector<int, SmallCapacity> vec;
    vec = {10, 20, 30};
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 10);
    EXPECT_EQ(vec[1], 20);
    EXPECT_EQ(vec[2], 30);
}

// Element access tests
TEST_F(StaticVectorTest, SubscriptOperator) {
    StaticVector<int, SmallCapacity> vec{5, 10, 15};
    EXPECT_EQ(vec[0], 5);
    EXPECT_EQ(vec[1], 10);
    EXPECT_EQ(vec[2], 15);

    // Test const version
    const auto& constVec = vec;
    EXPECT_EQ(constVec[0], 5);
    EXPECT_EQ(constVec[1], 10);
    EXPECT_EQ(constVec[2], 15);

    // Modify through subscript
    vec[1] = 100;
    EXPECT_EQ(vec[1], 100);
}

TEST_F(StaticVectorTest, At) {
    StaticVector<int, SmallCapacity> vec{5, 10, 15};
    EXPECT_EQ(vec.at(0), 5);
    EXPECT_EQ(vec.at(1), 10);
    EXPECT_EQ(vec.at(2), 15);

    // Test const version
    const auto& constVec = vec;
    EXPECT_EQ(constVec.at(0), 5);
    EXPECT_EQ(constVec.at(1), 10);
    EXPECT_EQ(constVec.at(2), 15);

    // Modify through at
    vec.at(1) = 100;
    EXPECT_EQ(vec.at(1), 100);

    // Out of bounds
    EXPECT_THROW(vec.at(3), std::out_of_range);
    EXPECT_THROW(constVec.at(3), std::out_of_range);
}

TEST_F(StaticVectorTest, Front) {
    StaticVector<int, SmallCapacity> vec{5, 10, 15};
    EXPECT_EQ(vec.front(), 5);

    // Test const version
    const auto& constVec = vec;
    EXPECT_EQ(constVec.front(), 5);

    // Modify through front
    vec.front() = 100;
    EXPECT_EQ(vec.front(), 100);

    // Empty vector
    StaticVector<int, SmallCapacity> emptyVec;
    EXPECT_THROW(emptyVec.front(), std::underflow_error);

    const auto& constEmptyVec = emptyVec;
    EXPECT_THROW(constEmptyVec.front(), std::underflow_error);
}

TEST_F(StaticVectorTest, Back) {
    StaticVector<int, SmallCapacity> vec{5, 10, 15};
    EXPECT_EQ(vec.back(), 15);

    // Test const version
    const auto& constVec = vec;
    EXPECT_EQ(constVec.back(), 15);

    // Modify through back
    vec.back() = 100;
    EXPECT_EQ(vec.back(), 100);

    // Empty vector
    StaticVector<int, SmallCapacity> emptyVec;
    EXPECT_THROW(emptyVec.back(), std::underflow_error);

    const auto& constEmptyVec = emptyVec;
    EXPECT_THROW(constEmptyVec.back(), std::underflow_error);
}

TEST_F(StaticVectorTest, Data) {
    StaticVector<int, SmallCapacity> vec{5, 10, 15};
    auto* data = vec.data();
    EXPECT_EQ(data[0], 5);
    EXPECT_EQ(data[1], 10);
    EXPECT_EQ(data[2], 15);

    // Test const version
    const auto& constVec = vec;
    const auto* constData = constVec.data();
    EXPECT_EQ(constData[0], 5);
    EXPECT_EQ(constData[1], 10);
    EXPECT_EQ(constData[2], 15);

    // Modify through data
    data[1] = 100;
    EXPECT_EQ(vec[1], 100);
}

// Capacity tests
TEST_F(StaticVectorTest, Empty) {
    EXPECT_TRUE(emptyIntVector.empty());
    EXPECT_FALSE(smallIntVector.empty());

    emptyIntVector.pushBack(1);
    EXPECT_FALSE(emptyIntVector.empty());

    emptyIntVector.clear();
    EXPECT_TRUE(emptyIntVector.empty());
}

TEST_F(StaticVectorTest, Size) {
    EXPECT_EQ(emptyIntVector.size(), 0);
    EXPECT_EQ(smallIntVector.size(), 3);

    emptyIntVector.pushBack(1);
    EXPECT_EQ(emptyIntVector.size(), 1);

    smallIntVector.pushBack(4);
    EXPECT_EQ(smallIntVector.size(), 4);

    smallIntVector.popBack();
    EXPECT_EQ(smallIntVector.size(), 3);

    smallIntVector.clear();
    EXPECT_EQ(smallIntVector.size(), 0);
}

TEST_F(StaticVectorTest, Capacity) {
    EXPECT_EQ(emptyIntVector.capacity(), SmallCapacity);
    EXPECT_EQ(smallIntVector.capacity(), SmallCapacity);
    EXPECT_EQ(sequentialIntVector.capacity(), MediumCapacity);
}

TEST_F(StaticVectorTest, MaxSize) {
    EXPECT_EQ(emptyIntVector.max_size(), SmallCapacity);
    EXPECT_EQ(smallIntVector.max_size(), SmallCapacity);
    EXPECT_EQ(sequentialIntVector.max_size(), MediumCapacity);
}

TEST_F(StaticVectorTest, Reserve) {
    // Reserve within capacity
    StaticVector<int, SmallCapacity> vec;
    vec.reserve(SmallCapacity);
    EXPECT_EQ(vec.capacity(), SmallCapacity);

    // Reserve beyond capacity should throw
    EXPECT_THROW(vec.reserve(SmallCapacity + 1), std::overflow_error);
}

TEST_F(StaticVectorTest, ShrinkToFit) {
    // This is a no-op for StaticVector
    StaticVector<int, SmallCapacity> vec{1, 2, 3};
    std::size_t sizeBefore = vec.size();
    std::size_t capacityBefore = vec.capacity();

    vec.shrink_to_fit();

    EXPECT_EQ(vec.size(), sizeBefore);
    EXPECT_EQ(vec.capacity(), capacityBefore);
}

// Modifiers tests
TEST_F(StaticVectorTest, Clear) {
    StaticVector<int, SmallCapacity> vec{1, 2, 3};
    EXPECT_FALSE(vec.empty());

    vec.clear();
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.size(), 0);
}

TEST_F(StaticVectorTest, PushBack) {
    StaticVector<int, SmallCapacity> vec;

    vec.pushBack(10);
    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(vec.back(), 10);

    vec.pushBack(20);
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec.back(), 20);

    // Test capacity limit
    vec.pushBack(30);
    vec.pushBack(40);
    vec.pushBack(50);
    EXPECT_EQ(vec.size(), 5);
    EXPECT_THROW(vec.pushBack(60), std::overflow_error);

    // Test pushing by rvalue
    int val = 25;
    StaticVector<int, SmallCapacity> vec2;
    vec2.pushBack(std::move(val));
    EXPECT_EQ(vec2.size(), 1);
    EXPECT_EQ(vec2.back(), 25);
}

TEST_F(StaticVectorTest, EmplaceBack) {
    StaticVector<std::string, SmallCapacity> vec;

    auto& ref1 = vec.emplaceBack("hello");
    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(ref1, "hello");
    EXPECT_EQ(vec.back(), "hello");

    auto& ref2 = vec.emplaceBack(5, 'a');  // Constructs std::string(5, 'a')
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(ref2, "aaaaa");
    EXPECT_EQ(vec.back(), "aaaaa");

    // Test capacity limit
    vec.emplaceBack("1");
    vec.emplaceBack("2");
    vec.emplaceBack("3");
    EXPECT_EQ(vec.size(), 5);
    EXPECT_THROW(vec.emplaceBack("overflow"), std::overflow_error);
}

TEST_F(StaticVectorTest, PopBack) {
    StaticVector<int, SmallCapacity> vec{10, 20, 30};

    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec.back(), 30);

    vec.popBack();
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec.back(), 20);

    vec.popBack();
    vec.popBack();
    EXPECT_EQ(vec.size(), 0);

    // Pop from empty vector
    EXPECT_THROW(vec.popBack(), std::underflow_error);
}

TEST_F(StaticVectorTest, Insert) {
    StaticVector<int, SmallCapacity> vec{10, 30};

    // Insert single element
    auto it = vec.insert(vec.begin() + 1, 20);
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(*it, 20);
    EXPECT_EQ(vec[0], 10);
    EXPECT_EQ(vec[1], 20);
    EXPECT_EQ(vec[2], 30);

    // Insert element at the beginning
    it = vec.insert(vec.begin(), 5);
    EXPECT_EQ(vec.size(), 4);
    EXPECT_EQ(*it, 5);
    EXPECT_EQ(vec[0], 5);

    // Insert element at the end
    it = vec.insert(vec.end(), 40);
    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(*it, 40);
    EXPECT_EQ(vec[4], 40);

    // Insert should fail when full
    EXPECT_THROW(vec.insert(vec.begin(), 0), std::overflow_error);

    // Test insert with rvalue
    StaticVector<int, SmallCapacity> vec2{1, 3};
    int val = 2;
    it = vec2.insert(vec2.begin() + 1, std::move(val));
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(vec2[1], 2);
}

TEST_F(StaticVectorTest, InsertN) {
    StaticVector<int, SmallCapacity> vec{10, 40};

    // Insert multiple copies
    auto it = vec.insert(vec.begin() + 1, 2, 20);
    EXPECT_EQ(vec.size(), 4);
    EXPECT_EQ(*it, 20);
    EXPECT_EQ(vec[0], 10);
    EXPECT_EQ(vec[1], 20);
    EXPECT_EQ(vec[2], 20);
    EXPECT_EQ(vec[3], 40);

    // Insert zero elements (no-op)
    it = vec.insert(vec.begin(), 0, 5);
    EXPECT_EQ(it, vec.begin());
    EXPECT_EQ(vec.size(), 4);

    // Insert should fail when capacity would be exceeded
    EXPECT_THROW(vec.insert(vec.begin(), 2, 50), std::overflow_error);
}

TEST_F(StaticVectorTest, InsertRange) {
    StaticVector<int, SmallCapacity> vec{10, 40};
    std::vector<int> rangeVec{20, 30};

    // Insert range
    auto it = vec.insert(vec.begin() + 1, rangeVec.begin(), rangeVec.end());
    EXPECT_EQ(vec.size(), 4);
    EXPECT_EQ(*it, 20);
    EXPECT_EQ(vec[0], 10);
    EXPECT_EQ(vec[1], 20);
    EXPECT_EQ(vec[2], 30);
    EXPECT_EQ(vec[3], 40);

    // Insert empty range (no-op)
    std::vector<int> emptyVec;
    it = vec.insert(vec.begin(), emptyVec.begin(), emptyVec.end());
    EXPECT_EQ(it, vec.begin());
    EXPECT_EQ(vec.size(), 4);

    // Insert should fail when capacity would be exceeded
    std::vector<int> largeVec{1, 2, 3, 4, 5};
    EXPECT_THROW(vec.insert(vec.begin(), largeVec.begin(), largeVec.end()),
                 std::overflow_error);
}

TEST_F(StaticVectorTest, InsertInitializerList) {
    StaticVector<int, SmallCapacity> vec{10, 40};

    // Insert initializer list
    auto it = vec.insert(vec.begin() + 1, {20, 30});
    EXPECT_EQ(vec.size(), 4);
    EXPECT_EQ(*it, 20);
    EXPECT_EQ(vec[0], 10);
    EXPECT_EQ(vec[1], 20);
    EXPECT_EQ(vec[2], 30);
    EXPECT_EQ(vec[3], 40);

    // Insert should fail when capacity would be exceeded
    EXPECT_THROW(vec.insert(vec.begin(), {1, 2, 3, 4, 5}), std::overflow_error);
}

TEST_F(StaticVectorTest, Emplace) {
    StaticVector<std::string, SmallCapacity> vec{"hello", "world"};

    // Emplace in the middle
    auto it = vec.emplace(vec.begin() + 1, "beautiful");
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(*it, "beautiful");
    EXPECT_EQ(vec[0], "hello");
    EXPECT_EQ(vec[1], "beautiful");
    EXPECT_EQ(vec[2], "world");

    // Emplace at the beginning
    it = vec.emplace(vec.begin(), "say");
    EXPECT_EQ(vec.size(), 4);
    EXPECT_EQ(*it, "say");
    EXPECT_EQ(vec[0], "say");

    // Emplace at the end
    it = vec.emplace(vec.end(), "!");
    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(*it, "!");
    EXPECT_EQ(vec[4], "!");

    // Emplace should fail when full
    EXPECT_THROW(vec.emplace(vec.begin(), "overflow"), std::overflow_error);
}

TEST_F(StaticVectorTest, Erase) {
    StaticVector<int, SmallCapacity> vec{10, 20, 30, 40, 50};

    // Erase middle element
    auto it = vec.erase(vec.begin() + 2);
    EXPECT_EQ(vec.size(), 4);
    EXPECT_EQ(*it, 40);
    EXPECT_EQ(vec[0], 10);
    EXPECT_EQ(vec[1], 20);
    EXPECT_EQ(vec[2], 40);
    EXPECT_EQ(vec[3], 50);

    // Erase first element
    it = vec.erase(vec.begin());
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(*it, 20);
    EXPECT_EQ(vec[0], 20);

    // Erase last element
    it = vec.erase(vec.end() - 1);
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(it, vec.end());
    EXPECT_EQ(vec[0], 20);
    EXPECT_EQ(vec[1], 40);

    // Erase out of bounds
    EXPECT_THROW(vec.erase(vec.end()), std::out_of_range);
    EXPECT_THROW(vec.erase(vec.begin() - 1), std::out_of_range);
}

TEST_F(StaticVectorTest, EraseRange) {
    StaticVector<int, SmallCapacity> vec{10, 20, 30, 40, 50};

    // Erase range in the middle
    auto it = vec.erase(vec.begin() + 1, vec.begin() + 4);
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(*it, 50);
    EXPECT_EQ(vec[0], 10);
    EXPECT_EQ(vec[1], 50);

    // Erase empty range (no-op)
    it = vec.erase(vec.begin(), vec.begin());
    EXPECT_EQ(it, vec.begin());
    EXPECT_EQ(vec.size(), 2);

    // Erase all elements
    it = vec.erase(vec.begin(), vec.end());
    EXPECT_EQ(it, vec.end());
    EXPECT_TRUE(vec.empty());

    // Erase invalid range
    vec = {10, 20, 30};
    EXPECT_THROW(vec.erase(vec.begin() + 2, vec.begin()), std::out_of_range);
    EXPECT_THROW(vec.erase(vec.begin() - 1, vec.begin() + 1),
                 std::out_of_range);
    EXPECT_THROW(vec.erase(vec.begin(), vec.end() + 1), std::out_of_range);
}

TEST_F(StaticVectorTest, Resize) {
    StaticVector<int, SmallCapacity> vec{1, 2, 3};

    // Resize to larger size
    vec.resize(5);
    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 3);
    EXPECT_EQ(vec[3], 0);  // Default-initialized
    EXPECT_EQ(vec[4], 0);  // Default-initialized

    // Resize to smaller size
    vec.resize(2);
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);

    // Resize to same size (no-op)
    vec.resize(2);
    EXPECT_EQ(vec.size(), 2);

    // Resize to zero
    vec.resize(0);
    EXPECT_TRUE(vec.empty());

    // Resize beyond capacity
    EXPECT_THROW(vec.resize(SmallCapacity + 1), std::overflow_error);
}

TEST_F(StaticVectorTest, ResizeWithValue) {
    StaticVector<int, SmallCapacity> vec{1, 2, 3};

    // Resize to larger size with value
    vec.resize(5, 42);
    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 3);
    EXPECT_EQ(vec[3], 42);  // Value-initialized
    EXPECT_EQ(vec[4], 42);  // Value-initialized

    // Resize to smaller size
    vec.resize(2, 99);  // Value doesn't matter when shrinking
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);

    // Resize beyond capacity
    EXPECT_THROW(vec.resize(SmallCapacity + 1, 42), std::overflow_error);
}

TEST_F(StaticVectorTest, Swap) {
    StaticVector<int, SmallCapacity> vec1{1, 2, 3};
    StaticVector<int, SmallCapacity> vec2{4, 5};

    vec1.swap(vec2);

    EXPECT_EQ(vec1.size(), 2);
    EXPECT_EQ(vec1[0], 4);
    EXPECT_EQ(vec1[1], 5);

    EXPECT_EQ(vec2.size(), 3);
    EXPECT_EQ(vec2[0], 1);
    EXPECT_EQ(vec2[1], 2);
    EXPECT_EQ(vec2[2], 3);

    // Swap with self (no-op)
    vec1.swap(vec1);
    EXPECT_EQ(vec1.size(), 2);
    EXPECT_EQ(vec1[0], 4);
    EXPECT_EQ(vec1[1], 5);

    // Global swap function
    swap(vec1, vec2);
    EXPECT_EQ(vec1.size(), 3);
    EXPECT_EQ(vec1[0], 1);
    EXPECT_EQ(vec2.size(), 2);
    EXPECT_EQ(vec2[0], 4);
}

// Iterator tests
TEST_F(StaticVectorTest, Iterators) {
    StaticVector<int, SmallCapacity> vec{10, 20, 30};

    // begin/end
    auto it = vec.begin();
    EXPECT_EQ(*it, 10);
    ++it;
    EXPECT_EQ(*it, 20);
    ++it;
    EXPECT_EQ(*it, 30);
    ++it;
    EXPECT_EQ(it, vec.end());

    // rbegin/rend
    auto rit = vec.rbegin();
    EXPECT_EQ(*rit, 30);
    ++rit;
    EXPECT_EQ(*rit, 20);
    ++rit;
    EXPECT_EQ(*rit, 10);
    ++rit;
    EXPECT_EQ(rit, vec.rend());

    // const iterators
    const auto& constVec = vec;
    auto cit = constVec.begin();
    EXPECT_EQ(*cit, 10);
    EXPECT_EQ(*(cit + 1), 20);
    EXPECT_EQ(*(cit + 2), 30);

    auto crit = constVec.rbegin();
    EXPECT_EQ(*crit, 30);
    EXPECT_EQ(*(crit + 1), 20);
    EXPECT_EQ(*(crit + 2), 10);

    // cbegin/cend, crbegin/crend
    auto ccit = vec.cbegin();
    EXPECT_EQ(*ccit, 10);

    auto ccrit = vec.crbegin();
    EXPECT_EQ(*ccrit, 30);

    // Modify through iterator
    *vec.begin() = 100;
    EXPECT_EQ(vec[0], 100);

    *(vec.rbegin()) = 300;
    EXPECT_EQ(vec[2], 300);
}

// Comparison tests
TEST_F(StaticVectorTest, Comparisons) {
    StaticVector<int, SmallCapacity> vec1{1, 2, 3};
    StaticVector<int, SmallCapacity> vec2{1, 2, 3};
    StaticVector<int, SmallCapacity> vec3{1, 2, 4};
    StaticVector<int, SmallCapacity> vec4{1, 2};

    // Equality
    EXPECT_TRUE(vec1 == vec2);
    EXPECT_FALSE(vec1 == vec3);
    EXPECT_FALSE(vec1 == vec4);

    // Three-way comparison
    EXPECT_TRUE(vec1 < vec3);
    EXPECT_FALSE(vec1 < vec2);
    EXPECT_FALSE(vec3 < vec1);

    EXPECT_TRUE(vec4 < vec1);  // Shorter vector is less
    EXPECT_FALSE(vec1 < vec4);
}

// Span tests
TEST_F(StaticVectorTest, AsSpan) {
    StaticVector<int, SmallCapacity> vec{1, 2, 3};

    auto span = vec.as_span();
    EXPECT_EQ(span.size(), 3);
    EXPECT_EQ(span[0], 1);
    EXPECT_EQ(span[1], 2);
    EXPECT_EQ(span[2], 3);

    // Modify through span
    span[1] = 20;
    EXPECT_EQ(vec[1], 20);

    // Const span
    const auto& constVec = vec;
    auto constSpan = constVec.as_span();
    EXPECT_EQ(constSpan.size(), 3);
    EXPECT_EQ(constSpan[0], 1);
    EXPECT_EQ(constSpan[1], 20);
    EXPECT_EQ(constSpan[2], 3);
}

// Assignment tests
TEST_F(StaticVectorTest, AssignFunction) {
    StaticVector<int, SmallCapacity> vec;

    // Assign from iterator range
    std::vector<int> stdVec{5, 6, 7};
    vec.assign(stdVec.begin(), stdVec.end());
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], 5);
    EXPECT_EQ(vec[1], 6);
    EXPECT_EQ(vec[2], 7);

    // Assign from container
    std::vector<int> stdVec2{10, 20};
    vec.assign(stdVec2);
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0], 10);
    EXPECT_EQ(vec[1], 20);

    // Assign with count and value
    vec.assign(4, 42);
    EXPECT_EQ(vec.size(), 4);
    for (size_t i = 0; i < vec.size(); ++i) {
        EXPECT_EQ(vec[i], 42);
    }

    // Assign beyond capacity
    std::vector<int> tooLargeVec(SmallCapacity + 1, 1);
    EXPECT_THROW(vec.assign(tooLargeVec), std::length_error);
    EXPECT_THROW(vec.assign(SmallCapacity + 1, 1), std::length_error);
}

// Error handling tests
TEST_F(StaticVectorTest, CapacityErrors) {
    StaticVector<int, 2> vec;

    vec.pushBack(1);
    vec.pushBack(2);
    EXPECT_THROW(vec.pushBack(3), std::overflow_error);

    vec.clear();
    vec.pushBack(1);

    EXPECT_THROW(vec.insert(vec.begin(), 2, 10), std::overflow_error);

    std::vector<int> threeInts{1, 2, 3};
    EXPECT_THROW(vec.assign(threeInts), std::length_error);
}

// Special member function tests
TEST_F(StaticVectorTest, TransformElements) {
    StaticVector<int, SmallCapacity> vec{1, 2, 3, 4, 5};

    // Double each element
    vec.transform_elements([](int x) { return x * 2; });

    EXPECT_EQ(vec[0], 2);
    EXPECT_EQ(vec[1], 4);
    EXPECT_EQ(vec[2], 6);
    EXPECT_EQ(vec[3], 8);
    EXPECT_EQ(vec[4], 10);
}

TEST_F(StaticVectorTest, ParallelForEach) {
    StaticVector<int, MediumCapacity> vec;
    for (int i = 0; i < 20; ++i) {
        vec.pushBack(i);
    }

    // Sum of values
    int sum = 0;
    std::mutex mutex;

    vec.parallel_for_each([&sum, &mutex](int value) {
        std::lock_guard<std::mutex> lock(mutex);
        sum += value;
    });

    // Sum of 0 to 19 = 190
    EXPECT_EQ(sum, 190);
}

TEST_F(StaticVectorTest, SafeAddElements) {
    StaticVector<int, SmallCapacity> vec{1, 2};

    // Add elements safely
    std::vector<int> elements{3, 4, 5};
    bool success = vec.safeAddElements(std::span(elements));
    EXPECT_TRUE(success);
    EXPECT_EQ(vec.size(), 5);

    // Attempt to add beyond capacity
    elements = {6, 7};
    success = vec.safeAddElements(std::span(elements));
    EXPECT_FALSE(success);
    EXPECT_EQ(vec.size(), 5);  // Size should remain unchanged

    // Global helper function
    StaticVector<int, SmallCapacity> vec2{1};
    success = safeAddElements(vec2, std::span(elements));
    EXPECT_TRUE(success);
    EXPECT_EQ(vec2.size(), 3);

    // Try to add too many elements
    std::vector<int> manyElements{1, 2, 3, 4, 5};
    success = safeAddElements(vec2, std::span(manyElements));
    EXPECT_FALSE(success);
}

// Helper function tests
TEST_F(StaticVectorTest, SimdTransform) {
    StaticVector<int, SmallCapacity> vec1{1, 2, 3, 4};
    StaticVector<int, SmallCapacity> vec2{10, 20, 30, 40};
    StaticVector<int, SmallCapacity> result;

    // Apply SIMD transform (addition)
    bool success = simdTransform(vec1, vec2, result, std::plus<int>());
    EXPECT_TRUE(success);
    EXPECT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], 11);
    EXPECT_EQ(result[1], 22);
    EXPECT_EQ(result[2], 33);
    EXPECT_EQ(result[3], 44);

    // Mismatched sizes
    StaticVector<int, SmallCapacity> vec3{1, 2};
    success = simdTransform(vec1, vec3, result, std::plus<int>());
    EXPECT_FALSE(success);
}

TEST_F(StaticVectorTest, MakeStaticVector) {
    std::vector<int> stdVec{1, 2, 3, 4, 5};

    auto vec = makeStaticVector<int, SmallCapacity>(stdVec);
    EXPECT_EQ(vec.size(), 5);
    for (size_t i = 0; i < vec.size(); ++i) {
        EXPECT_EQ(vec[i], stdVec[i]);
    }

    // Too large
    std::vector<int> largeVec(SmallCapacity + 1, 1);
    EXPECT_THROW(makeStaticVector<int, SmallCapacity>(largeVec),
                 std::length_error);
}

// Smart pointer wrapper tests
TEST_F(StaticVectorTest, SmartStaticVector) {
    // Basic usage
    SmartStaticVector<int, SmallCapacity> smartVec;
    smartVec->pushBack(1);
    smartVec->pushBack(2);

    EXPECT_EQ(smartVec->size(), 2);
    EXPECT_EQ(smartVec->at(0), 1);
    EXPECT_EQ(smartVec->at(1), 2);

    // Reference access
    auto& vec = smartVec.get();
    vec.pushBack(3);
    EXPECT_EQ(smartVec->size(), 3);

    // Sharing
    auto sharedVec = smartVec;
    EXPECT_TRUE(smartVec.isShared());
    EXPECT_TRUE(sharedVec.isShared());

    // Make unique copy
    sharedVec.makeUnique();
    EXPECT_FALSE(smartVec.isShared());
    EXPECT_FALSE(sharedVec.isShared());

    // Modify copy without affecting original
    sharedVec->pushBack(4);
    EXPECT_EQ(sharedVec->size(), 4);
    EXPECT_EQ(smartVec->size(), 3);
}

// Thread safety test - make sure parallel execution works
TEST_F(StaticVectorTest, ThreadSafety) {
    const int numThreads = 10;
    StaticVector<int, LargeCapacity> vec;

    // Add some initial elements
    for (int i = 0; i < 500; ++i) {
        vec.pushBack(i);
    }

    std::vector<std::thread> threads;
    std::mutex mutex;
    int sum = 0;

    // Multiple threads read from the vector
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&vec, &sum, &mutex]() {
            int localSum = 0;
            for (auto val : vec) {
                localSum += val;
            }
            std::lock_guard<std::mutex> lock(mutex);
            sum += localSum;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Expected sum: sum of 0 to 499, 10 times
    // Sum of 0 to 499 = 499*500/2 = 124750
    EXPECT_EQ(sum, 124750 * numThreads);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
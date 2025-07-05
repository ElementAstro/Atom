#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "small_list.hpp"

using namespace atom::type;
using ::testing::ElementsAre;

class SmallListTest : public ::testing::Test {
protected:
    SmallList<int> list;
    const int TEST_SIZE = 1000;
};

// Basic Construction Tests
TEST_F(SmallListTest, DefaultConstructor) {
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0);
}

TEST_F(SmallListTest, InitializerListConstructor) {
    SmallList<int> list = {1, 2, 3, 4, 5};
    EXPECT_EQ(list.size(), 5);
    EXPECT_EQ(list.front(), 1);
    EXPECT_EQ(list.back(), 5);
}

TEST_F(SmallListTest, CopyConstructor) {
    list.pushBack(1);
    list.pushBack(2);
    list.pushBack(3);

    SmallList<int> copy(list);
    EXPECT_EQ(copy.size(), list.size());
    EXPECT_TRUE(std::equal(copy.begin(), copy.end(), list.begin()));
}

TEST_F(SmallListTest, MoveConstructor) {
    list.pushBack(1);
    list.pushBack(2);
    size_t originalSize = list.size();

    SmallList<int> moved(std::move(list));
    EXPECT_EQ(moved.size(), originalSize);
    EXPECT_TRUE(list.empty());
}

// Push/Pop Operations Tests
TEST_F(SmallListTest, PushBackAndFront) {
    list.pushBack(1);
    list.pushFront(2);
    EXPECT_EQ(list.front(), 2);
    EXPECT_EQ(list.back(), 1);
}

TEST_F(SmallListTest, PopBackAndFront) {
    list.pushBack(1);
    list.pushBack(2);

    list.popFront();
    EXPECT_EQ(list.front(), 2);

    list.popBack();
    EXPECT_TRUE(list.empty());
}

TEST_F(SmallListTest, EmplaceOperations) {
    list.emplaceBack(1);
    list.emplaceFront(2);
    auto it = list.begin();
    ++it;
    list.emplace(it, 3);

    std::vector<int> expected = {2, 3, 1};
    EXPECT_TRUE(std::equal(list.begin(), list.end(), expected.begin()));
}

// Iterator Tests
TEST_F(SmallListTest, IteratorOperations) {
    for(int i = 0; i < 5; ++i) {
        list.pushBack(i);
    }

    auto it = list.begin();
    EXPECT_EQ(*it, 0);
    ++it;
    EXPECT_EQ(*it, 1);
    --it;
    EXPECT_EQ(*it, 0);
}

TEST_F(SmallListTest, ConstIterator) {
    list.pushBack(1);
    list.pushBack(2);

    const SmallList<int>& constList = list;
    auto it = constList.begin();
    EXPECT_EQ(*it, 1);
}

TEST_F(SmallListTest, ReverseIterator) {
    list = {1, 2, 3, 4, 5};
    std::vector<int> reversed;
    for(auto it = list.rbegin(); it != list.rend(); ++it) {
        reversed.push_back(*it);
    }
    EXPECT_THAT(reversed, ElementsAre(5, 4, 3, 2, 1));
}

// Modification Tests
TEST_F(SmallListTest, InsertAndErase) {
    list = {1, 2, 4, 5};
    auto it = list.begin();
    ++it;
    ++it;
    list.insert(it, 3);
    EXPECT_THAT(list, ElementsAre(1, 2, 3, 4, 5));

    it = list.begin();
    ++it;
    list.erase(it);
    EXPECT_THAT(list, ElementsAre(1, 3, 4, 5));
}

TEST_F(SmallListTest, Clear) {
    list = {1, 2, 3};
    list.clear();
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.size(), 0);
}

TEST_F(SmallListTest, Resize) {
    list = {1, 2, 3};
    list.resize(5, 0);
    EXPECT_EQ(list.size(), 5);
    EXPECT_EQ(list.back(), 0);

    list.resize(2);
    EXPECT_EQ(list.size(), 2);
    EXPECT_EQ(list.back(), 2);
}

TEST_F(SmallListTest, Sort) {
    list = {5, 3, 1, 4, 2};
    list.sort();
    EXPECT_TRUE(std::is_sorted(list.begin(), list.end()));
}

TEST_F(SmallListTest, CustomSort) {
    list = {1, 2, 3, 4, 5};
    list.sort(std::greater<int>());
    EXPECT_TRUE(std::is_sorted(list.begin(), list.end(), std::greater<int>()));
}

TEST_F(SmallListTest, Reverse) {
    list = {1, 2, 3, 4, 5};
    list.reverse();
    EXPECT_THAT(list, ElementsAre(5, 4, 3, 2, 1));
}

TEST_F(SmallListTest, Remove) {
    list = {1, 2, 2, 3, 2, 4};
    size_t removed = list.remove(2);
    EXPECT_EQ(removed, 3);
    EXPECT_THAT(list, ElementsAre(1, 3, 4));
}

TEST_F(SmallListTest, RemoveIf) {
    list = {1, 2, 3, 4, 5, 6};
    size_t removed = list.removeIf([](int n) { return n % 2 == 0; });
    EXPECT_EQ(removed, 3);
    EXPECT_THAT(list, ElementsAre(1, 3, 5));
}

TEST_F(SmallListTest, Unique) {
    list = {1, 1, 2, 2, 2, 3, 3, 1};
    size_t removed = list.unique();
    EXPECT_EQ(removed, 4);
    EXPECT_THAT(list, ElementsAre(1, 2, 3, 1));
}

// Edge Cases Tests
TEST_F(SmallListTest, EmptyListOperations) {
    EXPECT_THROW(list.front(), std::out_of_range);
    EXPECT_THROW(list.back(), std::out_of_range);
    EXPECT_THROW(list.popFront(), std::out_of_range);
    EXPECT_THROW(list.popBack(), std::out_of_range);
}

TEST_F(SmallListTest, SingleElementOperations) {
    list.pushBack(1);
    list.sort();
    list.reverse();
    EXPECT_EQ(list.size(), 1);
    EXPECT_EQ(list.front(), 1);
}

TEST_F(SmallListTest, LargeListOperations) {
    // Test with a large number of elements
    for(int i = 0; i < TEST_SIZE; ++i) {
        list.pushBack(i);
    }

    list.sort();
    EXPECT_TRUE(std::is_sorted(list.begin(), list.end()));
    EXPECT_EQ(list.size(), TEST_SIZE);
}

// Define this outside the test function
struct ThrowingCopy {
    int value;
    static bool shouldThrow;

    ThrowingCopy(int v) : value(v) {}
    ThrowingCopy(const ThrowingCopy& other) {
        if(shouldThrow) throw std::runtime_error("Copy error");
        value = other.value;
    }
};

bool ThrowingCopy::shouldThrow = false;

// Exception Safety Tests
TEST_F(SmallListTest, ExceptionSafety) {
    ThrowingCopy::shouldThrow = false;
    SmallList<ThrowingCopy> throwingList;
    throwingList.pushBack(ThrowingCopy(1));

    ThrowingCopy::shouldThrow = true;
    EXPECT_THROW(throwingList.pushBack(ThrowingCopy(2)), std::runtime_error);
    EXPECT_EQ(throwingList.size(), 1); // List should remain unchanged
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

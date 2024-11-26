#ifndef ATOM_MEMORY_TEST_UTILS_HPP
#define ATOM_MEMORY_TEST_UTILS_HPP

#include <gtest/gtest.h>

#include "atom/memory/utils.hpp"

using namespace atom::memory;

struct TestStruct {
    int a;
    double b;
    TestStruct(int x, double y) : a(x), b(y) {}
};

struct NoConstructorStruct {};

TEST(MemoryUtilsTest, MakeSharedValid) {
    auto ptr = makeShared<TestStruct>(10, 20.5);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->a, 10);
    EXPECT_EQ(ptr->b, 20.5);
}

TEST(MemoryUtilsTest, MakeSharedInvalid) {
    // This should fail to compile due to static_assert
    // auto ptr = makeShared<TestStruct>("invalid", 20.5);
}

TEST(MemoryUtilsTest, MakeUniqueValid) {
    auto ptr = makeUnique<TestStruct>(10, 20.5);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->a, 10);
    EXPECT_EQ(ptr->b, 20.5);
}

TEST(MemoryUtilsTest, MakeUniqueInvalid) {
    // This should fail to compile due to static_assert
    // auto ptr = makeUnique<TestStruct>("invalid", 20.5);
}

TEST(MemoryUtilsTest, MakeSharedNoConstructor) {
    // This should fail to compile due to static_assert
    // auto ptr = makeShared<NoConstructorStruct>();
}

TEST(MemoryUtilsTest, MakeUniqueNoConstructor) {
    // This should fail to compile due to static_assert
    // auto ptr = makeUnique<NoConstructorStruct>();
}

#endif // ATOM_MEMORY_TEST_UTILS_HPP

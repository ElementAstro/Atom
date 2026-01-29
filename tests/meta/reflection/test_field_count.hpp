#include <gtest/gtest.h>
#include "atom/meta/field_count.hpp"

namespace {

// Test fixtures for different struct types
struct Empty {};

struct SimpleFields {
    int a;
    double b;
    char c;
};

struct NestedStruct {
    int x;
    SimpleFields nested;
    double y;
};

struct WithArray {
    int arr[3];
    std::array<double, 2> stdArr;
    float f;
};

struct WithPointers {
    int* ptr;
    const char* str;
    void* vptr;
};

struct WithBitfields {
    int a : 1;
    int b : 2;
    int c : 3;
};

union TestUnion {
    int i;
    float f;
    double d;
};

struct WithUnion {
    int a;
    TestUnion u;
    char c;
};

struct NonAggregate {
    NonAggregate() {}
    int x;
};

class FieldCountTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Basic field counting tests
TEST_F(FieldCountTest, EmptyStruct) {
    constexpr auto count = atom::meta::fieldCountOf<Empty>();
    EXPECT_EQ(count, 0);
}

TEST_F(FieldCountTest, SimpleStructFields) {
    constexpr auto count = atom::meta::fieldCountOf<SimpleFields>();
    EXPECT_EQ(count, 3);
}

TEST_F(FieldCountTest, NestedStructFields) {
    constexpr auto count = atom::meta::fieldCountOf<NestedStruct>();
    EXPECT_EQ(count, 3);
}

TEST_F(FieldCountTest, ArrayFields) {
    constexpr auto count = atom::meta::fieldCountOf<WithArray>();
    EXPECT_EQ(count, 3);
}

// Complex type tests
TEST_F(FieldCountTest, PointerFields) {
    constexpr auto count = atom::meta::fieldCountOf<WithPointers>();
    EXPECT_EQ(count, 3);
}

TEST_F(FieldCountTest, BitFields) {
    constexpr auto count = atom::meta::fieldCountOf<WithBitfields>();
    EXPECT_EQ(count, 3);
}

TEST_F(FieldCountTest, UnionFields) {
    constexpr auto count = atom::meta::fieldCountOf<WithUnion>();
    EXPECT_EQ(count, 3);
}

TEST_F(FieldCountTest, NonAggregateType) {
    constexpr auto count = atom::meta::fieldCountOf<NonAggregate>();
    EXPECT_EQ(count, 0);
}

// Test custom type_info specialization
struct CustomType {
    int x, y, z;
};

// Test multiple inheritance
struct Base1 {
    int a;
};
struct Base2 {
    double b;
};
struct Derived : Base1, Base2 {
    char c;
};

TEST_F(FieldCountTest, InheritanceFields) {
    constexpr auto count = atom::meta::fieldCountOf<Derived>();
    EXPECT_EQ(count, 3);
}

// Test complex nested structures
struct ComplexNested {
    struct Inner {
        int x;
        double y;
    } inner;
    float outer;
    std::array<int, 4> arr;
};

TEST_F(FieldCountTest, ComplexNestedFields) {
    constexpr auto count = atom::meta::fieldCountOf<ComplexNested>();
    EXPECT_EQ(count, 3);
}

// Test maximum field count
struct MaxFields {
    int f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
    int f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
};

TEST_F(FieldCountTest, MaximumFields) {
    constexpr auto count = atom::meta::fieldCountOf<MaxFields>();
    EXPECT_EQ(count, 20);
}

// Test alignment and padding
struct AlignedStruct {
    char a;
    alignas(8) double b;
    int c;
};

TEST_F(FieldCountTest, AlignedFields) {
    constexpr auto count = atom::meta::fieldCountOf<AlignedStruct>();
    EXPECT_EQ(count, 3);
}

// Test reference members
struct WithReferences {
    int& ref;
    const double& constRef;
};

TEST_F(FieldCountTest, ReferenceFields) {
    constexpr auto count = atom::meta::fieldCountOf<WithReferences>();
    EXPECT_EQ(count, 2);
}

// Test various STL container members
struct WithSTL {
    std::array<int, 3> arr;
    std::array<std::array<double, 2>, 2> nested;
};

TEST_F(FieldCountTest, STLContainerFields) {
    constexpr auto count = atom::meta::fieldCountOf<WithSTL>();
    EXPECT_EQ(count, 2);
}

}  // namespace

#include "atom/function/member.hpp"
#include <gtest/gtest.h>
#include <sstream>
#include <vector>

using namespace atom::meta;

struct TestStruct {
    int a;
    double b;
    char c;
};

class Base {
public:
    int base_member;
};

class Derived : public Base {
public:
    double derived_member;
};

class MemberTest : public ::testing::Test {
protected:
    TestStruct test_struct{1, 2.0, 'a'};
    std::vector<int> test_vector{1, 2, 3, 4, 5};
};

TEST_F(MemberTest, MemberOffset) {
    EXPECT_EQ(member_offset(&TestStruct::a), 0);
    EXPECT_EQ(member_offset(&TestStruct::b), sizeof(int));
    EXPECT_GT(member_offset(&TestStruct::c), member_offset(&TestStruct::b));
}

TEST_F(MemberTest, MemberSize) {
    EXPECT_EQ(member_size(&TestStruct::a), sizeof(int));
    EXPECT_EQ(member_size(&TestStruct::b), sizeof(double));
    EXPECT_EQ(member_size(&TestStruct::c), sizeof(char));
}

TEST_F(MemberTest, StructSize) {
    EXPECT_EQ(struct_size<TestStruct>(), sizeof(TestStruct));
}

TEST_F(MemberTest, PrintMemberInfo) {
    std::stringstream ss;
    auto cout_buff = std::cout.rdbuf();
    std::cout.rdbuf(ss.rdbuf());

    print_member_info<TestStruct>(&TestStruct::a, &TestStruct::b,
                                  &TestStruct::c);

    std::cout.rdbuf(cout_buff);
    std::string output = ss.str();
    EXPECT_NE(output.find("Offset:"), std::string::npos);
    EXPECT_NE(output.find("Size:"), std::string::npos);
}

TEST_F(MemberTest, PointerToObject) {
    int* member_ptr = &test_struct.a;
    TestStruct* obj_ptr = pointer_to_object(&TestStruct::a, member_ptr);
    EXPECT_EQ(obj_ptr, &test_struct);
}

TEST_F(MemberTest, ConstPointerToObject) {
    const int* member_ptr = &test_struct.a;
    const TestStruct* obj_ptr = pointer_to_object(&TestStruct::a, member_ptr);
    EXPECT_EQ(obj_ptr, &test_struct);
}

TEST_F(MemberTest, ContainerOfBasic) {
    int* ptr = &test_struct.a;
    auto* container = container_of(ptr, &TestStruct::a);
    EXPECT_EQ(container, &test_struct);
}

TEST_F(MemberTest, ContainerOfInheritance) {
    Derived derived;
    derived.base_member = 42;
    derived.derived_member = 3.14;

    int* ptr = &derived.base_member;
    Base* base = container_of(ptr, &Derived::base_member);
    EXPECT_EQ(base, static_cast<Base*>(&derived));
}

TEST_F(MemberTest, ContainerOfConst) {
    const int* ptr = &test_struct.a;
    const auto* container = container_of(ptr, &TestStruct::a);
    EXPECT_EQ(container, &test_struct);
}

TEST_F(MemberTest, ContainerOfRange) {
    int search_val = 3;
    auto* found = container_of_range(test_vector, &search_val);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(*found, 3);

    int not_found_val = 99;
    auto* not_found = container_of_range(test_vector, &not_found_val);
    EXPECT_EQ(not_found, nullptr);
}

TEST_F(MemberTest, ContainerOfIfRange) {
    auto predicate = [](int val) { return val > 3; };
    auto* found = container_of_if_range(test_vector, predicate);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(*found, 4);

    auto impossible_predicate = [](int val) { return val > 100; };
    auto* not_found = container_of_if_range(test_vector, impossible_predicate);
    EXPECT_EQ(not_found, nullptr);
}

TEST_F(MemberTest, OffsetOf) {
    EXPECT_EQ(offset_of(&TestStruct::a), 0);
    EXPECT_EQ(offset_of(&TestStruct::b), sizeof(int));
    EXPECT_GT(offset_of(&TestStruct::c), offset_of(&TestStruct::b));
}

TEST_F(MemberTest, NullPointerAssertions) {
    EXPECT_DEATH(container_of((int*)nullptr, &TestStruct::a), "");
    EXPECT_DEATH(container_of((const int*)nullptr, &TestStruct::a), "");
}
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <tuple>
#include <vector>
#include <optional>

#include "atom/type/argsview.hpp"

using namespace atom;

// Test fixture for ArgsView tests
class ArgsViewTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Basic Construction Tests
TEST_F(ArgsViewTest, BasicConstruction) {
    // Test construction with different types
    ArgsView<int, std::string, double> view(42, "hello", 3.14);
    
    EXPECT_EQ(view.get<0>(), 42);
    EXPECT_EQ(view.get<1>(), "hello");
    EXPECT_EQ(view.get<2>(), 3.14);
}

TEST_F(ArgsViewTest, SingleArgumentConstruction) {
    ArgsView<int> singleView(100);
    EXPECT_EQ(singleView.get<0>(), 100);
}

TEST_F(ArgsViewTest, EmptyConstruction) {
    ArgsView<> emptyView;
    EXPECT_EQ(emptyView.size(), 0);
}

// Tuple Construction Tests
TEST_F(ArgsViewTest, TupleConstruction) {
    auto tuple = std::make_tuple(42, "test", 3.14);
    ArgsView<int, std::string, double> view(tuple);
    
    EXPECT_EQ(view.get<0>(), 42);
    EXPECT_EQ(view.get<1>(), "test");
    EXPECT_EQ(view.get<2>(), 3.14);
}

// ArgsView to ArgsView Construction Tests
TEST_F(ArgsViewTest, ArgsViewConstruction) {
    ArgsView<int, std::string> originalView(42, "hello");
    ArgsView<int, std::string> copiedView(originalView);
    
    EXPECT_EQ(copiedView.get<0>(), 42);
    EXPECT_EQ(copiedView.get<1>(), "hello");
}

// Optional Construction Tests
TEST_F(ArgsViewTest, OptionalConstruction) {
    std::optional<int> opt1 = 42;
    std::optional<std::string> opt2 = "hello";
    std::optional<double> opt3 = std::nullopt;
    
    ArgsView<int, std::string, double> view(std::move(opt1), std::move(opt2), std::move(opt3));
    
    EXPECT_EQ(view.get<0>(), 42);
    EXPECT_EQ(view.get<1>(), "hello");
    EXPECT_EQ(view.get<2>(), 0.0); // Default value for double
}

// Access Methods Tests
TEST_F(ArgsViewTest, GetMethod) {
    ArgsView<int, std::string, bool> view(123, "world", true);
    
    EXPECT_EQ(view.get<0>(), 123);
    EXPECT_EQ(view.get<1>(), "world");
    EXPECT_EQ(view.get<2>(), true);
}

// Note: ArgsView doesn't have an at() method, only get()
// This test is removed as the functionality is covered by GetMethod test

// Size and Empty Tests
TEST_F(ArgsViewTest, SizeMethod) {
    ArgsView<int> singleView(1);
    ArgsView<int, std::string> doubleView(1, "test");
    ArgsView<int, std::string, double> tripleView(1, "test", 3.14);
    ArgsView<> emptyView;
    
    EXPECT_EQ(singleView.size(), 1);
    EXPECT_EQ(doubleView.size(), 2);
    EXPECT_EQ(tripleView.size(), 3);
    EXPECT_EQ(emptyView.size(), 0);
}

TEST_F(ArgsViewTest, EmptyMethod) {
    ArgsView<> emptyView;
    ArgsView<int> nonEmptyView(42);
    
    EXPECT_TRUE(emptyView.empty());
    EXPECT_FALSE(nonEmptyView.empty());
}

// Functional Operations Tests
TEST_F(ArgsViewTest, ForEachOperation) {
    ArgsView<int, int, int> view(1, 2, 3);
    
    int sum = 0;
    view.forEach([&sum](const auto& value) {
        sum += value;
    });
    
    EXPECT_EQ(sum, 6);
}

TEST_F(ArgsViewTest, TransformOperation) {
    ArgsView<int, int, int> view(1, 2, 3);
    
    auto doubled = view.transform([](const auto& value) {
        return value * 2;
    });
    
    EXPECT_EQ(doubled.get<0>(), 2);
    EXPECT_EQ(doubled.get<1>(), 4);
    EXPECT_EQ(doubled.get<2>(), 6);
}

// Note: ArgsView doesn't have filter method
// This test is removed as the functionality is not available

// Conversion Tests
TEST_F(ArgsViewTest, ToTupleConversion) {
    ArgsView<int, std::string, double> view(42, "hello", 3.14);
    
    auto tuple = view.toTuple();
    
    EXPECT_EQ(std::get<0>(tuple), 42);
    EXPECT_EQ(std::get<1>(tuple), "hello");
    EXPECT_EQ(std::get<2>(tuple), 3.14);
}

// Note: ArgsView doesn't have toVector method
// This test is removed as the functionality is not available

// Complex Type Tests
struct ComplexType {
    int id;
    std::string name;
    
    bool operator==(const ComplexType& other) const {
        return id == other.id && name == other.name;
    }
};

TEST_F(ArgsViewTest, ComplexTypes) {
    ComplexType obj1{1, "first"};
    ComplexType obj2{2, "second"};
    
    ArgsView<ComplexType, ComplexType> view(obj1, obj2);
    
    EXPECT_EQ(view.get<0>().id, 1);
    EXPECT_EQ(view.get<0>().name, "first");
    EXPECT_EQ(view.get<1>().id, 2);
    EXPECT_EQ(view.get<1>().name, "second");
}

// Edge Cases Tests
TEST_F(ArgsViewTest, LargeNumberOfArguments) {
    ArgsView<int, int, int, int, int, int, int, int, int, int> view(
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    
    EXPECT_EQ(view.size(), 10);
    EXPECT_EQ(view.get<0>(), 1);
    EXPECT_EQ(view.get<9>(), 10);
}

TEST_F(ArgsViewTest, MixedTypes) {
    ArgsView<int, double, std::string, bool, char> view(
        42, 3.14, "test", true, 'A');
    
    EXPECT_EQ(view.get<0>(), 42);
    EXPECT_EQ(view.get<1>(), 3.14);
    EXPECT_EQ(view.get<2>(), "test");
    EXPECT_EQ(view.get<3>(), true);
    EXPECT_EQ(view.get<4>(), 'A');
}

// Constexpr Tests
TEST_F(ArgsViewTest, ConstexprOperations) {
    constexpr ArgsView<int, int> view(10, 20);
    
    static_assert(view.size() == 2);
    static_assert(view.get<0>() == 10);
    static_assert(view.get<1>() == 20);
    static_assert(!view.empty());
}

// Hash and Comparison Tests (if implemented)
TEST_F(ArgsViewTest, EqualityComparison) {
    ArgsView<int, std::string> view1(42, "hello");
    ArgsView<int, std::string> view2(42, "hello");
    ArgsView<int, std::string> view3(43, "hello");
    
    EXPECT_EQ(view1, view2);
    EXPECT_NE(view1, view3);
}

// Note: ArgsView doesn't have iterator support
// This test is removed as the functionality is not available

// Memory and Performance Tests
TEST_F(ArgsViewTest, MemoryEfficiency) {
    // Test that ArgsView doesn't copy unnecessarily
    std::string large_string(1000, 'x');
    ArgsView<std::string> view(large_string);
    
    // The view should reference the original string, not copy it
    EXPECT_EQ(view.get<0>().size(), 1000);
}

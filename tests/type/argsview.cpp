#include <gtest/gtest.h>

#include "atom/type/argsview.hpp"

using namespace atom;

TEST(ArgsViewTest, Constructor) {
    ArgsView<int, double, std::string> args_view(1, 2.0, "test");
    EXPECT_EQ(args_view.size(), 3);
    EXPECT_EQ(args_view.get<0>(), 1);
    EXPECT_EQ(args_view.get<1>(), 2.0);
    EXPECT_EQ(args_view.get<2>(), "test");
}

TEST(ArgsViewTest, ConstructorFromTuple) {
    std::tuple<int, double, std::string> t(1, 2.0, "test");
    ArgsView<int, double, std::string> args_view(t);
    EXPECT_EQ(args_view.size(), 3);
    EXPECT_EQ(args_view.get<0>(), 1);
    EXPECT_EQ(args_view.get<1>(), 2.0);
    EXPECT_EQ(args_view.get<2>(), "test");
}

TEST(ArgsViewTest, ConstructorFromOptional) {
    std::optional<int> opt_int = 1;
    std::optional<double> opt_double = 2.0;
    std::optional<std::string> opt_string = "test";
    ArgsView<int, double, std::string> args_view(std::move(opt_int), std::move(opt_double), std::move(opt_string));
    EXPECT_EQ(args_view.size(), 3);
    EXPECT_EQ(args_view.get<0>(), 1);
    EXPECT_EQ(args_view.get<1>(), 2.0);
    EXPECT_EQ(args_view.get<2>(), "test");
}

TEST(ArgsViewTest, Get) {
    ArgsView<int, double, std::string> args_view(1, 2.0, "test");
    EXPECT_EQ(args_view.get<0>(), 1);
    EXPECT_EQ(args_view.get<1>(), 2.0);
    EXPECT_EQ(args_view.get<2>(), "test");
}

TEST(ArgsViewTest, Size) {
    ArgsView<int, double, std::string> args_view(1, 2.0, "test");
    EXPECT_EQ(args_view.size(), 3);
}

TEST(ArgsViewTest, Empty) {
    ArgsView<> args_view;
    EXPECT_TRUE(args_view.empty());
}

/*
TODO: Fix this test
TEST(ArgsViewTest, ForEach) {
    ArgsView<int, double, std::string> args_view(1, 2.0, "test");
    int count = 0;
    args_view.forEach([&count](const auto& arg) { count++; });
    EXPECT_EQ(count, 3);
}
*/


TEST(ArgsViewTest, Transform) {
    ArgsView<int, double> args_view(1, 2.0);
    auto transformed = args_view.transform([](const auto& arg) { return arg + 1; });
    EXPECT_EQ(transformed.get<0>(), 2);
    EXPECT_EQ(transformed.get<1>(), 3.0);
}

TEST(ArgsViewTest, Accumulate) {
    ArgsView<int, int, int> args_view(1, 2, 3);
    int sum = args_view.accumulate([](int a, int b) { return a + b; }, 0);
    EXPECT_EQ(sum, 6);
}

TEST(ArgsViewTest, Apply) {
    ArgsView<int, double> args_view(1, 2.0);
    auto result = args_view.apply([](int a, double b) { return a + b; });
    EXPECT_EQ(result, 3.0);
}

TEST(ArgsViewTest, OperatorEqual) {
    ArgsView<int, double> args_view1(1, 2.0);
    ArgsView<int, double> args_view2(1, 2.0);
    EXPECT_TRUE(args_view1 == args_view2);
}

TEST(ArgsViewTest, OperatorNotEqual) {
    ArgsView<int, double> args_view1(1, 2.0);
    ArgsView<int, double> args_view2(2, 3.0);
    EXPECT_TRUE(args_view1 != args_view2);
}

TEST(ArgsViewTest, OperatorLessThan) {
    ArgsView<int, double> args_view1(1, 2.0);
    ArgsView<int, double> args_view2(2, 3.0);
    EXPECT_TRUE(args_view1 < args_view2);
}

TEST(ArgsViewTest, OperatorLessThanOrEqual) {
    ArgsView<int, double> args_view1(1, 2.0);
    ArgsView<int, double> args_view2(1, 2.0);
    EXPECT_TRUE(args_view1 <= args_view2);
}

TEST(ArgsViewTest, OperatorGreaterThan) {
    ArgsView<int, double> args_view1(2, 3.0);
    ArgsView<int, double> args_view2(1, 2.0);
    EXPECT_TRUE(args_view1 > args_view2);
}

TEST(ArgsViewTest, OperatorGreaterThanOrEqual) {
    ArgsView<int, double> args_view1(2, 3.0);
    ArgsView<int, double> args_view2(1, 2.0);
    EXPECT_TRUE(args_view1 >= args_view2);
}

TEST(ArgsViewTest, Sum) {
    int result = sum(1, 2, 3);
    EXPECT_EQ(result, 6);
}

TEST(ArgsViewTest, Concat) {
    std::string result = concat("Hello", " ", "World", "!");
    EXPECT_EQ(result, "Hello World!");
}

TEST(ArgsViewTest, MakeArgsView) {
    auto args_view = makeArgsView(1, 2.0, "test");
    EXPECT_EQ(args_view.size(), 3);
    EXPECT_EQ(args_view.get<0>(), 1);
    EXPECT_EQ(args_view.get<1>(), 2.0);
    EXPECT_EQ(args_view.get<2>(), "test");
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
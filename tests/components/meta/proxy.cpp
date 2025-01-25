#include "atom/function/proxy.hpp"
#include <gtest/gtest.h>
#include <any>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

// 示例函数和类，用于测试
int add(int a, int b) { return a + b; }

void voidFunction(int& a, int b) { a += b; }

class TestClass {
public:
    int multiply(int a, int b) const { return a * b; }

    void setValue(int value) { value_ = value; }

    int getValue() const { return value_; }

private:
    int value_ = 0;
};

// 测试非成员函数代理
TEST(ProxyFunctionTest, NonMemberFunction) {
    atom::meta::ProxyFunction proxy(add);

    std::vector<std::any> args{2, 3};
    auto result = proxy(args);

    EXPECT_EQ(std::any_cast<int>(result), 5);
}

TEST(ProxyFunctionTest, VoidNonMemberFunction) {
    int a = 1;
    atom::meta::FunctionInfo info;
    atom::meta::ProxyFunction proxy(voidFunction, info);

    std::vector<std::any> args{std::ref(a), 4};
    auto result = proxy(args);

    EXPECT_FALSE(result.has_value());  // void函数返回空any
    EXPECT_EQ(a, 5);                   // 检查引用参数是否被修改
}

// 测试成员函数代理
TEST(ProxyFunctionTest, MemberFunction) {
    TestClass obj;
    atom::meta::ProxyFunction proxy(&TestClass::multiply);

    std::vector<std::any> args{std::ref(obj), 4, 5};
    auto result = proxy(args);

    EXPECT_EQ(std::any_cast<int>(result), 20);
}

// 测试void成员函数代理
TEST(ProxyFunctionTest, VoidMemberFunction) {
    TestClass obj;
    atom::meta::ProxyFunction proxy(&TestClass::setValue);

    std::vector<std::any> args{std::ref(obj), 42};
    auto result = proxy(args);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(obj.getValue(), 42);
}

// 测试参数数量不足的异常情况
TEST(ProxyFunctionTest, IncorrectNumberOfArguments) {
    atom::meta::ProxyFunction proxy(add);

    std::vector<std::any> args{2};  // 缺少一个参数
    EXPECT_THROW(proxy(args), atom::error::Exception);
}

// 测试成员函数参数数量不足的异常情况
TEST(ProxyFunctionTest, IncorrectNumberOfArgumentsMemberFunction) {
    TestClass obj;
    atom::meta::ProxyFunction proxy(&TestClass::multiply);

    std::vector<std::any> args{std::ref(obj), 4};  // 缺少一个参数
    EXPECT_THROW(proxy(args), atom::error::Exception);
}

// 测试返回类型不匹配的情况
TEST(ProxyFunctionTest, InvalidReturnType) {
    atom::meta::ProxyFunction proxy(add);

    std::vector<std::any> args{2, 3};
    auto result = proxy(args);

    // 尝试提取错误的返回类型
    EXPECT_THROW(std::any_cast<std::string>(result), std::bad_any_cast);
}
